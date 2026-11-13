#!/usr/bin/env python3
# m8_test.py -- Milestone M8: headless emulator tests of BACKTRACK mode and the renames (built on tools/m5_test.py's
# runner, its prepared flash images and presses, and tools/m6_test.py's / m7_test.py's). Never touches the FM-1.
# Licence: TBD (see D017)
#
#   .venv/bin/python -I tools/m8_test.py [--only m8p0,m8p1,..] [--pkg PKG] [--out DIR]   -> build/emu/m8/report.md
#
#   m8p0  measure (Part 0): the song buffer's use in LEARN for the largest song, LEARN's busiest song's CPU (EAS),
#         SYNTH's 4 voices of the heaviest preset with and without delay / reverb
import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools/fm1song/.."))
import m5_test as m5                     # noqa: E402
import m6_test as m6                     # noqa: E402
import m7_test as m7                     # noqa: E402
from fm1song import binformat as bf      # noqa: E402

O = m5.OPEN_MS
PRESETS = [l.split('"')[1] for l in (ROOT / "firmware/app/vsynth.c").read_text().splitlines()
           if l.strip().startswith('PR("')]
KEYS4 = ("C4", "E4", "G4", "B4")


def need_bytes(c, blob=None):
    """the song buffer a compiled 2.x song needs in LEARN (library.c: the largest level of each layer, 2 events a note,
    8 bytes an event); 1.x: its decoded events"""
    total = 0
    for name, ly in c["layers"].items():
        most = 0
        for v, evs in ly["variants"].items():
            most = max(most, sum(1 for e in evs if e[1] == 0x90) * 2)
        total += most
    return total * 8


# --------------------------------------------------------------------------------------------- part 0
def test_m8p0(all_songs, rep):
    lines, st, check = m5.checker("M8 part 0: measure first")
    cs = m6.all_compiled()
    needs = sorted(((need_bytes(c), c["slug"]) for c in cs), reverse=True)
    lines.append("  - the song buffer (192 KiB = 196,608 B) in LEARN, the largest level of each layer: " +
                 ", ".join(f"{s} {n:,}" for n, s in needs))
    fl = m6.priv_flash(all_songs) if m6.PRIV else "songs"
    from concurrent.futures import ThreadPoolExecutor

    def learn(i):
        c = cs[i]
        sc = ([f"4150:e0:{i}"] if i else []) + m5.press_raw(O, "PLAY") + ["1500:pot:4096"]
        sc += [f"{O + 800}:e2:2", f"{O + 900}:e3:3", f"{O + 1000}:e4:4"]      # every layer at its top level
        r = m5.run(f"m8p0-learn-{c['slug']}", m5.PLAY_MS + 30000, sc + m5.press_raw(m5.PLAY_MS, "PLAY"), flash=fl)
        s = r["stats"]
        return c["slug"], s["render_max"] / s["budget"] if s["budget"] else 9, max(s["voices"]), s["levels"]
    with ThreadPoolExecutor(3) as ex:
        res = list(ex.map(learn, range(len(cs))))
    res.sort(key=lambda x: -x[1])
    lines.append("  - LEARN, every layer at its top level, 30 s, EAS render / block: " +
                 ", ".join(f"{s} {v * 100:.0f} % ({n} voices)" for s, v, n, _ in res))
    busiest = res[0]

    def synth(job):
        name, extra = job
        i = PRESETS.index("MIAMI PAD")
        sc = [f"4600:e6:{i}"] + extra + [f"6500:{k}" for k in KEYS4] + [f"9500:{k}^" for k in KEYS4]
        r = m7.syrun(f"m8p0-synth-{name}", 10000, sc)
        s = r["stats"]
        return name, s["synth_render_max"] / s["budget"] if s["budget"] else 9, s["fx_active"], s["synth_voices_max"]
    jobs = [("as-made", []), ("no-delay-reverb", ["5000:e4:-127", "5300:e5:-127"]),
            ("delay-reverb-top", ["5000:e4:127", "5300:e5:127"])]
    with ThreadPoolExecutor(3) as ex:
        sy = list(ex.map(synth, jobs))
    # every effect at its top (FX pages), 4 voices
    i = PRESETS.index("MIAMI PAD")
    r = m7.syrun("m8p0-synth-all-fx", 19500, [f"4500:e6:{i}"] + m7.fx_max(4700) + [f"17000:{k}" for k in KEYS4]
                 + [f"19100:{k}^" for k in KEYS4])
    s = r["stats"]
    sy.append(("every-effect-top", s["synth_render_max"] / s["budget"], s["fx_active"], s["synth_voices_max"]))
    lines.append("  - SYNTH, MIAMI PAD x 4 voices, synth render / block: " +
                 ", ".join(f"{n} {v * 100:.0f} % (fx 0x{fx:02x}, {nv} voices)" for n, v, fx, nv in sy))
    worst_synth = max(v for _, v, _, _ in sy)
    lines.append(f"  - sum, the busiest song's EAS ({busiest[0]}) + 4 voices with every effect: "
                 f"{(busiest[1] + worst_synth) * 100:.0f} % of a block (limit 60 %)")
    check(all(nv == 4 for _, _, _, nv in sy), "4 synth voices in every SYNTH run")
    rep += lines
    st["numbers"] = dict(needs=needs, learn=res, synth=sy)
    return st["ok"]


PARTS = {"m8p0": test_m8p0}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    ap.add_argument("--pkg")
    ap.add_argument("--out")
    ap.add_argument("--jobs", type=int, default=3)
    a = ap.parse_args()
    m5.OUT = Path(a.out).resolve() if a.out else ROOT / "build/emu/m8"
    if a.pkg:
        m5.PKG = Path(a.pkg).resolve()
    m5.OUT.mkdir(parents=True, exist_ok=True)
    only = set(a.only.split(",")) if a.only else set(PARTS)
    all_songs = m5.songs()
    m5.prepare_flash()
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
    text = [f"# M8 emulator tests\n\nPackage: {m5.PKG}, sha256 {sha}\n"]
    for k in reports:
        text += reports[k]
    text.append(f"\n**{'ALL PASS' if all(results.values()) else 'FAILURES: ' + ', '.join(k for k, v in results.items() if not v)}**\n")
    (m5.OUT / "report.md").write_text("\n".join(text))
    print("\n".join(text))
    sys.exit(0 if all(results.values()) else 1)


if __name__ == "__main__":
    main()
