# fm1song: the MIDI import tool's core (M4 part 5, D004 hybrid, D026). Licence: TBD (see D017)
#
# A Standard MIDI File -> a song folder (song.mid + song.yaml) with the full level set:
#   melody   L1 only: the file's melody (merged tracks, quantized, doubled notes removed, one note at a time)
#   harmony  L1-L5 generated from the recognized chord chart (tools/fm1song/arrange.py); the file's own harmony part
#            replaces the level it most resembles (held / rhythm / inversions / broken / arpeggio)
#   bass     L1-L4 generated the same way; the file's bass replaces the level it resembles
#   drums    L2 the file's drums; L1 simplified from them (kick, snare, hi-hat on the beats); L3 the file's + fills
# Nothing is added to the file's melody or parts; what is generated is reported. Pure functions on bytes and data
# (no file access but in import_file), so the future website can reuse them (D010). The CLI: fm1song import.
from dataclasses import dataclass, field

from . import arrange as arr
from . import chords as ch
from .smf import read_midi

PPQ = 480
GRID = PPQ // 4                         # quantize to sixteenths
# the device's limit for one song (M5, format 2.0): the largest level of each layer together, as the FM-1 holds one
# level a layer at a time (it receives into the flash: the file's size is no longer the limit; M4 cut songs to fit
# 1.3's every level in 64 KiB). Only over BUDGET_EVENTS: the file's harmony and drums on an eighth grid first, then,
# if still over, the first sections that fit (reported)
BUDGET_EVENTS = 24000                   # (the FM-1: 24576)
QUALS = ["", "m", "7", "m7", "maj7", "sus4", "dim"]
DRUM_CH = 9


@dataclass
class TrackInfo:
    index: int
    name: str
    channels: list
    program: int                        # 0..127 (GM, 0-based), -1 none
    notes: int
    lo: int = 0
    hi: int = 0
    mean: float = 0.0
    poly: float = 0.0                   # notes starting together, on average (1: a single line)
    role: str = ""


@dataclass
class Imported:
    """everything a song folder needs, plus a report"""
    title: str
    slug: str
    bpb: int
    tempo: int
    pickup: int
    key: str
    nbars: int
    chart_bars: list                    # per bar index: chart text of the bar ("| C G |" without the bars)
    tracks: dict                        # "melody:1", "harmony:1".. -> [(start, end, pitch, vel)]
    programs: dict                      # layer -> GM program 1..128
    levels: dict                        # layer -> {level: "from the file" / "generated: ..."}
    report: list = field(default_factory=list)
    style: str = "pop"                  # M5 (format 2.0): the FM-1 generates the levels not from the file
    stored: set = field(default_factory=set)   # the track names written to song.mid (from the file)


# --------------------------------------------------------------------------------------------- reading the file
def _notes(track, scale):
    """a track's notes: [(start, end, pitch, vel, channel)] at PPQ 480"""
    out, open_ = [], {}
    for e in track.events:
        if e.kind == "on" and e.b > 0:
            open_.setdefault((e.channel, e.a), []).append((e.tick, e.b))
        elif e.kind == "off" or (e.kind == "on" and e.b == 0):
            st = open_.get((e.channel, e.a))
            if st:
                t0, v = st.pop(0)
                out.append((round(t0 * scale), round(e.tick * scale), e.a, v, e.channel))
    for (c, p), st in open_.items():     # never ended: a sixteenth
        for t0, v in st:
            out.append((round(t0 * scale), round(t0 * scale) + GRID, p, v, c))
    return sorted(out)


