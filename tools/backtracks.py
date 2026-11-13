#!/usr/bin/env python3
# backtracks.py -- M8 (D046, D047), M9 (D054: 36): the backtracks from ONE table (tools/backtracks.tsv) -> songs/backtracks/<slug>/
# song.yaml + song.mid (a conductor track only: the compiler wants the file; a backtrack has no melody). Every one in C
# major or A minor, kind backtrack, style POP (the FM-1 plays it in the style chosen there), category "Backtracks".
# Licence: TBD (see D017)
#
#   .venv/bin/python -I tools/backtracks.py [--check]
#       writes the folders, compiles each, and reports the sizes, the song store's room and (--check) that every
#       backtrack in every style (WALTZ in 3/4) fits BACKTRACK's part of the song buffer
#
# The form (plan-M8 Part 3), repeats stored once (a chart part a progression): INTRO (the verse's last 4 bars, energy 0),
# VERSE 1 (the verse x2), CHORUS 1 (x2; the chorus progression, or the verse's; energy +1), VERSE 2, CHORUS 2 (+1),
# BRIDGE (4 bars by the rule below, energy 0), CHORUS 3 (+1), ENDING (one bar, I or i, held). A progression shorter
# than 4 bars is repeated to 4 first. Blues (20-22): INTRO (the last 4 bars), 3 rounds (energy 0, +1, +1), ENDING.
# Bridge: major IV V vi vi, or vi IV V V when the verse or chorus starts on IV; minor bVI bVII i i, or iv v bVI bVII
# when the verse starts on bVI; its last bar becomes V (minor: V, the harmonic minor's).
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from fm1song import compile_song, read_compiled          # noqa: E402
from fm1song import arrange as arr                       # noqa: E402
from fm1song import binformat as bf                      # noqa: E402
from fm1song.smf import meta_tempo, meta_timesig, write_midi   # noqa: E402
from fm1song.source import song_from_data                # noqa: E402

TABLE = ROOT / "tools/backtracks.tsv"
OUT = ROOT / "songs/backtracks"
FLAT = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]
DEGREE = {"I": 0, "II": 2, "III": 4, "IV": 5, "V": 7, "VI": 9, "VII": 11}
ROOM = 196608 - 83712          # BACKTRACK's part of the song buffer (firmware: the synth's lines with the delay halved)
TEMPO = 100
# M9 (D052): every layer has 4 levels, the tiers of the style's table (tools/band_styles.txt): the FM-1 plays tier
# complexity (1..3) in verses, intros and bridges, and one tier up in choruses
TIER_TEXT = {1: "tier 1: the sparest", 2: "tier 2", 3: "tier 3", 4: "tier 4: the busiest (choruses at complexity 3)"}


def chord(numeral, tonic):
    """'bVII' -> 'Bb' (tonic C), 'iv' -> 'Dm' (tonic A), 'ii7' -> 'Dm7', 'Imaj7' -> 'Cmaj7', 'I7' -> 'C7', 'ii7b5' -> 'Bm7b5' (tonic A)"""
    m = re.fullmatch(r"(b?)([IViv]+)(maj7|7b5|7)?", numeral)
    if not m:
        raise SystemExit(f"backtracks: numeral {numeral!r}?")
    flat, deg, ext = m.groups()
    pc = (tonic + DEGREE[deg.upper()] - (1 if flat else 0)) % 12
    minor = deg.islower()
    # (M9: "ii7b5" = the half-diminished chord, m7b5; "i7" / "iv7" = minor sevenths)
    q = ("m" if minor else "") + ({"maj7": "maj7", "7": "7"}.get(ext or "", "") if not minor else
                                   {"7": "7", "7b5": "7b5"}.get(ext or "", ""))
    if ext == "7b5" and not minor:
        raise SystemExit(f"backtracks: numeral {numeral!r}: 7b5 is for a minor numeral (ii7b5)")
    return FLAT[pc] + q


def rows():
    out = []
    for line in TABLE.read_text().splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        num, name, verse, chorus, mode = line.split("\t")
        assert len(name) <= 15, name
        out.append(dict(number=int(num), name=name, verse=verse.split(), chorus=None if chorus == "-" else chorus.split(),
                        mode=mode))
    return out


def slug_of(r):
    return f"bt{r['number']:02d}-" + re.sub(r"[^a-z0-9]+", "-", r["name"].lower()).strip("-")


def at_least4(p):
    while len(p) < 4:
        p = p + p
    return p


