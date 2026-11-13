#!/usr/bin/env python3
# m9_test.py -- Milestone M9: headless emulator tests (built on tools/m8_test.py's runner and stats; same structure).
# Never touches the FM-1.
# Licence: TBD (see D017)
#
#   .venv/bin/python -I tools/m9_test.py [--only m9p1,m9p2,..] [--pkg PKG] [--out DIR]   -> build/emu/m9/report.md
import argparse
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import m5_test as m5                     # noqa: E402
import m6_test as m6                     # noqa: E402
import m7_test as m7                     # noqa: E402
import m8_test as m8                     # noqa: E402

O = m5.OPEN_MS
PRESETS = m8.PRESETS
V_LIST, V_PLAY, V_HELP, V_SETTINGS, V_MODE, V_SYNTH, V_SETUP = m8.V_LIST, m8.V_PLAY, m8.V_HELP, m8.V_SETTINGS, \
    m8.V_MODE, m8.V_SYNTH, m8.V_SETUP
btrun, bt_flash, compiled_bt, BTS = m8.btrun, m8.bt_flash, m8.compiled_bt, m8.BTS


# --------------------------------------------------------------------------------------------- part 0 (measure)
# m9p0 needs the TEMPORARY measuring build: git apply tools/patches/m9-part0-measure.patch (firmware/app/bt.c, player.h),
# build, run `m9_test.py --only m9p0`, then git checkout those two files. The committed firmware has none of it.
_stats8 = m5.stats


def stats9_temp(ram):
    """Part 0 only: the 80 temporary words after M8's 18"""
    s = _stats8(ram)
    if s is None:
        return s
    i = ram.find(b"PLST")
    while i >= 0 and struct.unpack_from("<I", ram, i + 4)[0] not in (3, 4):
        i = ram.find(b"PLST", i + 1)
    s["m9"] = struct.unpack_from("<80I", ram, i + m5.STATS.size + m5.STATS4.size + 128 + 18 * 4)
    return s


def test_m9p0(all_songs, rep):
    lines, st, check = m5.checker("M9 part 0: measure first")
    m5.stats = stats9_temp
    names = m8.BT_STYLE_NAMES if hasattr(m8, "BT_STYLE_NAMES") else ["POP", "ROCK", "INDIE POP", "BALLAD", "SHUFFLE",
                                                                     "STRIDE", "WALTZ", "COUNTRY", "BOSSA NOVA", "FUNK", "REGGAE"]
    from concurrent.futures import ThreadPoolExecutor
    progs = (28, 9, 20, 1, 5)

    def one(p):
        sc = ([f"4300:e2:{p - 1}"] if p > 1 else []) + m5.press_raw(4700, "PLAY") + ["9000:e3:1"]
        r = btrun(f"m9p0-{p}", 14000, sc)
        return p, r
    with ThreadPoolExecutor(3) as ex:
        res = list(ex.map(one, progs))
    for p, r in res:
        m = r["stats"]["m9"]
        lines.append(f"  - backtrack {p}: all 11 styles in {m[78]} ms, the player made {m[79]} blocks meanwhile "
                     f"(expected {m[78] * 44.1 / 256:.0f}); render_max {r['stats']['render_max']} of budget "
                     f"{r['stats']['budget']}")
        for k, n in enumerate(names):
            a = m[k * 6:k * 6 + 6]
            lines.append(f"      {n:10s} open {a[0]:4d} ms, restyle {a[1]:4d} ms, build 3 layers {a[2]:4d} ms, rebuild 3 "
                         f"levels {a[3]:4d} ms, need {a[4]:6,d} B, err {a[5]}")
    rep += lines
    st["res"] = res
    return st["ok"]


PARTS = {"m9p0": test_m9p0}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    ap.add_argument("--pkg")
    ap.add_argument("--out")
    ap.add_argument("--jobs", type=int, default=3)
    a = ap.parse_args()
    m5.OUT = Path(a.out).resolve() if a.out else ROOT / "build/emu/m9"
    if a.pkg:
        m5.PKG = Path(a.pkg).resolve()
    m5.OUT.mkdir(parents=True, exist_ok=True)
    only = set(a.only.split(",")) if a.only else set(PARTS) - {"m9p0"}
    all_songs = m5.songs()
    m5.prepare_flash()
    bt_flash()
    reports, results = {}, {}
    from concurrent.futures import ThreadPoolExecutor
    with ThreadPoolExecutor(a.jobs) as pool:
        futs = {}
        for k, fn in PARTS.items():
            if k in only:
                reports[k] = []
                futs[k] = pool.submit(fn, all_songs, reports[k])
        for k, f in futs.items():
            results[k] = f.result()
    sha = subprocess.run(["sha256sum", str(m5.PKG)], capture_output=True, text=True).stdout.split()[0]
    text = [f"# M9 emulator tests\n\nPackage: {m5.PKG}, sha256 {sha}\n"]
    for k in reports:
        text += reports[k]
    text.append(f"\n**{'ALL PASS' if all(results.values()) else 'FAILURES: ' + ', '.join(k for k, v in results.items() if not v)}**\n")
    (m5.OUT / "report.md").write_text("\n".join(text))
    print("\n".join(text))
    sys.exit(0 if all(results.values()) else 1)


if __name__ == "__main__":
    main()
