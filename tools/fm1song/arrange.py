# fm1song: the accompaniment generators (M4, D026): harmony levels 1-5, bass 1-4, drums 1-3, from a chord timeline.
# Licence: TBD (see D017)
#
# Used by songs/make_songs.py (our songs) and by the MIDI import tool (tools/fm1song/importer.py): the import keeps a
# file's own parts and generates the other levels here (D004: hybrid). Pure functions, no file access, so the
# website can reuse them later (D010).
#
# The ladder (the user's, D026):
#   harmony  L1 block chords held          L2 rhythmic block chords        L3 rhythmic chords with inversions
#            L4 broken chords              L5 arpeggios (~1.5 octaves)
#   bass     L1 root held    L2 rhythmic root    L3 root-fifth    L4 walking / more motion
#   drums    L1 the pulse    L2 a groove         L3 the groove with fills and crashes
# The rhythm of each level comes from the song's style (its metre and feel): see STYLES.
# Loudness (M5): the levels of a layer sound about equally loud (measured in the emulator, docs/LOG M5 Part 1):
# harmony L4, L5 (single notes) and bass L1 (held) have higher velocities than the busier levels.
from . import chords as ch
from .errors import Diagnostics
from .source import parse_chart_text

PPQ = 480
KICK, STICK, SNARE, HAT, OPEN_HAT, PEDAL_HAT, RIDE, CRASH, TOM_LO, TOM_HI = 36, 37, 38, 42, 46, 44, 51, 49, 45, 50

# style -> what each part does; a song names one (or a part each)
STYLE_NAMES = ("waltz", "ballad", "shuffle", "march", "pop", "train",   # (format 2.0's STYL byte: the index)
               "rock", "indie", "bossa", "funk", "reggae")              # M8 (D048, format 2.2): 6..10
NEW_STYLES = STYLE_NAMES[6:]                                            # (an older FM-1 reads only the first six)
STYLES = {
    "waltz":   dict(harmony="waltz", bass="waltz", drums="waltz"),        # 3/4: oom-pah-pah
    "ballad":  dict(harmony="ballad", bass="ballad", drums="ballad"),     # slow 4/4: held quarters, Alberti
    "shuffle": dict(harmony="shuffle", bass="boogie", drums="shuffle"),   # swung 4/4 (the blues)
    "march":   dict(harmony="stride", bass="two-beat", drums="march"),    # 2-feel 4/4 (the Saints)
    "pop":     dict(harmony="eighths", bass="eighths", drums="pop"),      # straight eighths
    "train":   dict(harmony="eighths", bass="two-beat", drums="train"),   # fast 2-feel (Oh! Susanna)
    # M8 (D048): table-driven (RHYTHM, BASS_LINE, GROOVE below): the harmony's L2 / L3 rhythm, the bass's L4 line,
    # the drums' L2 groove (L1 the pulse, L3 the groove with crashes and fills, as every style)
    "rock":    dict(harmony="rock", bass="rock", drums="rock"),           # straight eighths, driving (ROCK1)
    "indie":   dict(harmony="indie", bass="indie", drums="indie"),        # 3-3-2 pushes (POP3)
    "bossa":   dict(harmony="bossa", bass="bossa", drums="bossa"),        # bossa nova: the 2-bar comp (BOSSA)
    "funk":    dict(harmony="funk", bass="funk", drums="funk"),           # 16th stabs (FUNK1)
    "reggae":  dict(harmony="reggae", bass="reggae", drums="reggae"),     # the skank on the off-beats (REGGAE)
}

