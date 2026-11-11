#!/usr/bin/env python3
# m7_test.py -- Milestone M7: headless emulator tests of the new features (built on tools/m5_test.py's runner, its
# prepared flash images and presses, and tools/m6_test.py's). Never touches the FM-1. Licence: TBD (see D017)
#
#   .venv/bin/python -I tools/m7_test.py [--only m7p1,m7p2,..] [--pkg PKG] [--out DIR]   -> build/emu/m7/report.md
#
#   m7p1  learning mode: the NOT IMPLEMENTED pop-up (every learning screen's free buttons), a song's start level
#         (format 2.1: Zombie's bass on L2), Rockin' gone
#   m7p2  the SYNTH UI on the parameter table: main screen, edit pages, quick knobs, controls, help, settings
#   m7p3  the effects: each alone and all together, silence after release, no CPU at 0
#   m7p4  the drums: patterns, kits, PLAY, the menu, a change on the next bar, drum volume
#   m7p5  engines and presets: 30 presets, each sounds, level-matched, PHASE
#   m7p6  the button census (both modes, every screen, every button), the power-on rescues, 24 quick HOME taps, CPU
# The full suite (Part 6) is this plus tools/m6_test.py, tools/m5_test.py (M2..M5's checks) and tools/m51_test.py.
import argparse
import os
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import m5_test as m5                     # noqa: E402
import m6_test as m6                     # noqa: E402

V_LIST, V_PLAY, V_HELP, V_SETTINGS, V_VIEWSET, V_HINT, V_MODE, V_SYNTH, V_WIPE = range(9)
M7_FIELDS = ("popups popup_open popup_btn synth_page drum_on drum_pattern drum_kit drum_hits drum_voices_max drum_bar "
             "fx_active synth_engine synth_bpm synth_level levels").split()
B = {n: i for i, n in enumerate("FX SCL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCTDN OCTUP".split())}
O = m5.OPEN_MS
SYNTH = m6.SYNTH

_stats6 = m5.stats


def stats7(ram):
    """m6_test's stats, plus M7's 16 words after M6's (player.h)"""
    s = _stats6(ram)
    if s is None:
        return s
    i = ram.find(b"PLST")
    while i >= 0 and struct.unpack_from("<I", ram, i + 4)[0] not in (3, 4):
        i = ram.find(b"PLST", i + 1)
    v = struct.unpack_from("<16I", ram, i + m5.STATS.size + m5.STATS4.size + 64)
    s.update(zip(M7_FIELDS, v))
    return s


m5.stats = stats7


def level(s, layer):
    return s["levels"] >> 4 * layer & 15


