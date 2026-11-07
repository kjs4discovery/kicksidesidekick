# fm1song: read a song folder (song.yaml + song.mid, format fm1-song/1) into a Song. Licence: TBD (see D017)
#
# docs/song-format.md is the specification; this module is its reference implementation. Every check
# records a Diagnostic with the file and, where it applies, bar:beat in the musician's numbering
# (bar 0 = the pickup bar, bar 1 = the first full bar).
import re
from pathlib import Path

import yaml

from . import chords as ch
from .errors import Diagnostics
from .smf import read_midi

FORMAT = "fm1-song/1"
LAYERS = ["drums", "bass", "harmony", "melody"]      # fixed order everywhere (compiled file, device buttons)
GEN_STYLES = ("waltz", "ballad", "shuffle", "march", "pop", "train")   # tools/fm1song/arrange.py STYLES, in order
GEN_LEVELS = {"harmony": 5, "bass": 4, "drums": 3}  # the generated levels of a layer (format 2.0)
MAX_VARIANTS = 5                  # M4 (format 1.3): up to 5 levels a layer; 1.2 and older: 3
MODES = ["major", "minor", "dorian", "mixolydian", "lydian", "phrygian", "locrian"]
KEEP_CC = {1: "modulation", 7: "volume", 10: "pan", 11: "expression", 64: "sustain", 91: "reverb", 93: "chorus"}
LIMITS = {"bars": 999, "sections": 32, "chords": 64, "chord_events": 2048, "events": 65535}
_SLUG = re.compile(r"^[a-z0-9][a-z0-9-]{0,22}$")
_TRACK = re.compile(r"^(drums|bass|harmony|melody)(?::([1-9]))?$")


class Section:
    def __init__(self, name, first_bar, bar_count, lead_ticks, chart):
        self.name, self.first_bar, self.bar_count, self.lead_ticks, self.chart = \
            name, first_bar, bar_count, lead_ticks, chart


class Layer:
    def __init__(self, name, channel, program, volume, pan):
        self.name, self.channel, self.program, self.volume, self.pan = name, channel, program, volume, pan
        self.variants = {}               # n -> [(tick, status, d1, d2)], status 0x80/0x90/0xB0/0xE0
        self.descriptions = {}
        self.levels = 0                  # 2.0: how many levels (generated or stored); 0: len(variants) (1.x)


class Song:
    """Everything the compiler needs, already checked. Bar *indices* count from the first bar in song.mid
    (index 0 = the pickup bar if there is one); bar *numbers* are what musicians read (bar_number())."""

    def bar_number(self, index):
        return index if self.pickup_beats else index + 1

    def bar_beat(self, tick):
        bar_ticks = self.ppq * self.beats_per_bar
        bar, rest = divmod(tick, bar_ticks)
        beat, sub = divmod(rest, self.ppq)
        return f"{self.bar_number(bar)}:{beat + 1}" + (f"+{sub}" if sub else "")


def _where_tick(song, tick):
    return song.bar_beat(tick) if song else str(tick)


# ------------------------------------------------------------------ song.yaml

def _get(diag, d, path, kind, required=True, default=None):
    """d['a']['b'] for path 'a.b', type-checked; records an error and returns default when wrong"""
    cur = d
    for part in path.split("."):
        if not isinstance(cur, dict) or part not in cur:
            if required:
                diag.error(f"missing '{path}'", "song.yaml", path)
            return default
        cur = cur[part]
    if kind is int and isinstance(cur, bool) or not isinstance(cur, kind):
        diag.error(f"'{path}' must be {_kind_name(kind)}, not {cur!r}", "song.yaml", path)
        return default
    return cur


def _kind_name(kind):
    names = {str: "text", int: "a whole number", dict: "a mapping", list: "a list", (int, float): "a number"}
    return names.get(kind, str(kind))


