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


# --------------------------------------------------------------------------------------------- stats
# Part 0's measuring build (tools/patches/m9-part0-measure.patch) is not run from here any more; its numbers are in
# docs/step-M9-part0-measure.md.
M9_FIELDS = "countins fold_shown btn_lit btn_dim".split()
_stats8 = m5.stats


def stats9(ram):
    """m8_test's stats, plus M9's words after M8's 18 (player.h)"""
    s = _stats8(ram)
    if s is None:
        return s
    i = ram.find(b"PLST")
    while i >= 0 and struct.unpack_from("<I", ram, i + 4)[0] not in (3, 4):
        i = ram.find(b"PLST", i + 1)
    v = struct.unpack_from(f"<{len(M9_FIELDS)}I", ram, i + m5.STATS.size + m5.STATS4.size + 128 + 18 * 4)
    s.update(zip(M9_FIELDS, v))
    return s


m5.stats = stats9
V_ALL = (V_PLAY,)


# --------------------------------------------------------------------------------------------- part 1
def test_m9p1(all_songs, rep):
    lines, st, check = m5.checker("M9 part 1: the count-in only after PLAY (stopped or paused), never at a loop wrap")
    from concurrent.futures import ThreadPoolExecutor
    P = m5.press_raw
    fast = ["4450:e0:150"]                                   # 250 BPM: a bar 0.96 s

    def bt_knobs(structure):
        # progression 4 (POP 3), LOOP (0) or FULL (2); 2 bars after the music starts: K4 +1 -1 +1 -1, K1-K3 turns
        sc = ["4300:e2:3"] + (["4350:e4:2"] if structure == 2 else []) + fast + P(4700, "PLAY")
        t = 8000
        for d in (1, -1, 1, -1, 2, -2):
            sc.append(f"{t}:e5:{d}")
            t += 450
        for enc in (2, 3, 4):
            for d in (1, -1):
                sc.append(f"{t}:e{enc}:{d}")
                t += 450
        return sc

    def bt_pause():
        # LOOP (8 bars): pause in the 3rd bar, PLAY again, let the loop wrap 3 times
        return ["4300:e2:3"] + fast + P(4700, "PLAY") + P(8000, "PLAY") + P(8500, "PLAY")

    def bt_off(pause):
        # started once (stopped at once by HOME: its count-in is the only one allowed), then the count-in OFF (ARP) and
        # PLAY: the loop (8 bars) wraps; with pause: pause in the 3rd bar and PLAY again
        sc = ["4300:e2:3"] + fast + P(4700, "PLAY") + P(4760, "HOME") + P(5200, "ARP") + P(5500, "PLAY")
        if pause:
            sc += P(8500, "PLAY") + P(9000, "PLAY")
        else:
            t = 8000
            for d in (1, -1, 1, -1):
                sc.append(f"{t}:e5:{d}")
                t += 450
        return sc

    def sl_song_loop(off=False):
        i = [c["slug"] for c in all_songs].index("happy-birthday")
        return m5.open_song(i) + ["5000:e0:150", "5100:e1:3"] + (P(5200, "ARP") if off else []) + P(5300, "SEQ") + \
            P(6000, "PLAY")

    def sl_bars_pause():
        i = [c["slug"] for c in all_songs].index("happy-birthday")
        return (m5.open_song(i) + ["5000:e0:150", "5100:e1:1"] + P(5300, "SEQ") + P(5500, "SEQ") + P(5700, "SEQ") +
                P(5900, "SEQ") + P(6200, "PLAY") + P(9600, "PLAY") + P(10200, "PLAY"))

    jobs = {
        "a-loop": lambda: btrun("m9p1-a-loop", 21000, bt_knobs(0)),
        "a-full": lambda: btrun("m9p1-a-full", 21000, bt_knobs(2)),
        "b-pause": lambda: btrun("m9p1-b-pause", 36000, bt_pause()),
        "c-song-loop": lambda: m5.run("m9p1-c-song-loop", 27000, sl_song_loop()),
        "c-bars-pause": lambda: m5.run("m9p1-c-bars-pause", 27000, sl_bars_pause()),
        "d-off-loop": lambda: btrun("m9p1-d-off-loop", 24000, bt_off(False)),
        "d-off-pause": lambda: btrun("m9p1-d-off-pause", 30000, bt_off(True)),
        "d-off-song": lambda: m5.run("m9p1-d-off-song", 27000, sl_song_loop(True)),
    }
    with ThreadPoolExecutor(3) as ex:
        futs = {k: ex.submit(f) for k, f in jobs.items()}
        res = {k: f.result() for k, f in futs.items()}
    for k, r in res.items():
        s = r["stats"]
        lines.append(f"  - {k}: starts {s['starts']}, count-ins {s['countins']}, clicks {s['clicks']}, loops "
                     f"{s['loops']}, ends {s['ends']}")
    A = lambda k: res[k]["stats"]
    check(A("a-loop")["countins"] == 1 and A("a-loop")["clicks"] <= 8 and A("a-loop")["loops"] >= 1,
          f"(a) BACKTRACK POP 3, LOOP: K4 and K1-K3 turned while playing, the loop wrapped: only the first count-in "
          f"(count-ins {A('a-loop')['countins']}, clicks {A('a-loop')['clicks']}, loops {A('a-loop')['loops']})")
    check(A("a-full")["countins"] == 1 and A("a-full")["clicks"] <= 8,
          f"(a) BACKTRACK POP 3, FULL: the same knobs: only the first count-in (count-ins {A('a-full')['countins']}, "
          f"clicks {A('a-full')['clicks']})")
    check(A("b-pause")["countins"] == 2 and A("b-pause")["loops"] >= 3,
          f"(b) BACKTRACK LOOP: pause in bar 3, PLAY, 3 wraps: exactly the start's and the resume's count-in "
          f"(count-ins {A('b-pause')['countins']}, loops {A('b-pause')['loops']})")
    check(A("c-song-loop")["countins"] == 1 and A("c-song-loop")["loops"] >= 1,
          f"(c) SONG LEARN, LOOP SONG with the start point at bar 4, the loop wrapped to the song's start: no "
          f"count-in at the wrap (count-ins {A('c-song-loop')['countins']}, loops {A('c-song-loop')['loops']})")
    check(A("c-bars-pause")["countins"] == 2 and A("c-bars-pause")["loops"] >= 3,
          f"(c) SONG LEARN, BARS A-B, pause and resume, 3 wraps: only the start's and the resume's count-in "
          f"(count-ins {A('c-bars-pause')['countins']}, loops {A('c-bars-pause')['loops']})")
    off = {k: A(k) for k in ("d-off-loop", "d-off-pause", "d-off-song")}
    first = ("d-off-loop", "d-off-pause")             # (BACKTRACK: the first start's count-in was stopped at once)
    check(all(off[k]["countins"] == 1 and off[k]["clicks"] <= 2 and off[k]["starts"] >= 2 for k in first) and
          off["d-off-song"]["countins"] == 0 and off["d-off-song"]["clicks"] == 0 and off["d-off-song"]["loops"] >= 1,
          f"(d) count-in OFF (ARP): no click after the first start's (BACKTRACK loop / pause; SONG LEARN none at all) "
          f"({', '.join(f'{k} {v['countins']}/{v['clicks']}' for k, v in off.items())})")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 2