def analyze(midi):
    """the file's tracks with notes: [TrackInfo]"""
    scale = PPQ / midi.ppq
    infos = []
    for t in midi.tracks:
        ns = _notes(t, scale)
        if not ns:
            continue
        progs = [e.a for e in t.events if e.kind == "program"]
        starts = {}
        for n in ns:
            starts[n[0] // GRID] = starts.get(n[0] // GRID, 0) + 1
        infos.append(TrackInfo(t.index, (t.name or "").strip() or f"track {t.index}", sorted({n[4] for n in ns}),
                               progs[0] if progs else -1, len(ns), min(n[2] for n in ns), max(n[2] for n in ns),
                               sum(n[2] for n in ns) / len(ns), len(ns) / len(starts)))
    return infos


def guess_roles(infos):
    """track index -> drums / bass / melody / harmony / drop: channel 10 = drums; the lowest-pitched part = bass;
    the highest single line = melody; the rest harmony; a track of a few notes (effects) dropped"""
    roles = {}
    for i in infos:
        if DRUM_CH in i.channels:
            roles[i.index] = "drums"
        elif i.notes < 16:
            roles[i.index] = "drop"
    rest = [i for i in infos if i.index not in roles]
    if rest:
        bass = min(rest, key=lambda i: i.mean)
        if bass.mean < 52:
            roles[bass.index] = "bass"
    rest = [i for i in infos if i.index not in roles]
    lines = [i for i in rest if i.poly < 1.3]
    if lines:
        mel = max(lines, key=lambda i: (i.mean, i.notes))
        roles[mel.index] = "melody"
    for i in infos:
        roles.setdefault(i.index, "harmony")
    return roles


def _meta(midi):
    """(tempo BPM, beats per bar, beat unit, key sharps or None, minor) from the file's first meta events"""
    tempo, num, den, key = 120, 4, 4, None
    found = set()
    for t in midi.tracks:
        for e in t.events:
            if e.kind == "tempo" and "tempo" not in found:
                tempo, _ = round(60_000_000 / e.a), found.add("tempo")
            elif e.kind == "timesig" and "ts" not in found and e.data:
                num, den = e.data[0], 2 ** e.data[1]
                found.add("ts")
            elif e.kind == "key" and "key" not in found and e.data:
                sf = e.data[0] - 256 if e.data[0] > 127 else e.data[0]
                key = (sf, e.data[1] if len(e.data) > 1 else 0)
                found.add("key")
    return tempo, num, den, key


# --------------------------------------------------------------------------------------------- simplifying
def _q(t):
    return (t + GRID // 2) // GRID * GRID


def quantize(notes):
    """starts and ends on sixteenths (a note at least a sixteenth), doubled notes (pitch and start) merged"""
    seen, out = set(), []
    for s, e, p, v, *_ in sorted(notes):
        a, b = _q(s), max(_q(e), _q(s) + GRID)
        if (a, p) in seen:
            continue
        seen.add((a, p))
        out.append((a, b, p, v))
    return out


def monophonic(notes):
    """one note at a time: at a shared start the highest; a note ends where the next starts"""
    first = {}
    for s, e, p, v in notes:
        if s not in first or p > first[s][2]:
            first[s] = (s, e, p, v)
    seq = [first[s] for s in sorted(first)]
    out = []
    for k, (s, e, p, v) in enumerate(seq):
        if k + 1 < len(seq) and e > seq[k + 1][0]:
            e = seq[k + 1][0]
        if e > s:
            out.append((s, e, p, v))
    return out


def thin_chords(notes, most=5):
    """at most `most` notes start together (the lowest bass note and the highest kept first), within C2..C7"""
    by = {}
    for n in notes:
        if 36 <= n[2] <= 96:
            by.setdefault(n[0], []).append(n)
    out = []
    for s in sorted(by):
        grp = sorted(by[s], key=lambda n: n[2])
        if len(grp) > most:
            grp = [grp[0]] + grp[-(most - 1):]
        out += grp
    return out


# --------------------------------------------------------------------------------------------- chords and key
MAJOR = [6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88]   # Krumhansl-Kessler profiles
MINOR = [6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17]
NAMES_SHARP = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
NAMES_FLAT = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]
FLAT_KEYS = {5, 10, 3, 8, 1}             # F Bb Eb Ab Db major (and their minors' parents)


def detect_key(notes, meta_key=None):
    """(root 0..11, minor) by the pitch profile of all pitched notes (a file's key signature, if any, for the
    sharps/flats only)"""
    hist = [0.0] * 12
    for s, e, p, v in notes:
        hist[p % 12] += e - s
    best = None
    for r in range(12):
        for minor, prof in ((0, MAJOR), (1, MINOR)):
            sc = sum(hist[(r + i) % 12] * prof[i] for i in range(12))
            if best is None or sc > best[0]:
                best = (sc, r, minor)
    return best[1], best[2]


def key_name(root, minor):
    parent = (root + 3) % 12 if minor else root
    nm = (NAMES_FLAT if parent in FLAT_KEYS else NAMES_SHARP)[root]
    return nm + ("m" if minor else "")


def chord_name(root, qual, flats):
    return (NAMES_FLAT if flats else NAMES_SHARP)[root] + qual


def recognize(hist, bass_pc, prev=None):
    """the chord (root, quality) that best explains a pitch-class weight histogram, a bass note helping the root;
    None if nearly silent"""
    total = sum(hist)
    if total <= 0:
        return None
    best = None
    for r in range(12):
        for q in QUALS:
            pcs = {(r + i) % 12 for i in ch.QUALITIES[q]}
            inside = sum(hist[pc] for pc in pcs)
            outside = total - inside
            sc = inside - 0.8 * outside - 0.12 * total * (len(pcs) - 3)   # (simpler chords preferred)
            if bass_pc is not None and bass_pc == r:
                sc += 0.25 * total
            if prev == (r, q):
                sc += 0.05 * total                                         # (a chord tends to go on)
            if best is None or sc > best[0]:
                best = (sc, r, q)
    return best[1], best[2]


def chord_chart(notes_h, notes_b, nbars, bpb, flats):
    """per bar: [(beat, (root, qual) or None)], one chord a bar, or two a bar in 4/4 when the halves differ"""
    bar = bpb * PPQ
    seg_beats = [0, 2] if bpb == 4 else [0]
    out, prev = [], None
    for b in range(nbars):
        row = []
        for k, beat in enumerate(seg_beats):
            t0 = b * bar + beat * PPQ
            t1 = b * bar + (seg_beats[k + 1] * PPQ if k + 1 < len(seg_beats) else bar)
            hist = [0.0] * 12
            for s, e, p, v in notes_h + notes_b:
                o = min(e, t1) - max(s, t0)
                if o > 0:
                    hist[p % 12] += o * (1.5 if (s, e, p, v) in notes_b else 1.0)
            low = [n for n in notes_b if n[0] < t1 and n[1] > t0]
            bass_pc = min(low, key=lambda n: n[2])[2] % 12 if low else None
            c = recognize(hist, bass_pc, prev)
            if c is None:
                c = prev
            row.append((beat, c))
            prev = c
        if len(row) == 2 and row[0][1] == row[1][1]:
            row = row[:1]
        out.append(row)
    return out


def chart_text(row, bpb, flats):
    toks = []
    if len(row) == 1:
        c = row[0][1]
        return "| " + ("N.C." if c is None else chord_name(c[0], c[1], flats)) + " |"
    for beat, c in row:
        toks.append("N.C." if c is None else chord_name(c[0], c[1], flats))
    return "| " + " ".join(toks) + " |"


# --------------------------------------------------------------------------------------------- level matching
def classify_harmony(notes, chart, bpb):
    """the level a harmony part resembles: 1 held, 2 rhythm (root position), 3 inversions, 4 broken, 5 arpeggio"""
    bar = bpb * PPQ
    starts = {}
    for n in notes:
        starts.setdefault(n[0], []).append(n[2])
    if not starts:
        return 1
    per_attack = sum(len(v) for v in starts.values()) / len(starts)
    nchords = sum(len(r) for r in chart) or 1
    attacks_per_chord = len(starts) / nchords
    if per_attack < 1.6:                 # a note at a time: broken chords or arpeggios, by their span in a bar
        spans = []
        for b in range(len(chart)):
            ps = [n[2] for n in notes if b * bar <= n[0] < (b + 1) * bar]
            if ps:
                spans.append(max(ps) - min(ps))
        return 5 if spans and sorted(spans)[len(spans) // 2] > 12 else 4
    if attacks_per_chord < 1.5:
        return 1
    inv = 0
    for b, row in enumerate(chart):      # the lowest note of each attack against the chord's root
        for k, (beat, c) in enumerate(row):
            t0 = b * bar + beat * PPQ
            t1 = b * bar + (row[k + 1][0] * PPQ if k + 1 < len(row) else bar)
            for s, ps in starts.items():
                if t0 <= s < t1 and c is not None and min(ps) % 12 != c[0]:
                    inv += 1
    return 3 if inv > 0.4 * len(starts) else 2


def classify_bass(notes, chart, bpb):
    """the level a bass part resembles: 1 held, 2 the root in rhythm, 3 root and fifth, 4 walking"""
    bar = bpb * PPQ
    nchords = sum(len(r) for r in chart) or 1
    if len(notes) / nchords < 1.5:
        return 1
    root = fifth = 0
    for s, e, p, v in notes:
        b = s // bar
        if b >= len(chart):
            continue
        row = chart[b]
        c = [x for beat, x in row if beat * PPQ <= s - b * bar][-1] if row else None
        if c is None:
            continue
        if p % 12 == c[0]:
            root += 1
        elif p % 12 == (c[0] + 7) % 12:
            fifth += 1
    n = len(notes)
    if root >= 0.8 * n:
        return 2
    if root + fifth >= 0.8 * n:
        return 3
    return 4


# --------------------------------------------------------------------------------------------- loudness
def loudness(notes, length):
    """a model of how loud a part sounds (M5): EAS's amplitude grows with the velocity squared and notes add up in
    power, so: sqrt(mean over time of the sum of (velocity / 127) ** 4 of the notes sounding), a note counted for at
    most a beat (they decay)"""
    if not notes or length <= 0:
        return 0.0
    e = sum((v / 127) ** 4 * min(e2 - s, PPQ) for s, e2, p, v in notes)
    return (e / length) ** 0.5


def match_loudness(notes, ref, length):
    """the part's velocities scaled so its loudness model equals ref's (the generated level it replaces): loudness
    goes with the velocity squared, so the factor is the square root of the ratio"""
    a, b = loudness(notes, length), loudness(ref, length)
    if a <= 0 or b <= 0:
        return notes, 1.0
    k = (b / a) ** 0.5
    return [(s, e, p, max(1, min(127, round(v * k)))) for s, e, p, v in notes], k


# --------------------------------------------------------------------------------------------- the whole import
def events_of(imp):
    return sum(2 * len(v) for v in imp.tracks.values())


def largest_levels(imp):
    """the events of the largest level of each layer together: what the FM-1 holds at once (format 2.0)"""
    by = {}
    for n, v in imp.tracks.items():
        by[n.split(":")[0]] = max(by.get(n.split(":")[0], 0), 2 * len(v))
    return sum(by.values())


def to_eighths(notes):
    """starts on eighths; at most one attack of a pitch an eighth (strums and rolls simplified)"""
    g = PPQ // 2
    seen, out = set(), []
    for s, e, p, v in sorted(notes):
        a = (s + g // 2) // g * g
        if (a, p) in seen:
            continue
        seen.add((a, p))
        out.append((a, max(e, a + g // 2), p, v))
    return out


def build(midi, title, slug, roles=None, style=None, report=None, eighths=False, max_bars=None):
    """a MidiFile -> Imported. roles: track index -> drums / bass / melody / harmony / drop (default: guess_roles);
    eighths: the file's harmony and drums on an eighth grid; max_bars: only the first bars"""
    rep = report if report is not None else []
    infos = analyze(midi)
    roles = roles or guess_roles(infos)
    scale = PPQ / midi.ppq
    tempo, num, den, kmeta = _meta(midi)
    if den != 4 or not 2 <= num <= 7:
        rep.append(f"time signature {num}/{den}: the song format takes 2/4..7/4; read as {min(max(num, 2), 7)}/4")
    bpb = min(max(num, 2), 7) if den == 4 else (num // 3 if den == 8 and num % 3 == 0 else 4)
    tempo = min(max(tempo, 30), 250)
    parts = {"melody": [], "harmony": [], "bass": [], "drums": []}
    progs = {}
    for t in midi.tracks:
        r = roles.get(t.index)
        if r in parts:
            parts[r] += _notes(t, scale)
            info = next(i for i in infos if i.index == t.index)
            if r != "drums" and info.program >= 0 and r not in progs:
                progs[r] = info.program + 1
    for i in infos:
        rep.append(f"track {i.index} '{i.name}': ch {[c + 1 for c in i.channels]}, program "
                   f"{i.program + 1 if i.program >= 0 else '-'}, {i.notes} notes, {ch.note_name(i.lo)}-"
                   f"{ch.note_name(i.hi)}, {i.poly:.1f} at once -> {roles.get(i.index, 'drop')}")
    q = {k: quantize(v) for k, v in parts.items()}
    q["melody"] = monophonic(q["melody"])
    if eighths:
        q["harmony"], q["drums"] = to_eighths(q["harmony"]), to_eighths(q["drums"])
    q["harmony"] = thin_chords(q["harmony"], 4 if eighths else 5)
    q["bass"] = monophonic([n for n in q["bass"]])     # (the lowest line: monophonic keeps the highest; fix below)
    if parts["bass"]:
        low = {}
        for s, e, p, v in quantize(parts["bass"]):
            if s not in low or p < low[s][2]:
                low[s] = (s, e, p, v)
        seq = [low[s] for s in sorted(low)]
        q["bass"] = [(s, min(e, seq[k + 1][0]) if k + 1 < len(seq) else e, p, v) for k, (s, e, p, v) in enumerate(seq)]
    allpitched = q["melody"] + q["harmony"] + q["bass"]
    every = allpitched + q["drums"]
    if not every:
        raise ValueError("no notes in the chosen tracks")
    bar = bpb * PPQ
    # the first bar with music becomes bar 1; a melody starting before the accompaniment in the bar before: a pickup
    first = min(n[0] for n in every)
    acc = [n[0] for n in q["harmony"] + q["bass"] + q["drums"]]
    first_acc = min(acc) if acc else first
    shift = first_acc // bar * bar
    pickup = 0
    if first < shift:
        pk = (shift - first + PPQ - 1) // PPQ
        if pk < bpb:
            pickup = pk
            shift -= bar
        else:
            shift = first // bar * bar
    for k in q:
        q[k] = [(s - shift, e - shift, p, v) for s, e, p, v in q[k] if s >= shift]
    last = max(e for v in q.values() for _, e, _, _ in v)
    nbars = (last + bar - 1) // bar
    if max_bars and nbars > max_bars:
        nbars = max_bars
        for k in q:
            q[k] = [(s, min(e, nbars * bar), p, v) for s, e, p, v in q[k] if s < nbars * bar]
    if nbars > 999:
        raise ValueError(f"{nbars} bars: the format takes 999")
    if shift:
        rep.append(f"{shift // bar} empty bar(s) at the start left out")
    kr, km = detect_key(allpitched)
    key = key_name(kr, km)
    flats = ((kr + 3) % 12 if km else kr) in FLAT_KEYS
    rep.append(f"tempo {tempo} BPM (the file's first), {bpb}/4, key {key} (by the notes), {nbars} bars"
               + (f", a {pickup}-beat pickup" if pickup else ""))
    chart = chord_chart(q["harmony"], q["bass"], nbars, bpb, flats)
    if pickup:
        chart[0] = [(0, None)]
    chart_bars = [chart_text(r, bpb, flats) for r in chart]
    # the levels
    sty = arr.style_of(style, bpb, tempo, False)
    rep.append(f"style for the generated levels: {sty}" + ("" if style else " (from the metre and tempo)"))
    bars = []
    for row in chart:
        cells = []
        for k, (beat, c) in enumerate(row):
            end = row[k + 1][0] if k + 1 < len(row) else bpb
            cells.append((beat, end - beat, None if c is None else ch.parse_chord(chord_name(c[0], c[1], flats))))
        bars.append(cells)
    section_starts = set(range(1 if pickup else 0, nbars, 8))
    gen = arr.arrange(bars, bpb, pickup, section_starts - {1 if pickup else 0}, arr.STYLES[sty], False)
    tracks = {"melody:1": q["melody"]} if q["melody"] else {}
    levels = {"melody": {1: "the file's melody"}} if q["melody"] else {}
    for layer, top in (("harmony", 5), ("bass", 4)):
        levels[layer] = {}
        own = None
        if q[layer]:
            own = (classify_harmony if layer == "harmony" else classify_bass)(q[layer], chart, bpb)
        for lv in range(1, top + 1):
            if lv == own:                # as loud as the level it replaces (M5: the file parts were up to 5x louder)
                tracks[f"{layer}:{lv}"], k = match_loudness(q[layer], gen[f"{layer}:{lv}"], nbars * bar)
                levels[layer][lv] = "from the file"
                rep.append(f"{layer}: the file's part at level {lv} scaled to the generated level's loudness "
                           f"(velocity x {k:.2f})")
            else:
                tracks[f"{layer}:{lv}"] = gen[f"{layer}:{lv}"]
                levels[layer][lv] = "generated: " + arr.LEVEL_TEXT[layer][lv]
        rep.append(f"{layer}: " + (f"the file's part resembles level {own} and is that level" if own else
                                   "no part in the file: all levels generated"))
    if q["drums"]:
        d2, k = match_loudness(q["drums"], gen["drums:2"], nbars * bar)
        rep.append(f"drums: the file's drums scaled to the generated groove's loudness (velocity x {k:.2f})")
        beat_hits = [(s, e, p, v) for s, e, p, v in d2 if s % PPQ == 0 and p in (35, 36, 37, 38, 40, 42, 44, 46)]
        d1 = [(s, s + GRID, 36 if p in (35, 36) else 38 if p in (37, 38, 40) else 42, v) for s, e, p, v in beat_hits]
        d3 = list(d2)
        fills = [n for n in gen["drums:3"] if n[2] in (38, 45, 50) and (n[0] // PPQ) % bpb == bpb - 1]
        fill_beats = {n[0] // PPQ for n in fills}
        d3 = [n for n in d3 if n[0] // PPQ not in fill_beats] + fills
        tracks.update({"drums:1": d1, "drums:2": d2, "drums:3": d3})
        levels["drums"] = {1: "simplified from the file: kick, snare, hi-hat on the beats", 2: "from the file",
                           3: "the file's drums with generated fills"}
    else:
        for lv in (1, 2, 3):
            tracks[f"drums:{lv}"] = gen[f"drums:{lv}"]
        levels["drums"] = {lv: "generated: " + arr.LEVEL_TEXT["drums"][lv] for lv in (1, 2, 3)}
        rep.append("drums: none in the file: all levels generated")
    stored = {n for n in tracks if n == "melody:1" or (levels.get(n.split(":")[0], {}).get(int(n.split(":")[1]), "")
                                                       .find("generated") != 0)}
    return Imported(title, slug, bpb, tempo, pickup, key, nbars, chart_bars, tracks, progs, levels, rep, sty, stored)


# --------------------------------------------------------------------------------------------- writing it
CHANNELS = dict(drums=10, bass=3, harmony=2, melody=1)


def song_mid(imp):
    from .smf import meta_tempo, meta_timesig, write_midi
    out = [("conductor", [(0, meta_tempo(round(60_000_000 / imp.tempo))), (0, meta_timesig(imp.bpb, 4))])]
    for name in sorted((n for n in imp.tracks if n in imp.stored),   # (2.0: the generated levels are the FM-1's)
                       key=lambda n: (["melody", "harmony", "bass", "drums"].index(n.split(":")[0]), n)):
        layer = name.split(":")[0]
        chan = CHANNELS[layer] - 1
        evs = []
        if layer in imp.programs and layer != "drums":
            evs.append((0, bytes([0xC0 | chan, imp.programs[layer] - 1])))
        raw, seen, last = [], set(), {}
        notes = sorted(imp.tracks[name])
        for k, (s, e, p, v) in enumerate(notes):
            if (s, p) in seen:
                continue
            seen.add((s, p))
            if p in last and last[p][1] > s:      # (an overlapping note of the same pitch ends where this starts)
                i = last[p][0]
                raw[i] = (s, 0, raw[i][2])
            raw.append((s, 1, bytes([0x90 | chan, p, max(1, min(127, v))])))
            raw.append((max(e, s + 10), 0, bytes([0x80 | chan, p, 0])))
            last[p] = (len(raw) - 1, e)
        raw.sort(key=lambda x: (x[0], x[1]))
        evs += [(t, b) for t, _, b in raw]
        out.append((name if layer != "melody" else "melody", evs))
    return write_midi(PPQ, out)


def song_yaml(imp, category, rights, credit):
    q = lambda x: "'" + str(x).replace("'", "''") + "'"
    form, parts, first = [], [], 1 if imp.pickup else 0
    start = 1 if imp.pickup else 0
    k = 0
    while start < imp.nbars:
        n = min(8, imp.nbars - start)
        k += 1
        num0 = start if imp.pickup else start + 1
        form.append(f"  - {{ section: Part {k}, bars: {num0}-{num0 + n - 1}, chart: p{k} }}")
        lines = []
        for r in range(start, start + n, 4):
            cells = [imp.chart_bars[b].strip("| ").strip() for b in range(r, min(r + 4, start + n))]
            lines.append("    | " + " | ".join(cells) + " |")
        parts.append(f"  p{k}: |\n" + "\n".join(lines))
        start += n
    lay = []
    for layer in ("drums", "bass", "harmony", "melody"):
        if layer not in imp.levels:
            continue
        lay.append(f"  {layer}:\n    channel: {CHANNELS[layer]}")
        if layer in imp.programs and layer != "drums":
            lay.append(f"    program: {imp.programs[layer]}")
        lay.append("    variants:")
        for lv, d in imp.levels[layer].items():
            lay.append(f"      {lv}: {q(d)}")
    pick = '  pickup: "| N.C. |"\n' if imp.pickup else ""
    return f"""# Song source file, format fm1-song/1 (docs/song-format.md).
# IMPORTED by fm1song import (tools/fm1song/importer.py) from a MIDI file; the chord chart was recognized from the
# file's harmony and bass and should be reviewed. Levels "from the file" are the file's parts, quantized; the
# others are generated (D004: hybrid).
format: fm1-song/1
slug: {imp.slug}
title: {q(imp.title)}
category: {q(category)}

credits:
  music: {q(credit)}
  arrangement: 'imported from the MIDI file; generated levels by tools/fm1song/arrange.py'

rights:
  composition: {q(rights)}
  arrangement_license: TBD
  generated: fm1song import

music:
  key: {imp.key}
  time_signature: {imp.bpb}/4
  tempo_bpm: {imp.tempo}
  pickup_beats: {imp.pickup}
  feel: straight
  style: {imp.style}                  # the FM-1 generates the levels not from the file (format 2.0)

form:
{chr(10).join(form)}

layers:
{chr(10).join(lay)}

chart:
{pick}{chr(10).join(parts)}
"""


def import_file(path, out, title=None, slug=None, category="Imported", rights="TBD", credit=None, roles=None,
                style=None):
    """read a .mid, write out/song.mid, out/song.yaml and out/IMPORT-REPORT.txt; -> (Imported, report lines)"""
    import re
    from pathlib import Path
    path, out = Path(path), Path(out)
    midi = read_midi(path.read_bytes())
    title = title or path.stem.replace("_", " ")[:31]
    slug = slug or re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")[:23].strip("-")
    rep = [f"import of {path.name}"]
    imp = build(midi, title, slug, roles, style, rep)
    if largest_levels(imp) > BUDGET_EVENTS:   # (M5: one level of each layer at once; over: simplify, then shorten)
        full = imp.nbars
        rep2 = [f"import of {path.name}"]
        imp = build(midi, title, slug, roles, style, rep2, eighths=True)
        note = [f"{largest_levels(imp)} events in the largest levels: the file's harmony and drums simplified to "
                f"eighths (at most 4 notes at once) to fit the FM-1 (budget {BUDGET_EVENTS} events)"]
        bars = imp.nbars
        while largest_levels(imp) > BUDGET_EVENTS and bars > 8:
            bars = (bars - 1) // 8 * 8 + (1 if imp.pickup else 0)
            rep2 = [f"import of {path.name}"]
            imp = build(midi, title, slug, roles, style, rep2, eighths=True, max_bars=bars)
        if imp.nbars < full:
            note.append(f"still over: only the first {imp.nbars} of {full} bars are kept (whole 8-bar sections), "
                        f"{largest_levels(imp)} events")
        rep = rep2 + note
    out.mkdir(parents=True, exist_ok=True)
    (out / "song.mid").write_bytes(song_mid(imp))
    (out / "song.yaml").write_text(song_yaml(imp, category, rights, credit or f"from the MIDI file {path.name}"))
    for layer, lv in imp.levels.items():
        rep.append(f"{layer} levels: " + "; ".join(f"L{k} {d}" for k, d in lv.items()))
    (out / "IMPORT-REPORT.txt").write_text("\n".join(rep) + "\n")
    return imp, rep


def parse_roles(infos, args):
    """the CLI's --melody 3,4 --bass 7 ... over the guess: -> track index -> role"""
    roles = guess_roles(infos)
    for role in ("melody", "harmony", "bass", "drums", "drop"):
        v = getattr(args, role, None)
        if v:
            for k in (int(x) for x in v.split(",")):
                roles[k] = role
    return roles