def _ascii(diag, text, where, maxlen):
    if any(not 32 <= ord(c) <= 126 for c in text):
        diag.error(f"{text!r}: only printable ASCII for now (the device font)", "song.yaml", where)
    if len(text) > maxlen:
        diag.error(f"{text!r} is longer than {maxlen} characters", "song.yaml", where)


def _parse_key(diag, key, mode):
    m = re.match(r"^([A-G])([#b]?)(m?)$", key or "")
    if not m:
        diag.error(f"key {key!r}: expected a note name like G, Bb, F# (or Em for E minor)", "song.yaml", "music.key")
        return 0, 0, "?"
    root = ch._pc(m.group(1), m.group(2))
    if m.group(3):
        if mode not in (None, "minor"):
            diag.error(f"key {key!r} says minor but mode is {mode!r}", "song.yaml", "music.mode")
        mode = "minor"
    mode = mode or "major"
    if mode not in MODES:
        diag.error(f"mode {mode!r}: must be one of {', '.join(MODES)}", "song.yaml", "music.mode")
        return root, 0, key
    note = m.group(1) + m.group(2)
    name = note if mode == "major" else note + "m" if mode == "minor" else f"{note} {mode}"
    return root, MODES.index(mode), name


def _bar_range(diag, text, where):
    m = re.match(r"^\s*(\d+)\s*(?:-\s*(\d+))?\s*$", str(text))
    if not m:
        diag.error(f"bars {text!r}: expected 'FIRST-LAST' like 1-16, or one bar number", "song.yaml", where)
        return None
    first = int(m.group(1))
    last = int(m.group(2)) if m.group(2) else first
    if last < first:
        diag.error(f"bars {text!r}: last bar before the first", "song.yaml", where)
        return None
    return first, last


def parse_chart_text(text, beats_per_bar, where, diag):
    """'| G | G . D7 |' -> [[(beat, symbol), ...] per bar] (beat counts from 0); '.' holds the chord before"""
    if not isinstance(text, str):
        diag.error(f"must be text like '| G | C D |', not {text!r}", "song.yaml", where)
        return []
    lines = [" ".join(line.split()) for line in text.splitlines() if line.strip()]
    if not lines:
        diag.error("empty chart", "song.yaml", where)
        return []
    if any(not line.startswith("|") or not line.endswith("|") or line == "|" for line in lines):
        diag.error("every chart line starts and ends with '|'", "song.yaml", where)
        return []
    bars = []
    for i, cell in enumerate(c for line in lines for c in line[1:-1].split("|")):
        toks = cell.split()
        bw = f"{where} bar {i + 1}"
        if not toks:
            diag.error("empty bar (use '.' to hold the chord, or N.C.)", "song.yaml", bw)
            bars.append([])
            continue
        if "." in toks:
            if len(toks) != beats_per_bar:
                diag.error(f"a bar with '.' needs one token per beat ({beats_per_bar}), it has {len(toks)}",
                           "song.yaml", bw)
                bars.append([])
                continue
            step = 1
        elif beats_per_bar % len(toks):
            diag.error(f"{len(toks)} chords do not split {beats_per_bar} beats evenly; use '.' per beat",
                       "song.yaml", bw)
            bars.append([])
            continue
        else:
            step = beats_per_bar // len(toks)
        cells = []
        for k, tok in enumerate(toks):
            if tok == ".":
                continue
            try:
                ch.parse_chord(tok)
            except ValueError as e:
                diag.error(str(e), "song.yaml", f"{bw} beat {k * step + 1}")
                continue
            cells.append((k * step, tok))
        bars.append(cells)
    return bars


def load_song(folder, diag=None):
    """song folder -> (Song, Diagnostics); raises SongError if the song cannot be compiled"""
    diag = diag or Diagnostics()
    folder = Path(folder)
    ypath, mpath = folder / "song.yaml", folder / "song.mid"
    for p in (ypath, mpath):
        if not p.is_file():
            diag.fatal(f"{p.name} not found in {folder}", p.name)
    try:
        data = yaml.safe_load(ypath.read_text(encoding="utf-8"))
    except (yaml.YAMLError, UnicodeDecodeError) as e:
        diag.fatal(f"not valid YAML: {e}".replace("\n", " "), "song.yaml")
    return song_from_data(data, mpath.read_bytes(), diag)