# M8 (D048): the new styles' tables, on a 16th-note grid of a 4/4 bar (step 0..15; in another metre the steps before
# 4 x beats; a 2-bar table: even bars the first, odd bars the second). firmware/app/gen.c has the same tables.
# RHYTHM: harmony L2 / L3: (step, length in steps, velocity); UPPER: the voicing's lowest note left out (4 or more)
RHYTHM = {
    "rock":   [[(0, 2, 62), (2, 2, 50), (4, 2, 58), (6, 2, 50), (8, 2, 60), (10, 2, 50), (12, 2, 58), (14, 2, 50)]],
    "indie":  [[(0, 6, 62), (6, 6, 56), (12, 4, 58)]],
    "bossa":  [[(0, 2, 58), (3, 2, 52), (6, 3, 54), (10, 2, 52), (13, 3, 54)],
               [(2, 2, 52), (5, 2, 54), (8, 2, 56), (11, 3, 52)]],
    "funk":   [[(0, 1, 70), (3, 1, 56), (6, 1, 62), (7, 1, 50), (10, 1, 64), (12, 1, 56), (14, 1, 60)]],
    "reggae": [[(2, 1, 64), (6, 1, 64), (10, 1, 64), (14, 1, 64)]],
}
UPPER = {"funk", "reggae"}
# BASS_LINE: bass L4: (step, length in steps, what, velocity); what: R root, F fifth, O octave, N the next chord's root,
# A a step into the next root (the fifth when the next chord has the same root)
BASS_LINE = {
    "rock":   [(0, 2, "R", 88), (2, 2, "R", 74), (4, 2, "R", 80), (6, 2, "R", 74), (8, 2, "R", 86), (10, 2, "R", 74),
               (12, 2, "F", 80), (14, 2, "A", 76)],
    "indie":  [(0, 6, "R", 90), (6, 6, "R", 80), (12, 4, "F", 82)],
    "bossa":  [(0, 6, "R", 90), (6, 2, "F", 74), (8, 6, "F", 84), (14, 2, "N", 76)],
    "funk":   [(0, 2, "R", 96), (3, 1, "O", 72), (6, 1, "R", 84), (7, 1, "R", 70), (10, 2, "F", 84), (12, 1, "O", 72),
               (14, 2, "A", 80)],
    "reggae": [(0, 4, "R", 92), (6, 2, "F", 78), (8, 4, "R", 88), (13, 1, "O", 70), (14, 2, "F", 76)],
}
# GROOVE: drums L2, from M7's docs/drum-patterns.txt (ROCK1, POP3, BOSSA, FUNK1, REGGAE): per bar a row per drum,
# x 92, o 70, g 34
GROOVE_VEL = {"x": 92, "o": 70, "g": 34}
GROOVE_DRUM = {"KICK": 36, "SNARE": 38, "CHH": 42, "OHH": 46, "CLAP": 39, "TOMLO": 45, "TOMHI": 50, "RIM": 37}
GROOVE = {
    "rock":   [{"CHH": "x.o.x.o.x.o.x.o.", "SNARE": "....x.......x...", "KICK": "x.......x.o....."}],
    "indie":  [{"CHH": "o.o.o.o.o.o.o.o.", "SNARE": "....x.......x...", "CLAP": "....o.......o...",
                "KICK": "x.....o...o....."},
               {"CHH": "o.o.o.o.o.o.o.o.", "SNARE": "....x.......x...", "CLAP": "....o.......o...",
                "KICK": "x.....o...o..o..", "OHH": "..............o."}],
    "bossa":  [{"CHH": "o.o.o.o.o.o.o.o.", "RIM": "x..x..x...x..x..", "KICK": "x..ox..ox..ox..o"},
               {"CHH": "o.o.o.o.o.o.o.o.", "RIM": "..x..x..x..x....", "KICK": "x..ox..ox..ox..o"}],
    "funk":   [{"CHH": "xoxoxoxoxoxoxoxo", "SNARE": "....x..g.g..x..g", "KICK": "x.x.......x..o.."}],
    "reggae": [{"CHH": "..o...o...o...o.", "RIM": "........x.......", "SNARE": "........x.......",
                "KICK": "........x......."}],
}
GROOVE_ORDER = ("KICK", "SNARE", "CHH", "OHH", "CLAP", "TOMLO", "TOMHI", "RIM")   # (gen.c's row order)
STEP = PPQ // 4


