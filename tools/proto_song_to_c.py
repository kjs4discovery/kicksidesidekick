#!/usr/bin/env python3
# proto_song_to_c.py -- PROTOTYPE (Step 0.7): one song folder -> a C header for the firmware, at build time.
# Licence: TBD (see D017)
#
#   proto_song_to_c.py SONG_DIR OUT.h
#
# Reads SONG_DIR/song.mid (the notes: tracks drums, bass, harmony, melody with variants like bass:1) and
# SONG_DIR/song.yaml (D014: metadata and the chord chart) and writes OUT.h: the MIDI file's bytes as they are,
# which track plays which layer, and the chord chart as a table of (tick, chord). The device parses no YAML.
# Phase 1's song compiler (Step 1.x) replaces this; it handles only what the draft format v0.1 sample uses:
# one tempo, one time signature, a pickup bar, sections that repeat one chart.
# Run by firmware/build.sh through fm1.toml's gen = [...] (the repo's venv: pyyaml, mido).
import re
import sys
from pathlib import Path

import mido
import yaml

LAYERS = ["drums", "bass", "harmony", "melody"]   # the order of the firmware's layer buttons
DEFAULT_VARIANT = 1


def fail(msg):
    raise SystemExit(f"proto_song_to_c: {msg}")


def bar_tokens(line):
    """'| G | G7 | C |' -> [['G'], ['G7'], ['C']]"""
    return [b.split() for b in line.strip().strip("|").split("|") if b.strip()]


def chart_spans(song, ppq, beats_per_bar):
    """[(tick, symbol)], one per chart token, the way the draft format reads a chart:
    a bar's tokens split it evenly; '.' holds the previous chord one more beat (and makes every token a beat)."""
    bar_ticks = ppq * beats_per_bar
    chart, form = song["chart"], song["form"]
    bars = {}                                       # bar number -> its tokens
    if "pickup" in chart:
        bars[0] = bar_tokens(chart["pickup"])[0]
    last_section = form[-1]["section"]
    for sec in form:
        key = re.sub(r"\s*\d+$", "", sec["section"]).strip().lower()   # "Verse 2" -> "verse"
        if key not in chart:
            fail(f"form section {sec['section']!r}: no chart {key!r}")
        first, last = (int(x) for x in str(sec["bars"]).split("-"))
        toks = [t for line in chart[key].strip().splitlines() for t in bar_tokens(line)]
        if len(toks) != last - first + 1:
            fail(f"section {sec['section']!r}: {last - first + 1} bars, chart {key!r} has {len(toks)}")
        for i, t in enumerate(toks):
            bars[first + i] = t
        if sec["section"] == last_section and "ending" in chart:
            bars[last] = bar_tokens(chart["ending"])[0]
    spans = []
    for bar in sorted(bars):
        toks = bars[bar]
        step = ppq if "." in toks else bar_ticks // len(toks)
        for i, tok in enumerate(toks):
            if tok != ".":
                spans.append((bar * bar_ticks + i * step, tok))
            elif not spans:
                fail(f"bar {bar}: '.' with no chord before it")
    return spans, max(bars) + 1


def merge(spans):
    """drop a span that repeats the chord before it: the screen's 'next' is the next change"""
    out = []
    for t, s in spans:
        if not out or out[-1][1] != s:
            out.append((t, s))
    return out


def c_string(s):
    if any(ord(c) < 32 or ord(c) > 126 or c in '"\\' for c in s):
        fail(f"{s!r}: only printable ASCII in names for now")
    return '"' + s + '"'


