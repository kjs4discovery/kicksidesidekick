#!/usr/bin/env python3
# guitar_check.py -- M3 part 6: checks our guitar chord table (firmware/app/guitar.h) on the laptop. Licence: TBD (see D017)
#
#   guitar_check.py              every shape's notes are its chord's (standard tuning), its lowest note the root or
#                                the slash bass; and every chord of every song, in all 12 keys, finds a shape the way
#                                ui.c guitar_find looks (open by pitch class, else a barre, else without the slash)
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from fm1song import compile_folder, read_compiled   # noqa: E402
from fm1song.chords import QUALITIES, parse_chord   # noqa: E402

TUNING = (40, 45, 50, 55, 59, 64)              # E2 A2 D3 G3 B3 E4


def table():
    src = (ROOT / "firmware/app/guitar.h").read_text()
    opens = re.findall(r'\{"([^"]+)", "([x0-9]{6})", "([0-9]{6})"\}', src)
    barres = re.findall(r'\{"([a-z0-9]*)", ([56]), "([x0-9]{6})", "([0-9]{6})"\}', src)
    return opens, [(q, int(s), f, g) for q, s, f, g in barres]


def notes(frets, base=0):
    return [TUNING[i] + base + int(f) for i, f in enumerate(frets) if f != "x"]


def check_shape(name, frets):
    c = parse_chord(name)
    ns = notes(frets)
    pcs = {n % 12 for n in ns}
    want = {(c.root + i) % 12 for i in c.intervals}
    low = min(ns) % 12
    bass = c.bass if c.bass is not None else c.root
    extra = pcs - want - ({c.bass} if c.bass is not None else set())
    missing = {(c.root + i) % 12 for i in c.intervals[:3]} - pcs - ({(c.root + 7) % 12} if len(c.intervals) > 3 else set())
    return not extra and not missing and low == bass, extra, missing, low, bass


def find(sym, opens, barres):
    c = parse_chord(sym)
    if c is None:
        return "nc"
    q = re.match(r"^[A-G][#b]?(.*?)(?:/.*)?$", sym).group(1)
    for pass_ in (0, 1):
        for name, f, g in opens:
            e = parse_chord(name)
            eq = re.match(r"^[A-G][#b]?(.*?)(?:/.*)?$", name).group(1)
            if e.root == c.root and eq == q and e.bass == (None if pass_ else c.bass):
                return "open" if not pass_ or c.bass is None else "open, no slash"
        if c.bass is None or pass_:
            if any(bq == q for bq, *_ in barres):
                return "barre" if c.bass is None else "barre, no slash"
        if c.bass is None:
            return None
    return None


def main():
    opens, barres = table()
    bad = 0
    for name, frets, _ in opens:
        ok, extra, missing, low, bass = check_shape(name, frets)
        if not ok:
            bad += 1
            print(f"FAIL {name} {frets}: extra {extra}, missing {missing}, lowest {low} (want {bass})")
    for q, string, frets, _ in barres:
        root = (4 if string == 6 else 9) + 1          # any root: at fret 1
        sym = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"][root % 12] + q
        f = "".join(c if c == "x" else str(int(c) + 1) for c in frets)
        ok, extra, missing, low, bass = check_shape(sym, f)
        if not ok:
            bad += 1
            print(f"FAIL barre {q!r} on string {string} ({sym} {f}): extra {extra}, missing {missing}")
    print(f"{len(opens)} open shapes, {len(barres)} barre templates: {'all match their chords' if not bad else f'{bad} FAIL'}")
    names = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
    syms = set()
    for f in sorted(p for p in (ROOT / "songs").iterdir() if (p / "song.yaml").is_file()):
        c = read_compiled(compile_folder(f)[0])
        for ch in c["chords"]:
            if ch["symbol"] != "N.C.":
                syms.add(ch["symbol"])
    total, kinds, none = 0, {}, set()
    for sym in sorted(syms):
        m = re.match(r"^([A-G][#b]?)(.*?)(?:/([A-G][#b]?))?$", sym)
        r = parse_chord(sym).root
        b = parse_chord(sym).bass
        for t in range(12):
            s = names[(r + t) % 12] + m.group(2) + ("/" + names[(b + t) % 12] if b is not None else "")
            k = find(s, opens, barres)
            total += 1
            kinds[k] = kinds.get(k, 0) + 1
            if k is None:
                none.add(s)
    print(f"the songs' {len(syms)} chords in all 12 keys ({total}): " +
          ", ".join(f"{k or 'NO SHAPE'} {n}" for k, n in sorted(kinds.items(), key=lambda x: -x[1])))
    if none:
        print("no shape: " + " ".join(sorted(none)))
    return 1 if bad or none else 0


if __name__ == "__main__":
    sys.exit(main())