_bt_only = {}


def bt_only_flash():
    """a song area with the backtracks and no song (sent over USB into the quiet empty flash) -> its name in m5.FLASH"""
    if "btonly" not in m5.FLASH:
        r = m5.run("m9-send-bt-only", 300000, [], flash="empty_quiet",
                   host=f"{m5.SEND} send --emu-pipe " + " ".join(str(p) for p in BTS))
        m5.FLASH["btonly"] = r["flash"]
    return "btonly"


def test_m9p2(all_songs, rep):
    lines, st, check = m5.checker("M9 part 2: small fixes and text")
    from concurrent.futures import ThreadPoolExecutor
    P = m5.press_raw
    cats = sorted({c["category"] for c in all_songs})
    want = [len(all_songs)] + [sum(c["category"] == k for c in all_songs) for k in cats]

    def folder(k, open_it):
        sc = P(4600, "HOME") + ([f"4800:e0:{k}"] if k else []) + (P(5200, "PLAY") if open_it else [])
        return m5.run(f"m9p2-folder-{k}-{'open' if open_it else 'row'}", 5900, sc, flash=bt_flash())
    with ThreadPoolExecutor(3) as ex:
        shown = list(ex.map(lambda k: folder(k, False), range(len(want))))
        listed = list(ex.map(lambda k: folder(k, True), range(len(want))))
    got = [r["stats"]["fold_shown"] for r in shown]
    n_in = [r["stats"]["list_n"] for r in listed]
    check(got == want and n_in == want,
          f"the folders with the 30 backtracks stored: All songs and each category count songs only: shown {got}, "
          f"opened {n_in}, songs {want} (All songs, {', '.join(cats)}; m9p2-folder-0-row.png)")
    only = m5.run("m9p2-bt-only", 5600, P(4600, "HOME"), flash=bt_only_flash())
    empty = m5.run("m9p2-empty", 5600, P(4600, "HOME"), flash="empty_quiet")
    key = lambda r: (r["stats"]["ui_view"], r["stats"]["list_n"], r["stats"]["popup_open"], r["stats"]["popup_btn"])
    check(key(only) == key(empty) and only["stats"]["list_n"] == 0 and only["stats"]["ui_view"] == V_LIST,
          f"SONG LEARN with only the backtracks stored behaves as an empty store (view, songs listed, the HOME "
          f"pop-up: {key(only)} = {key(empty)}; m9p2-bt-only.png)")
    # the mode screen: the title line "modes:" (dark pixels at x 10.., y 158..171 in the 240 x 240 screen) and the
    # new subtitle (read by eye in m9p2-mode.png; the pixels: the subtitle's right edge region changed from M8's)
    mode = m5.run("m9p2-mode", 4700, [], mode=None)
    img = mode["lcd"].convert("L").resize((240, 240))
    dark = sum(1 for x in range(10, 60) for y in range(158, 171) if img.getpixel((x, y)) < 200)
    check(dark > 20 and mode["stats"]["ui_view"] == V_MODE,
          f"the mode screen shows the title line \"modes:\" above the first choice ({dark} text pixels; m9p2-mode.png)")
    lfo = btrun("m9p2-lfo", 6500, P(4500, "PLAY"))
    lit, dim = lfo["stats"]["btn_lit"], lfo["stats"]["btn_dim"]
    check(lit >> m7.B["LFO"] & 1 and dim >> m7.B["LFO"] & 1 and lit >> m7.B["FX"] & 1,
          f"BACKTRACK's play screen: the LFO light is on steadily (lit {lit:#x}), like the drums' light")
    rep += lines
    return st["ok"]


PARTS = {"m9p1": test_m9p1, "m9p2": test_m9p2}


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
