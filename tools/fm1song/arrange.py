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
from . import chords as ch
from .errors import Diagnostics
from .source import parse_chart_text

PPQ = 480
KICK, STICK, SNARE, HAT, OPEN_HAT, PEDAL_HAT, RIDE, CRASH, TOM_LO, TOM_HI = 36, 37, 38, 42, 46, 44, 51, 49, 45, 50

# style -> what each part does; a song names one (or a part each)
STYLES = {
    "waltz":   dict(harmony="waltz", bass="waltz", drums="waltz"),        # 3/4: oom-pah-pah
    "ballad":  dict(harmony="ballad", bass="ballad", drums="ballad"),     # slow 4/4: held quarters, Alberti
    "shuffle": dict(harmony="shuffle", bass="boogie", drums="shuffle"),   # swung 4/4 (the blues)
    "march":   dict(harmony="stride", bass="two-beat", drums="march"),    # 2-feel 4/4 (the Saints)
    "pop":     dict(harmony="eighths", bass="eighths", drums="pop"),      # straight eighths
    "train":   dict(harmony="eighths", bass="two-beat", drums="train"),   # fast 2-feel (Oh! Susanna)
}

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
    return 36 + pc if pc >= 4 else 48 + pc          # E2..D#3


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
        for beat, length, c in row:
            if c is None:
                continue
            start, dur = t0 + beat * PPQ, length * PPQ
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
                out[4].note(start + k * PPQ // 2, PPQ // 2 + 40, seq[(beat * 2 + k) % len(seq)], 58 if k % 2 == 0 else 48)
            pool = _arp_pool(v2)                     # L5: up and down the pool, in eighths
            cyc = pool + pool[-2:0:-1]
            for k in range(length * 2):
                out[5].note(start + k * PPQ // 2, PPQ // 2 + 60, cyc[k % len(cyc)], 58 if k % 2 == 0 else 46)
    return {lv: o.ev for lv, o in out.items()}


def bass(bars, bpb, style, swing, final_bar):
    """{1..4: [(start, end, pitch, vel)]}"""
    out = {lv: _Out(swing) for lv in range(1, 5)}
    flat = [(b, beat, length, c) for b, row in enumerate(bars) for beat, length, c in row if c is not None]
    for i, (b, beat, length, c) in enumerate(flat):
        start, dur = b * bpb * PPQ + beat * PPQ, length * PPQ
        root = _bass_root(c)
        fifth = root + 7 if root + 7 <= 52 else root - 5
        third = root + (3 if 3 in c.intervals else 4)
        nxt = _bass_root(flat[i + 1][3]) if i + 1 < len(flat) else root
        if b == final_bar:
            for lv in range(1, 5):
                out[lv].note(start, dur, root, 84)
            continue
        out[1].note(start, dur, root, 84)                                    # L1: held
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
        if style == "boogie":                                                # L4: more motion
            line = [0, 4, 7, 9, 10, 9, 7, 4]
            for k in range(length * 2):
                out[4].note(start + k * PPQ // 2, PPQ // 2, root + line[(beat * 2 + k) % 8], 84 if k % 2 == 0 else 70)
        elif bpb % 3 == 0:                                                   # 3/4: root, fifth, octave
            for k in range(length):
                p = (root, fifth, root + 12)[(beat + k) % 3]
                out[4].note(start + k * PPQ, PPQ - 30, p if p <= 55 else p - 12, 84 if (beat + k) % 3 == 0 else 72)
        else:                                                                # walking quarters into the next root
            walk = [root, third, fifth, nxt + (1 if nxt < root else -1)]
            for k in range(length):
                p = walk[k % 4] if k < length - 1 or length < 2 else walk[3]
                if k == length - 1 and length >= 2 and nxt == root:
                    p = root + 9 if root + 9 <= 55 else fifth             # the same chord next: the sixth
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
        for k in range(bpb):                                                 # L2: the style's groove
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