def song_from_data(data, midi_bytes, diag=None):
    """the parsed song.yaml (a dict) + song.mid's bytes -> (Song, Diagnostics)"""
    diag = diag or Diagnostics()
    if not isinstance(data, dict):
        diag.fatal("song.yaml must be a mapping of keys (format:, title:, ...)", "song.yaml")
    song = Song()
    fmt = data.get("format")
    if fmt != FORMAT:
        diag.fatal(f"format must be '{FORMAT}', not {fmt!r}", "song.yaml", "format")

    song.slug = _get(diag, data, "slug", str, default="?")
    if not _SLUG.match(song.slug):
        diag.error(f"slug {song.slug!r}: lower-case letters, digits and '-', up to 23 characters", "song.yaml", "slug")
    song.title = _get(diag, data, "title", str, default="?")
    _ascii(diag, song.title, "title", 31)
    song.category = _get(diag, data, "category", str, required=False, default="")   # M3, format 1.2: the list's folder
    if song.category:
        _ascii(diag, song.category, "category", 15)
    rights = _get(diag, data, "rights", dict, default={})
    song.rights = rights
    _get(diag, data, "rights.composition", str)
    _get(diag, data, "credits", dict)

    key = _get(diag, data, "music.key", str)
    mode = _get(diag, data, "music.mode", str, required=False)
    song.key_root, song.key_mode, song.key_name = _parse_key(diag, key, mode)
    ts = _get(diag, data, "music.time_signature", str, default="4/4")
    m = re.match(r"^(\d+)/(\d+)$", ts)
    if not m or not 2 <= int(m.group(1)) <= 7 or int(m.group(2)) != 4:
        diag.error(f"time signature {ts!r}: v1 supports 2/4 to 7/4", "song.yaml", "music.time_signature")
        song.beats_per_bar, song.beat_unit = 4, 4
    else:
        song.beats_per_bar, song.beat_unit = int(m.group(1)), 4
    bpm = _get(diag, data, "music.tempo_bpm", (int, float), default=120)
    if not 30 <= bpm <= 250:
        diag.error(f"tempo {bpm} bpm: must be 30..250", "song.yaml", "music.tempo_bpm")
        bpm = 120
    song.tempo_bpm = bpm
    song.tempo_us = round(60_000_000 / bpm)
    song.pickup_beats = _get(diag, data, "music.pickup_beats", int, required=False, default=0)
    if not 0 <= song.pickup_beats < song.beats_per_bar:
        diag.error(f"pickup_beats {song.pickup_beats}: must be 0..{song.beats_per_bar - 1}",
                   "song.yaml", "music.pickup_beats")
        song.pickup_beats = 0
    feel = _get(diag, data, "music.feel", str, required=False, default="straight")
    if feel not in ("straight", "swing"):
        diag.error(f"feel {feel!r}: straight or swing", "song.yaml", "music.feel")
    song.swing = feel == "swing"
    # M5 (format 2.0, D029): a style -> the FM-1 generates harmony 1-5, bass 1-4, drums 1-3 from the chord chart;
    # song.mid then holds only the melody and the levels made by hand or imported (they replace the generated ones)
    song.style = _get(diag, data, "music.style", str, required=False, default=None)
    if song.style is not None and song.style not in GEN_STYLES:
        diag.error(f"style {song.style!r}: one of {', '.join(GEN_STYLES)}", "song.yaml", "music.style")
        song.style = None
    diag.check()                                   # the rest needs a valid metre

    midi = read_midi(midi_bytes, diag)
    song.ppq = midi.ppq
    bpb, ppq = song.beats_per_bar, song.ppq
    bar_ticks = bpb * ppq
    first_number = 0 if song.pickup_beats else 1

    # ---- form and chart
    form = _get(diag, data, "form", list, default=[])
    chart = _get(diag, data, "chart", dict, default={})
    if not form:
        diag.fatal("the form needs at least one section", "song.yaml", "form")
    if len(form) > LIMITS["sections"]:
        diag.fatal(f"{len(form)} sections: at most {LIMITS['sections']}", "song.yaml", "form")
    bar_cells = {}                                  # bar number -> [(beat, symbol)]
    song.sections, expect, parsed_parts = [], 1, {}
    for i, sec in enumerate(form):
        where = f"form[{i + 1}]"
        if not isinstance(sec, dict):
            diag.error("each section is a mapping like { section: Verse 1, bars: 1-16 }", "song.yaml", where)
            continue
        name = sec.get("section")
        if not isinstance(name, str) or not name:
            diag.error("missing 'section' name", "song.yaml", where)
            continue
        _ascii(diag, name, where, 15)
        rng = _bar_range(diag, sec.get("bars"), f"{where} ({name})")
        if rng is None:
            continue
        first, last = rng
        if first != expect:
            diag.error(f"section {name!r} starts at bar {first}; expected bar {expect} (sections must follow "
                       f"each other with no gaps, starting at bar 1)", "song.yaml", where)
        expect = last + 1
        part = sec.get("chart", re.sub(r"\s*\d+$", "", name).strip().lower())
        if part not in chart:
            diag.error(f"section {name!r}: no chart part {part!r}", "song.yaml", where)
            cells = []
        else:
            if part not in parsed_parts:
                parsed_parts[part] = parse_chart_text(chart[part], bpb, f"chart.{part}", diag)
            cells = parsed_parts[part]
            if cells and len(cells) != last - first + 1:
                diag.error(f"section {name!r} has {last - first + 1} bars but chart.{part} has {len(cells)}",
                           "song.yaml", where)
        for k, c in enumerate(cells):
            bar_cells[first + k] = c
        lead = sec.get("pickup_beats", song.pickup_beats)
        if isinstance(lead, bool) or not isinstance(lead, int) or not 0 <= lead < bpb:
            diag.error(f"pickup_beats {lead!r}: must be 0..{bpb - 1}", "song.yaml", where)
            lead = 0
        song.sections.append(Section(name, first - first_number, last - first + 1, lead * ppq, part))
    last_bar = expect - 1
    if last_bar > LIMITS["bars"]:
        diag.fatal(f"{last_bar} bars: at most {LIMITS['bars']}", "song.yaml", "form")
    if song.pickup_beats:
        if "pickup" not in chart:
            diag.error("pickup_beats is set, so the chart needs a one-bar 'pickup' part", "song.yaml", "chart")
        else:
            cells = parse_chart_text(chart["pickup"], bpb, "chart.pickup", diag)
            if len(cells) != 1:
                diag.error(f"chart.pickup must be one bar, it has {len(cells)}", "song.yaml", "chart.pickup")
            elif cells:
                bar_cells[0] = cells[0]
    elif "pickup" in chart:
        diag.error("chart has a 'pickup' part but music.pickup_beats is 0", "song.yaml", "chart.pickup")
    if "ending" in chart:
        cells = parse_chart_text(chart["ending"], bpb, "chart.ending", diag)
        if len(cells) > last_bar:
            diag.error("the ending is longer than the song", "song.yaml", "chart.ending")
        for k, c in enumerate(cells):
            bar_cells[last_bar - len(cells) + 1 + k] = c
    used = {s.chart for s in song.sections} | {"pickup", "ending"}
    for part in chart:
        if part not in used:
            diag.warn(f"chart part {part!r} is not used by any section", "song.yaml", f"chart.{part}")
    diag.check()

    song.bar_count = last_bar + 1 - first_number
    song.end_tick = song.bar_count * bar_ticks
    song.start_tick = (bpb - song.pickup_beats) * ppq if song.pickup_beats else 0

    # ---- chord timeline: merge repeats, so a chord held over several bars is one event
    events = []                                    # [(tick, symbol)]
    for number in range(first_number, last_bar + 1):
        cells = bar_cells.get(number, [])
        if number == first_number and (not cells or cells[0][0] != 0):
            cells = [(0, ch.NC)] + cells            # nothing before the first chord: no chord
        for beat, sym in cells:
            tick = (number - first_number) * bar_ticks + beat * ppq
            if not events or events[-1][1] != sym:
                events.append((tick, sym))
    if len(events) > LIMITS["chord_events"]:
        diag.fatal(f"{len(events)} chord changes: at most {LIMITS['chord_events']}", "song.yaml", "chart")
    table = []
    for _, sym in events:
        if sym not in table:
            table.append(sym)
    if len(table) > LIMITS["chords"]:
        diag.fatal(f"{len(table)} different chords: at most {LIMITS['chords']}", "song.yaml", "chart")
    parsed = [ch.parse_chord(s) for s in table]
    song.chord_table = list(zip(table, parsed))
    voiced = ch.voicings([parsed[table.index(s)] for _, s in events])
    song.chord_events = [(t, table.index(s), v1, v2) for (t, s), (v1, v2) in zip(events, voiced)]

    song.bars = []
    for b in range(song.bar_count):
        t0 = b * bar_ticks
        idx = max(i for i, e in enumerate(song.chord_events) if e[0] <= t0)
        sec = next(i for i, s in enumerate(song.sections) if b < s.first_bar + s.bar_count) \
            if b >= song.sections[0].first_bar else 0
        flags = (1 if song.chord_events[idx][0] == t0 else 0) | \
            (2 if any(s.first_bar == b for s in song.sections) else 0)
        song.bars.append((idx, sec, flags))

    # ---- song.mid
    _read_layers(song, data, midi, events, diag)
    diag.check()
    return song, diag