def main():
    if len(sys.argv) != 3:
        fail("usage: proto_song_to_c.py SONG_DIR OUT.h")
    folder, out = Path(sys.argv[1]), Path(sys.argv[2])
    song = yaml.safe_load((folder / "song.yaml").read_text())
    raw = (folder / "song.mid").read_bytes()
    mid = mido.MidiFile(file=__import__("io").BytesIO(raw))
    if mid.type != 1:
        fail("song.mid must be a type 1 MIDI file")
    ppq = mid.ticks_per_beat

    tempos, sigs = [], []
    for tr in mid.tracks:
        t = 0
        for e in tr:
            t += e.time
            if e.type == "set_tempo":
                tempos.append((t, e.tempo))
            elif e.type == "time_signature":
                sigs.append((t, e.numerator, e.denominator))
    if len(tempos) != 1 or tempos[0][0] != 0 or len(sigs) != 1 or sigs[0][0] != 0:
        fail("the prototype handles one tempo and one time signature, at tick 0")
    tempo_us, (_, num, den) = tempos[0][1], sigs[0]
    want = f"{num}/{den}"
    if str(song["music"]["time_signature"]) != want:
        fail(f"song.yaml time_signature {song['music']['time_signature']} but song.mid {want}")
    bpm_yaml, bpm_mid = song["music"]["tempo_bpm"], 60e6 / tempo_us
    if abs(bpm_yaml - bpm_mid) > 0.05:
        fail(f"song.yaml tempo {bpm_yaml} but song.mid {bpm_mid:.2f}")
    if den != 4:
        fail("the prototype counts beats in quarter notes")

    # tracks -> layers: "bass:1" plays layer bass, variant 1; "melody" variant 1; others (conductor, chords) none
    channels = {name: int(v["channel"]) - 1 for name, v in song["layers"].items()}
    track_layer, track_variant, layer_tracks = [], [], [0] * len(LAYERS)
    for i, tr in enumerate(mid.tracks):
        name = tr.name or ""
        base, _, var = name.partition(":")
        var = int(var) if var else 1
        if base in LAYERS:
            ch = {e.channel for e in tr if hasattr(e, "channel")}
            if ch and ch != {channels[base]}:
                fail(f"track {name!r} on channel(s) {sorted(c + 1 for c in ch)}, song.yaml says {channels[base] + 1}")
            track_layer.append(LAYERS.index(base))
            track_variant.append(var)
            if var == DEFAULT_VARIANT:
                layer_tracks[LAYERS.index(base)] |= 1 << i
        else:
            track_layer.append(255)
            track_variant.append(0)
    if len(mid.tracks) > 32:
        fail("more than 32 tracks")
    for li, m in enumerate(layer_tracks):
        if not m:
            fail(f"no track for layer {LAYERS[li]} variant {DEFAULT_VARIANT}")
    # every track with notes that is not a default variant is muted from the start
    always_muted = 0
    for i, tr in enumerate(mid.tracks):
        notes = any(e.type == "note_on" for e in tr)
        if notes and not any(m >> i & 1 for m in layer_tracks):
            always_muted |= 1 << i

    spans, nbars = chart_spans(song, ppq, num)
    end_tick = max(sum(e.time for e in tr) for tr in mid.tracks)
    if nbars * num * ppq < end_tick - ppq:
        fail(f"the chart covers {nbars} bars, song.mid {end_tick / (num * ppq):.1f}")

    # cross-check against the old "chords" text track if there is one (D014: optional)
    for tr in mid.tracks:
        if tr.name == "chords":
            t, old = 0, []
            for e in tr:
                t += e.time
                if e.type == "text":
                    old.append((t, e.text))
            if merge(old) != merge(spans):
                fail("the chart and song.mid's chords track disagree")
            print(f"proto_song_to_c: chart matches song.mid's chords track ({len(old)} chords)")
    chords = merge(spans)
    names = sorted({s for _, s in chords}, key=lambda s: [x for _, x in chords].index(s))

    L = ["/* generated by tools/proto_song_to_c.py from " + folder.name + "/song.mid and song.yaml: do not edit.",
         " * PROTOTYPE (Step 0.7). */", "#pragma once", "#include <stdint.h>", ""]
    L += [f"#define SONG_TITLE {c_string(song['title'])}",
          f"#define SONG_KEY {c_string(str(song['music']['key']))}",
          f"#define SONG_PPQ {ppq}u",
          f"#define SONG_BEATS_PER_BAR {num}u",
          f"#define SONG_TEMPO_US {tempo_us}u          /* {bpm_mid:.2f} bpm */",
          f"#define SONG_PICKUP_BEATS {int(song['music'].get('pickup_beats', 0))}u",
          f"#define SONG_BARS {nbars}u                 /* bar 0 = the pickup bar */",
          f"#define SONG_END_TICK {end_tick}u",
          f"#define SONG_NTRACKS {len(mid.tracks)}u",
          f"#define SONG_MUTED_ALWAYS 0x{always_muted:08X}u   /* tracks of the other variants */",
          ""]
    L.append(f"static const uint32_t SONG_LAYER_TRACKS[{len(LAYERS)}] = {{   /* {', '.join(LAYERS)}: "
             f"variant {DEFAULT_VARIANT}'s tracks */")
    L.append("    " + ", ".join(f"0x{m:08X}u" for m in layer_tracks) + "};")
    L.append(f"static const uint8_t SONG_LAYER_CHANNEL[{len(LAYERS)}] = {{"
             + ", ".join(str(channels[n]) for n in LAYERS) + "};   /* counted from 0 */")
    L.append("")
    L.append(f"static const char *const SONG_CHORD_NAME[{len(names)}] = {{" + ", ".join(map(c_string, names)) + "};")
    L.append("static const struct { uint32_t tick; uint8_t chord; } SONG_CHORDS[] = {")
    for t, s in chords:
        b, r = divmod(t, num * ppq)
        L.append(f"    {{{t}u, {names.index(s)}}},   /* bar {b} beat {r // ppq + 1}: {s} */")
    L.append("};")
    L.append(f"#define SONG_NCHORDS {len(chords)}u")
    L.append("")
    L.append(f"static const uint8_t SONG_MID[{len(raw)}] = {{   /* song.mid, as it is */")
    for i in range(0, len(raw), 20):
        L.append("    " + ",".join(f"0x{b:02X}" for b in raw[i:i + 20]) + ",")
    L.append("};")
    text = "\n".join(L) + "\n"
    out.parent.mkdir(parents=True, exist_ok=True)
    if not out.exists() or out.read_text() != text:
        out.write_text(text)
    print(f"proto_song_to_c: {song['title']}: {len(raw)} B MIDI, {len(mid.tracks)} tracks, {len(chords)} chords, "
          f"{nbars} bars -> {out.name}")


if __name__ == "__main__":
    main()