# --------------------------------------------------------------------------------------------- part 1
def test_m7p1(all_songs, rep):
    lines, st, check = m5.checker("M7 part 1: learning mode")
    slugs = [c["slug"] for c in all_songs]
    g = slugs.index("amazing-grace")
    # the pop-up on the song list (FX: no job there), closed by PLAY, which does nothing else (no song opens)
    a = m5.run("m7p1-popup-list", 5200, m5.press_raw(4400, "FX"))
    b = m5.run("m7p1-popup-close", 5600, m5.press_raw(4400, "FX") + m5.press_raw(4900, "PLAY"))
    check(a["stats"]["popup_open"] == 1 and a["stats"]["popups"] == 1 and a["stats"]["popup_btn"] == B["FX"] and
          a["stats"]["ui_view"] == V_LIST,
          "the song list, FX (no job there): the NOT IMPLEMENTED pop-up (m7p1-popup-list.png)")
    check(b["stats"]["popup_open"] == 0 and b["stats"]["ui_view"] == V_LIST and b["stats"]["popups"] == 1,
          "PLAY closes it and does nothing else (no song opened; m7p1-popup-close.png)")
    # every learning screen: each free button opens it, each button with a job does not
    free = {"list": ["FX", "SCL", "ENV", "LFO", "SAVE", "ARP", "SEQ", "REC"],
            "settings": ["FX", "SCL", "ENV", "LFO", "SAVE", "ARP", "SEQ", "REC"]}
    bad = []
    for scr, pre in (("list", []), ("settings", m5.press_raw(4400, "GLO") + m5.press_raw(4600, "GLO"))):
        for k, name in enumerate(free[scr]):
            r = m5.run(f"m7p1-free-{scr}-{name}", 5400, pre + m5.press_raw(4900, name))
            if not (r["stats"]["popup_open"] == 1 and r["stats"]["popup_btn"] == B[name]):
                bad.append((scr, name))
    check(not bad, f"the list's and the settings' free buttons ({', '.join(free['list'])}): each opens it {bad or ''}")
    # held since power-on: never
    h = m5.run("m7p1-popup-held", 5200, ["0:FX!", "5100:FX^!"])
    check(h["stats"]["popups"] == 0, "FX held since power-on, released on the list: no pop-up")
    # the play screen: every button has a job (a song with every layer) -> no pop-up from any of them
    jobs = ["FX", "SCL", "ENV", "LFO", "EDIT", "SAVE", "ARP", "SEQ", "REC", "PLAY"]
    ps = m5.run("m7p1-play-jobs", m5.PLAY_MS + 6000, m5.open_song(g) + [x for k, n in enumerate(jobs)
                                                                         for x in m5.press_raw(O + 600 + 300 * k, n)])
    check(ps["stats"]["popups"] == 0 and ps["stats"]["ui_view"] in (V_PLAY, V_HINT),
          "the play screen (a song with all four layers): every button has a job, no pop-up")
    # HOME held 2 s works with it open; the keys play with it open; encoders never open it
    hh = m5.run("m7p1-popup-home", 8200, m5.press_raw(4400, "FX") + m6.hold_home(4700))
    check(hh["stats"]["ui_view"] == V_MODE and hh["stats"]["mode_switches"] == 1,
          "the pop-up open, HOME held 2 s: the mode screen")
    kk = m5.run("m7p1-popup-keys", 6000, m5.press_raw(4400, "FX") + ["4700:C4", "5400:C4^", "4600:e1:3", "4650:e6:2"])
    snd = m5.rms(kk["left"][int(4.8 * m5.RATE):int(5.3 * m5.RATE)])
    check(kk["stats"]["popup_open"] == 1 and kk["stats"]["popups"] == 1 and snd > 50,
          f"with it open the keys play (rms {snd:.0f}); encoders turned: it stays, none opened another")
    en = m5.run("m7p1-enc", 5600, ["4400:e1:3", "4500:e6:-2", "4600:e2:4", "4700:e5:1"])
    check(en["stats"]["popups"] == 0, "encoders with no job (ALGO, PRESETS, K1..K4 on the list): no pop-up")
    # format 2.1: Zombie's bass opens on L2, the others on L1 (the private songs, when on this machine)
    if m6.PRIV and any(p.name == "zombie" for p in m6.PRIV):
        fl = m6.priv_flash(all_songs)
        cs = m6.all_compiled()
        z = [c["slug"] for c in cs].index("zombie")
        zr = m5.run("m7p1-zombie-start", O + 2500, ([f"4150:e0:{z}"] if z else []) + m5.press_raw(O, "PLAY"), flash=fl)
        s = zr["stats"]
        check(cs[z]["version"] == "2.1" and level(s, 1) == 2 and level(s, 0) == 1 and level(s, 2) == 1 and
              level(s, 3) == 1, f"Zombie (format {cs[z]['version']}): bass opens on L{level(s, 1)}, drums L{level(s, 0)}, "
              f"harmony L{level(s, 2)}, melody L{level(s, 3)}")
        check("rockin-free-world" not in [c["slug"] for c in cs], "Rockin' in the Free World is gone from the songs")
        gr = m5.run("m7p1-grace-start", O + 2500, m5.open_song(g))
        check(gr["stats"]["levels"] & 0xFFFF == 0x1111, "a 2.0 song opens on L1 everywhere")
    rep += lines
    return st["ok"]


PARTS = {"m7p1": test_m7p1}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    ap.add_argument("--pkg")
    ap.add_argument("--out")
    ap.add_argument("--jobs", type=int, default=3)
    a = ap.parse_args()
    m5.OUT = Path(a.out).resolve() if a.out else ROOT / "build/emu/m7"
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
    text = [f"# M7 emulator tests\n\nPackage: {m5.PKG}, sha256 {sha}\n"]
    for k in reports:
        text += reports[k]
    text.append(f"\n**{'ALL PASS' if all(results.values()) else 'FAILURES: ' + ', '.join(k for k, v in results.items() if not v)}**\n")
    (m5.OUT / "report.md").write_text("\n".join(text))
    print("\n".join(text))
    sys.exit(0 if all(results.values()) else 1)


if __name__ == "__main__":
    main()