def _read_layers(song, data, midi, chart_events, diag):
    layers_yaml = _get(diag, data, "layers", dict, default={})
    for name in layers_yaml:
        if name not in LAYERS:
            diag.error(f"unknown layer {name!r} (layers are {', '.join(LAYERS)})", "song.yaml", f"layers.{name}")
    song.layers = {}
    for name in LAYERS:
        if name not in layers_yaml:
            continue
        ly = layers_yaml[name]
        where = f"layers.{name}"
        if not isinstance(ly, dict):
            diag.error("must be a mapping (channel:, variants:, ...)", "song.yaml", where)
            continue
        channel = ly.get("channel")
        if isinstance(channel, bool) or not isinstance(channel, int) or not 1 <= channel <= 16:
            diag.error(f"channel {channel!r}: must be 1..16", "song.yaml", where)
            channel = 1
        if name == "drums" and channel != 10:
            diag.error("drums must be on channel 10 (General MIDI percussion)", "song.yaml", where)
        program = ly.get("program")
        if program is not None and (isinstance(program, bool) or not isinstance(program, int)
                                    or not 1 <= program <= 128):
            diag.error(f"program {program!r}: a General MIDI number 1..128", "song.yaml", where)
            program = None
        if name == "drums" and program is not None:
            diag.error("drums take no program (the GM kit is used)", "song.yaml", where)
        vol, pan = ly.get("volume", 100), ly.get("pan", 64)
        for label, v in (("volume", vol), ("pan", pan)):
            if isinstance(v, bool) or not isinstance(v, int) or not 0 <= v <= 127:
                diag.error(f"{label} {v!r}: must be 0..127", "song.yaml", where)
        layer = Layer(name, channel - 1, None if program is None else program - 1,
                      vol if isinstance(vol, int) else 100, pan if isinstance(pan, int) else 64)
        desc = ly.get("variants", {1: ""})
        if not isinstance(desc, dict) or not desc:
            diag.error("variants must map 1, 2, ... (up to 5) to a short description", "song.yaml", where)
            desc = {1: ""}
        for n in desc:
            if isinstance(n, bool) or not isinstance(n, int) or not 1 <= n <= MAX_VARIANTS:
                diag.error(f"variant {n!r}: must be 1..{MAX_VARIANTS}", "song.yaml", where)
        layer.descriptions = {n: str(d) for n, d in desc.items() if isinstance(n, int)}
        song.layers[name] = layer

    seen = {}
    for track in midi.tracks:
        m = _TRACK.match(track.name or "")
        notes = any(e.kind == "on" for e in track.events)
        if not m:
            if track.name == "chords":
                _cross_check_chords(song, track, chart_events, diag)
            elif notes:
                diag.warn(f"track {track.name!r} has notes but is not a layer (drums, bass, harmony, melody, "
                          f"with :1..:3); ignored", "song.mid", f"track {track.index + 1}")
            _check_meta(song, track, diag)
            continue
        name, var = m.group(1), int(m.group(2) or 1)
        label = f"{name}:{var}"
        if var > MAX_VARIANTS:
            diag.error(f"variant {var}: at most {MAX_VARIANTS}", "song.mid", f"track {track.name}")
            continue
        if label in seen:
            diag.error(f"two tracks for {label}", "song.mid", f"track {track.name}")
            continue
        seen[label] = track
        _check_meta(song, track, diag)
        if name not in song.layers:
            diag.error(f"track {track.name!r} has no entry under layers: in song.yaml", "song.mid",
                       f"track {track.name}")
            continue
        song.layers[name].variants[var] = _convert(song, song.layers[name], track, diag)

    for name, layer in song.layers.items():
        have = sorted(layer.variants)
        if song.style and name in GEN_LEVELS:   # generated: any of its levels may be in song.mid (they override)
            layer.levels = GEN_LEVELS[name]
            for n in have:
                if n > layer.levels:
                    diag.error(f"{name}:{n}: a generated {name} has levels 1..{layer.levels}", "song.mid")
            continue
        if not have:
            diag.error(f"layer {name} is in song.yaml but song.mid has no {name} track", "song.mid")
            continue
        if have != list(range(1, len(have) + 1)):
            diag.error(f"{name} variants {have}: must be 1, 1-2, ... 1-{MAX_VARIANTS} with no gaps", "song.mid")
        for n in layer.descriptions:
            if n not in layer.variants:
                diag.error(f"song.yaml describes {name}:{n} but song.mid has no such track", "song.yaml",
                           f"layers.{name}.variants")
        for n in have:
            if n not in layer.descriptions:
                diag.warn(f"{name}:{n} has no description under layers.{name}.variants", "song.yaml")
    if not song.layers:
        diag.error("no layers: a song needs at least one of drums, bass, harmony, melody", "song.yaml", "layers")