def form_of(r):
    """[(section name, role, energy, [numerals])] in order"""
    v = at_least4(r["verse"])
    if r["mode"] in ("blues", "minorblues"):
        return [("Intro", "intro", 0, v[-4:]), ("Round 1", "verse", 0, v), ("Round 2", "chorus", 1, v),
                ("Round 3", "chorus", 1, v), ("Ending", "ending", 0, ["i" if r["mode"] == "minorblues" else "I"])]
    c = at_least4(r["chorus"] or r["verse"])
    if r["mode"] == "major":
        br = ["vi", "IV", "V", "V"] if v[0] == "IV" or c[0] == "IV" else ["IV", "V", "vi", "V"]
        end = "I"
    else:
        br = ["iv", "v", "bVI", "V"] if v[0] == "bVI" else ["bVI", "bVII", "i", "V"]
        end = "i"
    return [("Intro", "intro", 0, v[-4:]), ("Verse 1", "verse", 0, v + v), ("Chorus 1", "chorus", 1, c + c),
            ("Verse 2", "verse", 0, v), ("Chorus 2", "chorus", 1, c), ("Bridge", "bridge", 0, br),
            ("Chorus 3", "chorus", 1, c), ("Ending", "ending", 0, [end])]


def song_data(r, style="pop", bpb=4):
    minor = r["mode"] in ("minor", "minorblues")
    tonic = 9 if minor else 0
    form, chart, bar, parts = [], {}, 1, {}
    for name, role, energy, nums in form_of(r):
        text = "| " + " | ".join(chord(n, tonic) for n in nums) + " |"
        part = parts.get(text)
        if part is None:                                 # (repeats stored once: a chart part per progression)
            part = parts[text] = f"p{len(parts) + 1}"
            chart[part] = text
        form.append({"section": name, "bars": f"{bar}-{bar + len(nums) - 1}" if len(nums) > 1 else str(bar),
                     "chart": part, "role": role, "energy": energy})
        bar += len(nums)
    numerals = " ".join(r["verse"]) + (" | " + " ".join(r["chorus"]) if r["chorus"] else "")
    return {
        "format": "fm1-song/1", "slug": slug_of(r), "title": r["name"], "category": "Backtracks", "kind": "backtrack",
        "backtrack": {"number": r["number"], "numerals": numerals,
                      "scale": "blues" if "blues" in r["mode"] else "minor" if minor else "major"},
        "credits": {"music": "a common chord progression (not protectable)",
                    "arrangement": "FM-1 Accompaniment project (tools/backtracks.py)"},
        "rights": {"composition": "a chord progression: not protectable", "arrangement_license": "TBD",
                   "generated": "tools/backtracks.py"},
        "music": {"key": "Am" if minor else "C", "time_signature": f"{bpb}/4", "tempo_bpm": TEMPO, "pickup_beats": 0,
                  "feel": "swing" if style == "shuffle" else "straight", "style": style},
        "form": form, "chart": chart,
        "layers": {"drums": {"channel": 10, "variants": dict(TIER_TEXT)},
                   "bass": {"channel": 3, "program": 34, "variants": dict(TIER_TEXT)},
                   "harmony": {"channel": 2, "program": 5, "variants": dict(TIER_TEXT)}},
    }


def midi_bytes(bpb=4):
    return write_midi(480, [("conductor", [(0, meta_tempo(round(60e6 / TEMPO))), (0, meta_timesig(bpb, 4))])])


def yaml_text(d):
    import yaml
    head = ("# Song source file, format fm1-song/1 (docs/song-format.md). A backtrack (M8, D046).\n"
            "# GENERATED by tools/backtracks.py from tools/backtracks.tsv: edit those, not this file.\n")
    return head + yaml.safe_dump(d, sort_keys=False, width=120)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="every backtrack in every style fits BACKTRACK's room")
    a = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    total, blocks, worst = 0, 0, (0, "")
    for r in rows():
        d = OUT / slug_of(r)
        d.mkdir(exist_ok=True)
        data = song_data(r)
        (d / "song.yaml").write_text(yaml_text(data))
        (d / "song.mid").write_bytes(midi_bytes())
        blob = compile_song(song_from_data(data, midi_bytes())[0])
        c = read_compiled(blob)
        assert c["version"] == "2.2" and c["kind"] == "backtrack" and not c["unknown_chunks"]
        total += len(blob)
        blocks += (32 + len(blob) + 511) // 512
        if a.check:                                      # (M9: every style, as the FM-1's restyle makes it)
            for st in arr.STYLE_NAMES:
                n = bf.need_bytes(bf.restyled(c, st, 3 if st == "waltz" else 4))
                worst = max(worst, (n, f"{slug_of(r)} {st}"))
        print(f"{r['number']:2d} {slug_of(r):24s} {c['bar_count']:3d} bars  {len(blob):5d} B")
    print(f"{len(rows())} backtracks: {total:,} B, {blocks} blocks of the song store's 552")
    if a.check:
        print(f"the largest need in any style: {worst[0]:,} B ({worst[1]}); BACKTRACK's room {ROOM:,} B: "
              f"{'fits' if worst[0] <= ROOM else 'DOES NOT FIT'}; two copies (M9's seamless switch, half of the room "
              f"each, {ROOM // 2:,} B): {'fit' if worst[0] <= ROOM // 2 else 'DO NOT FIT'}")
        if worst[0] > ROOM // 2:
            sys.exit(1)


if __name__ == "__main__":
    main()