def _steps(tab, bpb):
    """a 16-step table over a bar of bpb beats: repeated every 16 steps, cut at the bar's end"""
    return [(r * 16 + e[0],) + tuple(e[1:]) for r in range((bpb * 4 + 15) // 16) for e in tab
            if r * 16 + e[0] < bpb * 4]

LEVEL_TEXT = {
    "harmony": {1: "block chords, root position, held", 2: "block chords in the song's rhythm",
                3: "the same rhythm with inversions (voice leading)", 4: "broken chords",
                5: "arpeggios across about 1.5 octaves"},
    "bass": {1: "the root, held", 2: "the root in rhythm", 3: "root and fifth", 4: "walking, more motion"},
    "drums": {1: "the pulse (kick, snare backbeat, hi-hat)", 2: "a groove", 3: "the groove with fills and crashes"},
}


def style_of(name, bpb, tempo, swing):
    """a style name (STYLES) for a song with no style given: from its metre, tempo and feel"""
    if name:
        return name
    if bpb == 3:
        return "waltz"
    if swing:
        return "shuffle"
    if tempo < 80:
        return "ballad"
    return "train" if tempo >= 140 else "pop"


def timeline(chart, form, pickup, bpb, ending=None):
    """the chord chart -> per bar index: [(start_beat, length_beats, Chord or None)]. chart: {part: text},
    form: [(name, bars, part)], pickup: beats (0: none)"""
    diag = Diagnostics()
    bars = []
    if pickup:
        bars.append(parse_chart_text(chart["pickup"], bpb, "pickup", diag)[0])
    for _, count, part in form:
        cells = parse_chart_text(chart[part], bpb, part, diag)
        assert len(cells) == count, (part, len(cells), count)
        bars += cells
    if ending:
        bars[-1] = parse_chart_text(ending, bpb, "ending", diag)[0]
    assert not diag.errors, [str(d) for d in diag.errors]
    out, last = [], None
    for cells in bars:
        row = []
        for k, (beat, sym) in enumerate(cells):
            end = cells[k + 1][0] if k + 1 < len(cells) else bpb
            row.append((beat, end - beat, ch.parse_chord(sym)))
        if not row or row[0][0] != 0:            # a bar starting with '.': the chord before goes on
            row.insert(0, (0, row[0][0] if row else bpb, last))
        last = row[-1][2]
        out.append(row)
    return out


def swing_tick(t):
    """straight eighths -> shuffle eighths: the off-beat moves from 1/2 to 2/3 of the beat"""
    beat, frac = divmod(t, PPQ)
    if frac <= PPQ // 2:
        frac = frac * (2 * PPQ // 3) // (PPQ // 2)
    else:
        frac = 2 * PPQ // 3 + (frac - PPQ // 2) * (PPQ // 3) // (PPQ // 2)
    return beat * PPQ + frac


class _Out:
    def __init__(self, swing):
        self.ev, self.sw = [], (swing_tick if swing else (lambda t: t))

    def note(self, start, dur, pitch, vel):
        a, b = self.sw(start), self.sw(start + dur)
        self.ev.append((a, max(a + 10, b - 15), pitch, vel))


def _bass_root(c):
    pc = c.bass if c.bass is not None else c.root
    return 24 + pc if pc >= 4 else 36 + pc          # E1..D#2: the bass's real octave (M5; M4 wrote it an octave up)


def _broken(v):
    """a voicing's notes for broken chords: low, mid, high (3 notes)"""
    v = sorted(v)
    return v[0], v[len(v) // 2], v[-1]


def _arp_pool(v):
    """the arpeggio's notes: the voicing and its notes an octave up, within ~1.5 octaves of the lowest"""
    v = sorted(set(v))
    pool = sorted(set(v + [x + 12 for x in v]))
    return [x for x in pool if x <= v[0] + 19]


def harmony(bars, bpb, style, swing, final_bar):
    """{1..5: [(start, end, pitch, vel)]}"""
    out = {lv: _Out(swing) for lv in range(1, 6)}
    prev2 = None
    for b, row in enumerate(bars):
        t0 = b * bpb * PPQ
        for cell in row:
            beat, length, c = cell[:3]
            if c is None:
                continue
            start, dur = t0 + beat * PPQ, length * PPQ
            if len(cell) > 3:                        # (M5: the voicings as compiled: the FM-1 uses those)
                v1, v2 = list(cell[3]), list(cell[4])
            else:
                v1 = ch.voicing_level1(c)
                v2 = ch.voicing_level2(c, prev2)
            prev2 = v2
            if b == final_bar:                       # the last bar: held, at every level
                for lv in range(1, 6):
                    for p in (v1 if lv <= 2 else v2):
                        out[lv].note(start, dur, p, 64)
                continue
            for p in v1:                             # L1: held
                out[1].note(start, dur, p, 64)
            for lv, v in ((2, v1), (3, v2)):         # L2, L3: the style's rhythm
                if style in RHYTHM:                  # (M8: the table styles, a 16th grid)
                    vv = sorted(v)[1:] if style in UPPER and len(v) >= 4 else v
                    tab = RHYTHM[style][b % len(RHYTHM[style])]
                    cend = start + dur
                    for st, ln, vel in _steps(tab, bpb):
                        t = t0 + st * STEP
                        if t < start or t >= cend:
                            continue
                        for p in vv:
                            out[lv].note(t, min(ln * STEP, cend - t), p, vel)
                    continue
                for k in range(length):
                    bib = beat + k                   # the beat in the bar
                    if style == "waltz":
                        d, vel = (PPQ - 40, 64) if bib % 3 == 0 else (PPQ // 2, 50)
                        for p in v:
                            out[lv].note(start + k * PPQ, d, p, vel)
                    elif style == "shuffle":
                        for p in v:
                            out[lv].note(start + k * PPQ, PPQ // 3, p, 58 if bib % 2 else 50)
                    elif style == "stride":
                        if bib % 2:
                            for p in v:
                                out[lv].note(start + k * PPQ, PPQ // 2, p, 60)
                    elif style == "ballad":
                        for p in v:
                            out[lv].note(start + k * PPQ, PPQ - 40, p, 60 if bib % 2 == 0 else 50)
                    else:                            # eighths
                        for h in range(2):
                            for p in v:
                                out[lv].note(start + k * PPQ + h * PPQ // 2, PPQ // 3, p, 56 if h == 0 else 44)
            lo, mid, hi = _broken(v2)                # L4: broken chords, in eighths
            seq = [lo, mid, hi, mid, hi, mid] if bpb % 3 == 0 else [lo, hi, mid, hi]
            for k in range(length * 2):
                out[4].note(start + k * PPQ // 2, PPQ // 2 + 40, seq[(beat * 2 + k) % len(seq)], 70 if k % 2 == 0 else 58)
            pool = _arp_pool(v2)                     # L5: up and down the pool, in eighths
            cyc = pool + pool[-2:0:-1]
            for k in range(length * 2):
                out[5].note(start + k * PPQ // 2, PPQ // 2 + 60, cyc[k % len(cyc)], 70 if k % 2 == 0 else 56)
    return {lv: o.ev for lv, o in out.items()}


def bass(bars, bpb, style, swing, final_bar):
    """{1..4: [(start, end, pitch, vel)]}"""
    out = {lv: _Out(swing) for lv in range(1, 5)}
    flat = [(b, cell[0], cell[1], cell[2]) for b, row in enumerate(bars) for cell in row if cell[2] is not None]
    for i, (b, beat, length, c) in enumerate(flat):
        start, dur = b * bpb * PPQ + beat * PPQ, length * PPQ
        root = _bass_root(c)
        fifth = root + 7 if root + 7 <= 40 else root - 5
        third = root + (3 if 3 in c.intervals else 4)
        nxt = _bass_root(flat[i + 1][3]) if i + 1 < len(flat) else root
        if b == final_bar:
            for lv in range(1, 5):
                out[lv].note(start, dur, root, 84)
            continue
        out[1].note(start, dur, root, 97)                                    # L1: held
        for k in range(length):                                              # L2: the root in rhythm
            bib = beat + k
            if bpb % 3 == 0:                                                 # (3/4: on 1, held 2 beats, and on 3)
                if bib % 3 == 1 and k > 0:
                    continue
                d = 2 * PPQ if bib % 3 == 0 and k + 1 < length else PPQ
            else:
                d = PPQ
            out[2].note(start + k * PPQ, d - 30, root, 86 if bib % bpb == 0 else 74)
        half = 2                                                             # L3: root and fifth
        for k in range(0, length, half if bpb % 2 == 0 else 1):
            bib = beat + k
            if bpb % 3 == 0:
                p = root if bib % 3 == 0 else fifth if bib % 3 == 2 else None
                if p is None:
                    continue
                d = (2 if bib % 3 == 0 else 1) * PPQ - 30
            else:
                p = root if k == 0 or bib % bpb == 0 else fifth
                d = min(half, length - k) * PPQ - 30
            out[3].note(start + k * PPQ, d, p, 88 if p == root else 76)
        if style in BASS_LINE:                                               # L4: the style's line (M8)
            cend = start + dur
            for st, ln, what, vel in _steps(BASS_LINE[style], bpb):
                t = b * bpb * PPQ + st * STEP
                if t < start or t >= cend:
                    continue
                p = {"R": root, "F": fifth, "O": root + 12, "N": nxt,
                     "A": fifth if nxt == root else nxt + (1 if nxt < root else -1)}[what]
                out[4].note(t, min(ln * STEP, cend - t), p, vel)
        elif style == "boogie":                                              # L4: more motion
            line = [0, 4, 7, 9, 10, 9, 7, 4]
            for k in range(length * 2):
                out[4].note(start + k * PPQ // 2, PPQ // 2, root + line[(beat * 2 + k) % 8], 84 if k % 2 == 0 else 70)
        elif bpb % 3 == 0:                                                   # 3/4: root, fifth, octave
            for k in range(length):
                p = (root, fifth, root + 12)[(beat + k) % 3]
                out[4].note(start + k * PPQ, PPQ - 30, p if p <= 43 else p - 12, 84 if (beat + k) % 3 == 0 else 72)
        else:                                                                # walking quarters into the next root
            walk = [root, third, fifth, nxt + (1 if nxt < root else -1)]
            for k in range(length):
                p = walk[k % 4] if k < length - 1 or length < 2 else walk[3]
                if k == length - 1 and length >= 2 and nxt == root:
                    p = root + 9 if root + 9 <= 43 else fifth             # the same chord next: the sixth
                out[4].note(start + k * PPQ, PPQ - 30, p, 84 if k == 0 else 72)
    return {lv: o.ev for lv, o in out.items()}


def drums(nbars, bpb, style, swing, pickup, section_starts, final_bar):
    """{1..3: [(start, end, drum, vel)]}; the pickup bar is silent; the last bar: a kick and a crash"""
    sw = swing_tick if swing else (lambda t: t)
    out = {1: [], 2: [], 3: []}
    for b in range(nbars):
        t0 = b * bpb * PPQ
        if b == 0 and pickup:
            continue
        if b == final_bar:
            for lv in out:
                out[lv] += [(t0, t0 + 60, KICK, 96), (t0, t0 + 60, CRASH, 90)]
            continue
        hits1, hits2 = [], []
        for k in range(bpb):                                                 # L1: the pulse
            tk = t0 + k * PPQ
            if k == 0 or (bpb == 4 and k == 2):
                hits1.append((tk, KICK, 88 if k == 0 else 80))
            else:
                hits1.append((tk, SNARE, 62 if bpb == 3 else 80))
            hits1.append((tk, RIDE if style == "shuffle" else HAT, 64 if k == 0 else 56))
        for k in range(bpb if style not in GROOVE else 0):                   # L2: the style's groove
            tk = t0 + k * PPQ
            if style == "waltz":
                if k == 0:
                    hits2.append((tk, KICK, 84))
                else:
                    hits2.append((tk, STICK, 58))
                hits2 += [(tk, HAT, 62 if k == 0 else 50), (sw(tk + PPQ // 2), HAT, 36)]
                continue
            if k in (0, 2):
                hits2.append((tk, KICK, 92 if k == 0 else 80))
            else:
                hits2.append((tk, STICK if style == "ballad" else SNARE, 70 if style == "ballad" else 88))
            cym = RIDE if style == "shuffle" else HAT
            hits2 += [(tk, cym, 66 if k % 2 == 0 else 58), (sw(tk + PPQ // 2), cym, 40)]
            if style == "shuffle" and k % 2 == 0:
                hits2.append((sw(tk + PPQ // 2), SNARE, 26))
            if style == "pop" and k == 2:
                hits2.append((tk + PPQ // 2, KICK, 62))
            if style == "train":
                hits2 += [(tk + PPQ // 4, SNARE, 24), (tk + 3 * PPQ // 4, SNARE, 30)]
            if style == "march" and k == 3 and b % 2:
                hits2.append((sw(tk + PPQ // 2), SNARE, 50))
        if style in GROOVE:                                                  # (M8: a table groove)
            g = GROOVE[style][b % len(GROOVE[style])]
            for st in range(bpb * 4):
                for name in GROOVE_ORDER:
                    c = g.get(name, "." * 16)[st % 16]
                    if c != ".":
                        hits2.append((sw(t0 + st * STEP), GROOVE_DRUM[name], GROOVE_VEL[c]))
        bar_no = b + 1 - (1 if pickup else 0)
        hits3 = list(hits2)                                                  # L3: + crashes and fills
        if b in section_starts:
            hits3.append((t0, CRASH, 84))
        if bar_no % 4 == 0:
            base = t0 + (bpb - 1) * PPQ
            hits3 = [h for h in hits3 if h[0] < base]
            hits3 += [(base + i * PPQ // 4, p, 64 + 6 * i) for i, p in enumerate((SNARE, SNARE, TOM_HI, TOM_LO))]
        for lv, hits in ((1, hits1), (2, hits2), (3, hits3)):
            out[lv] += [(t, t + 50, p, v) for t, p, v in hits]
    return out


def canonical(notes):
    """notes [(start, end, pitch, vel)] -> events [(tick, status, pitch, vel)] as the FM-1 makes them (format 2.0; the
    C generator, firmware/app/gen.c, does exactly this): sorted by start, pitch, the louder first, the shorter first;
    one note of a pitch at a start; a note ends where the next of its pitch starts; note-offs before note-ons at a
    tick, then by pitch"""
    seen, ns = set(), []
    for a, e, p, v in sorted(notes, key=lambda n: (n[0], n[2], -n[3], n[1])):
        if (a, p) in seen:
            continue
        seen.add((a, p))
        ns.append([a, e, p, v])
    nxt = {}
    for n in reversed(ns):
        if n[2] in nxt and nxt[n[2]] < n[1]:
            n[1] = nxt[n[2]]
        nxt[n[2]] = n[0]
    ev = [(a, 144, p, v) for a, e, p, v in ns] + [(e, 128, p, 0) for a, e, p, v in ns]
    ev.sort(key=lambda x: (x[0], x[1] == 144, x[2]))
    return [list(x) for x in ev]


def arrange(bars, bpb, pickup, section_starts, styles, swing):
    """every generated level: {"harmony:1".."harmony:5", "bass:1".."bass:4", "drums:1".."drums:3"}"""
    final = len(bars) - 1
    tracks = {}
    for lv, ev in harmony(bars, bpb, styles["harmony"], swing, final).items():
        tracks[f"harmony:{lv}"] = ev
    for lv, ev in bass(bars, bpb, styles["bass"], swing, final).items():
        tracks[f"bass:{lv}"] = ev
    for lv, ev in drums(len(bars), bpb, styles["drums"], swing, pickup, section_starts, final).items():
        tracks[f"drums:{lv}"] = ev
    return tracks