def _check_meta(song, track, diag):
    for e in track.events:
        if e.kind == "tempo":
            bpm = 60_000_000 / e.a if e.a else 0
            if abs(bpm - song.tempo_bpm) > 0.5:
                diag.error(f"tempo {bpm:.1f} bpm, song.yaml says {song.tempo_bpm} (v1: one tempo per song)",
                           "song.mid", f"{track.name or 'track ' + str(track.index + 1)} at {_where_tick(song, e.tick)}")
        elif e.kind == "timesig":
            if (e.a, e.b) != (song.beats_per_bar, song.beat_unit):
                diag.error(f"time signature {e.a}/{e.b}, song.yaml says {song.beats_per_bar}/{song.beat_unit}",
                           "song.mid", f"{track.name or 'track ' + str(track.index + 1)} at {_where_tick(song, e.tick)}")


def _cross_check_chords(song, track, chart_events, diag):
    """the optional 'chords' text track (D014: only a cross-check) must agree with the chart"""
    marks = []
    for e in track.events:
        if e.kind == "text" and e.a == 1:
            sym = e.data.decode("latin-1").strip()
            if not marks or marks[-1][1] != sym:
                marks.append((e.tick, sym))
    chart = [(t, s) for t, s in chart_events if s != ch.NC]
    marks = [(t, s) for t, s in marks if s != ch.NC]
    if marks and marks != chart:
        for (t1, s1), (t2, s2) in zip(marks + [(None, None)] * len(chart), chart):
            if (t1, s1) != (t2, s2):
                diag.warn(f"the 'chords' track says {s1!r} at {_where_tick(song, t1) if t1 is not None else 'the end'}, "
                          f"the chart says {s2!r} at {_where_tick(song, t2)}", "song.mid", "track chords")
                break


def _convert(song, layer, track, diag):
    """a layer track -> sorted [(tick, status, d1, d2)]; notes paired, clipped and checked"""
    out, active, warned = [], {}, set()
    where = lambda t: f"track {track.name} at {song.bar_beat(t)}"

    def warn_once(key, msg, tick):
        if key not in warned:
            warned.add(key)
            diag.warn(msg, "song.mid", where(tick))
    for e in track.events:
        if e.channel is not None and e.channel != layer.channel and e.kind in ("on", "off", "cc", "bend"):
            warn_once("channel", f"events on channel {e.channel + 1}; song.yaml says {layer.channel + 1} "
                                 f"(the device plays them on {layer.channel + 1})", e.tick)
        if e.kind == "on":
            if e.tick >= song.end_tick:
                diag.error(f"note {ch.note_name(e.a)} starts after the end of the song (bar "
                           f"{song.bar_number(song.bar_count - 1)})", "song.mid", where(e.tick))
                continue
            if e.tick < song.start_tick:
                diag.error(f"note {ch.note_name(e.a)} starts before the pickup", "song.mid", where(e.tick))
                continue
            if e.a in active:
                warn_once(("retrigger", e.a), f"note {ch.note_name(e.a)} starts again before its note-off; "
                                              "the first one is ended there", e.tick)
                out.append((e.tick, 0x80, e.a, 0))
            active[e.a] = e.tick
            out.append((e.tick, 0x90, e.a, e.b))
        elif e.kind == "off":
            if e.a not in active:
                continue
            del active[e.a]
            tick = e.tick
            if tick > song.end_tick:
                warn_once("clip", "notes ring past the end of the song; they are cut at the end", tick)
                tick = song.end_tick
            out.append((tick, 0x80, e.a, 0))
        elif e.kind == "cc":
            if e.a not in KEEP_CC:
                warn_once(("cc", e.a), f"controller {e.a} is not kept (v1 keeps "
                                       f"{', '.join(map(str, KEEP_CC))})", e.tick)
            elif e.tick <= song.end_tick:
                out.append((max(e.tick, song.start_tick), 0xB0, e.a, e.b))
        elif e.kind == "bend":
            if e.tick <= song.end_tick:
                out.append((max(e.tick, song.start_tick), 0xE0, e.a, e.b))
        elif e.kind == "program":
            if layer.program is None and layer.name != "drums":
                layer.program = e.a             # song.yaml names none: the track's first one is used
            elif e.a != layer.program:
                warn_once("program", f"program change to {e.a + 1} ignored: the layer plays "
                                     f"layers.{layer.name}.program from song.yaml", e.tick)
        elif e.kind == "pressure":
            warn_once("pressure", "aftertouch is not kept", e.tick)
    for pitch, tick in active.items():
        diag.error(f"note {ch.note_name(pitch)} has no note-off", "song.mid", where(tick))
    order = {0x80: 0, 0xB0: 1, 0xE0: 2, 0x90: 3}
    out.sort(key=lambda e: (e[0], order[e[1]], e[2]))
    if len(out) > LIMITS["events"]:
        diag.error(f"{len(out)} events: at most {LIMITS['events']} per track", "song.mid", f"track {track.name}")
    return out
