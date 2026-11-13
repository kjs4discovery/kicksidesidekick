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
M9_FIELDS = ("countins fold_shown btn_lit btn_dim swaps swaps_late swap_at swap_to bt_cplx bt_want bt_tier_now "
             "bt_prepares bt_prepare_ms bt_pending_bar").split()
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


# --------------------------------------------------------------------------------------------- part 4
B = m7.B
BAR = 1920                                   # a 4/4 bar's ticks
STYL = {"POP": 4, "ROCK": 6, "WALTZ": 0}
LOOP_FROM = 4                                # the first verse of a backtrack starts at bar 4 (an intro of 4 bars)


def bt_play(extra, ms, setup=(), fast=True):
    """BACKTRACK at 250 BPM (a bar 0.96 s; the count-in's bar ends at 5.66 s): SETUP's choices, PLAY at 4.7 s, then extra"""
    sc = list(setup) + (["4450:e0:150"] if fast else []) + m5.press_raw(4700, "PLAY") + list(extra)
    return btrun(f"m9p4-{abs(hash(tuple(sc))) % 10**8}", ms, sc)


def named(name, extra, ms, setup=(), fast=True):
    sc = list(setup) + (["4450:e0:150"] if fast else []) + m5.press_raw(4700, "PLAY") + list(extra)
    return btrun(f"m9p4-{name}", ms, sc)


def test_m9p4(all_songs, rep):
    lines, st, check = m5.checker("M9 part 4: one knob map; changes on the phrase (D053)")
    from concurrent.futures import ThreadPoolExecutor
    P = m5.press_raw
    jobs = {}
    # ---- SETUP: every row of the map
    jobs["setup"] = lambda: btrun("m9p4-setup", 5700, ["4500:e2:14", "4600:e3:6", "4700:e4:1", "4800:e5:3", "4900:e6:3",
                                                     "5000:e1:1", "5100:e0:7"])
    jobs["setup-clamp-lo"] = lambda: btrun("m9p4-setup-lo", 5000, ["4500:e4:-5"])
    jobs["setup-clamp-hi"] = lambda: btrun("m9p4-setup-hi", 5000, ["4500:e4:5"])
    jobs["setup-minor"] = lambda: btrun("m9p4-setup-minor", 5000, ["4500:e2:14"])
    # ---- PLAY (LOOP: bars 4..11; the phrase boundary after the knob: bar 8, the line 15360)
    jobs["style"] = lambda: named("style", ["8000:e3:1"], 14000)
    jobs["style-next"] = lambda: named("style-next", ["8000:e3:1"], 8900)
    jobs["prog"] = lambda: named("prog", ["8000:e2:3"], 14000)
    jobs["to-full"] = lambda: named("to-full", ["8000:e1:2"], 15000)
    jobs["to-loop"] = lambda: named("to-loop", ["17800:e1:-2"], 25000, setup=["4400:e1:2"])
    jobs["cplx"] = lambda: named("cplx", ["7500:e4:-1"], 10300, setup=["4400:e1:2"])
    jobs["cplx-chorus"] = lambda: named("cplx-chorus", ["7500:e4:-1"], 19500, setup=["4400:e1:2"])
    jobs["cplx-chorus3"] = lambda: named("cplx-chorus3", ["7500:e4:1"], 19500, setup=["4400:e1:2"])
    jobs["key"] = lambda: named("key", ["8000:e6:3"], 11000)
    jobs["seq"] = lambda: named("seq", P(8000, "SEQ"), 9000)
    jobs["save"] = lambda: named("save", P(8000, "SAVE"), 9000)
    jobs["sound"] = lambda: named("sound", ["8000:e5:2"], 9000)
    jobs["turn-back"] = lambda: named("turn-back", ["8000:e3:1", "8100:e3:-1"], 12000)
    jobs["cancel"] = lambda: named("cancel", ["8000:e3:1", "8900:e3:-1"], 13000)
    jobs["together"] = lambda: named("together", ["8000:e2:1", "8050:e3:1"], 14000)
    jobs["two-classes"] = lambda: named("two-classes", ["9300:e2:1", "9350:e4:1"], 16500)
    jobs["waltz"] = lambda: named("waltz", ["8000:e3:6"], 14000)
    jobs["stopped"] = lambda: named("stopped", P(7000, "HOME") + ["8000:e4:1"], 9500)
    jobs["play-screen"] = lambda: named("play", [], 7600)
    for pr in range(31, 37):             # Part 5: the six new minor progressions play (and the setup shows them)
        jobs[f"prog{pr}"] = (lambda pr: lambda: btrun(f"m9p4-prog{pr}", 9000, [f"4300:e2:{pr - 1}"] +
                                                      m5.press_raw(4700, "PLAY")))(pr)
    jobs["setup-35"] = lambda: btrun("m9p4-setup-35", 4900, ["4300:e2:34"])
    jobs["help-setup"] = lambda: btrun("m9p4-help-setup", 5600, P(4600, "GLO"))
    jobs["help-play"] = lambda: named("help-play", P(7000, "GLO"), 7800)
    with ThreadPoolExecutor(3) as ex:
        futs = {k: ex.submit(f) for k, f in jobs.items()}
        R = {k: f.result()["stats"] for k, f in futs.items()}
    full = {}
    S = R["setup"]
    check(S["ui_view"] == V_SETUP and S["bt_prog"] == 15 and S["bt_style"] == 6 and S["bt_cplx"] == 3 and
          S["bt_sound"] == 3 and S["bt_key"] == 3 and S["bt_structure"] == 1 and S["bt_bpm"] == 103,
          f"SETUP: K1 +14 15 MINOR POP, K2 +6 WALTZ, K3 +1 complexity 3, K4 +3 sound 4, PRESETS +3 key 3, ALGO +1 "
          f"VERSE/CHORUS, SELECT +7 (96 -> 103 BPM) [{S['bt_prog']}, {S['bt_style']}, {S['bt_cplx']}, {S['bt_sound']}, "
          f"{S['bt_key']}, {S['bt_structure']}, {S['bt_bpm']}] (m9p4-setup.png, m9p4-setup-minor.png)")
    check(R["setup-clamp-lo"]["bt_cplx"] == 1 and R["setup-clamp-hi"]["bt_cplx"] == 3 and
          R["setup-minor"]["bt_prog"] == 15, "K3 stops at 1 and at 3 (no wrap)")
    n_ = lambda k: R[k]
    check(n_("play-screen")["bt_cplx"] == 2 and n_("play-screen")["swaps"] == 0 and n_("play-screen")["ui_view"] == V_PLAY,
          "PLAY: complexity 2 (MEDIUM) by default; the play screen (m9p4-play.png)")
    a = n_("style")
    check(a["swaps"] == 1 and a["bt_style"] == 1 and a["bt_style_now"] == STYL["ROCK"] and a["swap_at"] == 8 * BAR and
          a["swap_to"] == 8 * BAR and a["countins"] == 1 and a["clicks"] <= 4 and a["swaps_late"] == 0,
          f"K2 +1 while playing: ROCK at the phrase boundary, bar 8 (tick {a['swap_at']}), from the same bar "
          f"({a['swap_to']}), no count-in (count-ins {a['countins']}, clicks {a['clicks']})")
    nx = n_("style-next")
    check(nx["bt_want"] == 2 and nx["swaps"] == 0 and nx["bt_style"] == 0 and nx["bt_pending_bar"] in (8, 0xFFFFFFFF),
          f"before the boundary: the change waits (NEXT: ROCK shown; m9p4-style-next.png) [want {nx['bt_want']:#x}, "
          f"pending bar {nx['bt_pending_bar']}]")
    p = n_("prog")
    check(p["swaps"] == 1 and p["bt_prog"] == 4 and p["swap_at"] == 8 * BAR and p["swap_to"] == 8 * BAR and
          p["countins"] == 1, f"K1 +3: 4 POP 3 at the phrase boundary, the same section and bar [{p['bt_prog']}, "
                              f"{p['swap_at']} -> {p['swap_to']}]")
    f = n_("to-full")
    check(f["swaps"] == 1 and f["bt_structure"] == 2 and f["bt_loop_from"] == 0 and f["bt_loop_to"] == 0 and
          f["loops"] == 0, f"ALGO +2: FULL at the phrase boundary: the loop is gone, no wrap "
                           f"(swap at {f['swap_at']}, loops {f['loops']})")
    l = n_("to-loop")
    check(l["swaps"] == 1 and l["bt_structure"] == 0 and l["swap_at"] == 16 * BAR and l["swap_to"] == LOOP_FROM * BAR and
          l["bt_loop_from"] == LOOP_FROM * BAR, f"FULL in the first chorus, ALGO -2 (LOOP): at its 4th bar (bar 16, tick "
          f"{l['swap_at']}) the music jumps to the loop's start ({l['swap_to']}; the position was outside the loop)")
    c1, c2, c3 = n_("cplx"), n_("cplx-chorus"), n_("cplx-chorus3")
    check(c1["swaps"] == 1 and c1["bt_cplx"] == 1 and c1["bt_tier_now"] == 1 and c1["swap_at"] == c1["swap_to"] and
          c1["swap_at"] == 3 * BAR, f"K3 -1: complexity 1 at the next bar line (tick "
          f"{c1['swap_at']}); a verse plays tier 1 (the layers row: L{c1['bt_tier_now']})")
    check(c2["bt_cplx"] == 1 and c2["bt_tier_now"] == 2 and c3["bt_cplx"] == 3 and c3["bt_tier_now"] == 4,
          f"a chorus plays one tier above: complexity 1 -> L{c2['bt_tier_now']}, complexity 3 -> L{c3['bt_tier_now']} "
          f"(m9p4-cplx-chorus.png)")
    k = n_("key")
    check(k["swaps"] == 1 and k["bt_key"] == 3 and k["transpose"] == 3 and k["swap_at"] % BAR == 0,
          f"PRESETS +3: the key Eb at the next bar line (transposed {k['transpose']}; m9p4-key.png)")
    for nm, btn in (("seq", "SEQ"), ("save", "SAVE")):
        q = n_(nm)
        check(q["popup_open"] == 1 and q["popup_btn"] == B[btn] and q["swaps"] == 0,
              f"{btn} on the play screen: the \"not implemented\" pop-up (m9p4-{nm}.png)")
    so = n_("sound")
    check(so["bt_sound"] == 2 and so["swaps"] == 0 and so["bt_want"] == 0, "K4: the melody sound at once, nothing waits")
    tb, cn = n_("turn-back"), n_("cancel")
    check(tb["swaps"] == 0 and tb["bt_want"] == 0 and tb["bt_style"] == 0,
          "turning the knob back to the playing value cancels the waiting change")
    check(cn["swaps"] == 0 and cn["bt_want"] == 0 and cn["bt_style"] == 0 and cn["swaps_late"] == 0,
          "turned back after the new version was prepared and asked for: withdrawn, nothing switches")
    tg = n_("together")
    check(tg["swaps"] == 1 and tg["bt_prog"] == 2 and tg["bt_style"] == 1 and tg["swap_at"] == 8 * BAR,
          f"K1 and K2 together: both at the same boundary, one switch [{tg['bt_prog']}, {tg['bt_style']}, swaps "
          f"{tg['swaps']}]")
    tw = n_("two-classes")
    check(tw["swaps"] == 2 and tw["bt_prog"] == 2 and tw["bt_cplx"] == 3 and tw["countins"] == 1,
          f"K1 (phrase) and K3 (bar) turned together late in the phrase: the complexity at the next bar, the progression "
          f"at the phrase boundary: {tw['swaps']} switches [{tw['bt_prog']}, {tw['bt_cplx']}]")
    wz = n_("waltz")
    check(wz["swaps"] == 1 and wz["bt_metre"] == 3 and wz["bt_style_now"] == STYL["WALTZ"] and wz["swap_to"] == 8 * 1440 and
          wz["bt_loop_from"] == LOOP_FROM * 1440, f"K2 +6 WALTZ: 3/4 from the same bar (tick {wz['swap_at']} -> "
                                                  f"{wz['swap_to']}), the loop in 3/4 ticks")
    sp = n_("stopped")
    check(sp["swaps"] == 0 and sp["bt_cplx"] == 3 and sp["ui_view"] == V_PLAY,
          "stopped (HOME): a change is made at once (the band loaded again), the play screen stays")
    bad = [pr for pr in range(31, 37) if not (n_(f"prog{pr}")["bt_prog"] == pr and n_(f"prog{pr}")["ui_view"] == V_PLAY and
           all(n_(f"prog{pr}")["notes_on"][l] > 0 for l in range(3)) and n_(f"prog{pr}")["scale_kind"] & 8)]
    check(not bad and n_("setup-35")["bt_prog"] == 35,
          f"the six new minor progressions 31-36 play (drums, bass, harmony sound) in A minor {bad or ''} "
          f"(m9p4-setup-35.png: Bm7b5 E7 Am Am)")
    rm = max((r["render_max"] + r["synth_render_max"]) / r["budget"] for r in R.values() if r["budget"])
    check(rm < 1.0, f"no block over its time in any of these runs (worst {rm * 100:.0f} % of a block)")
    ph = n_("help-play")
    check(R["help-setup"]["ui_view"] == V_HELP and ph["ui_view"] == V_HELP,
          "GLO: the help of SETUP and of the play screen, the new map (m9p4-help-setup.png, m9p4-help-play.png)")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 7
SAMPLE_PROGS = (1, 9, 20, 35)                  # POP 1 (37 bars), CANON (65), 12-BAR BLUES (41), MINOR 2-5-1 (37)
KEYS4 = m8.KEYS4


def test_m9census(all_songs, rep):
    """the button census (D044), BACKTRACK's SETUP and play screen with the M9 map: every button has its job or the pop-up"""
    lines, st, check = m5.checker("M9: the button census (D044) of SETUP and BACKTRACK's play screen; HOME taps; rescues")
    names = list(m7.B)
    jobs = {"SETUP": ({"PLAY", "GLO", "OCTDN", "OCTUP"}, []),
            "BACKTRACK play": (set(names) - {"SEQ", "SAVE"}, m5.press_raw(4500, "PLAY") + m5.press_raw(4900, "PLAY"))}
    bad = []
    for scr, (has, pre) in jobs.items():
        for b in names:
            if b in ("GLO", "HOME"):
                continue
            t0 = 5600 if pre else 4700
            r = btrun(f"m9c-{scr[:5]}-{b}", t0 + 600, pre + m5.press_raw(t0, b))
            pop = r["stats"]["popup_open"] == 1 and r["stats"]["popup_btn"] == m7.B[b]
            if pop == (b in has):
                bad.append((scr, b, pop))
    hm = btrun("m9c-setup-home", 5500, m5.press_raw(4700, "HOME"))
    check(not bad and hm["stats"]["popup_btn"] == m7.B["HOME"] and hm["stats"]["popup_open"] == 1,
          f"SETUP: PLAY, GLO, OCT-/OCT+ have jobs, every other button (HOME a tap) the pop-up; BACKTRACK's play screen: "
          f"every button a job, but SEQ and SAVE (the pop-up; the knobs and PRESETS do their jobs) {bad or ''}")
    taps = []
    for k in range(24):
        t = 4800 + 137 * k
        taps += [f"{t}:HOME!", f"{t + 40 + k % 3}:HOME^!"]
    tp = btrun("m9c-home-taps", 9000, m5.press_raw(4500, "PLAY") + taps)
    check(tp["stats"]["mode_switches"] == 0 and tp["stats"]["ui_view"] in (V_SETUP, V_PLAY),
          "24 quick HOME taps in BACKTRACK: never the mode screen")
    md = m5.run("m9c-mode", 4700, [], mode=None)
    sw = m5.run("m9c-switch", 7000, m5.press_raw(4500, "GLO") + m5.press_raw(4700, "GLO") + ["4900:e0:10"] +
                m5.press_raw(5300, "PLAY"))
    hl = btrun("m9c-home-long", 9000, m5.press_raw(4500, "PLAY") + m6.hold_home(6000))
    check(md["stats"]["ui_view"] == V_MODE and sw["stats"]["ui_view"] == V_MODE and hl["stats"]["ui_view"] == V_MODE,
          "the mode screen (three choices) after the splash, from Settings > Switch mode, and HOME held 2 s in BACKTRACK; "
          "the rescues (OCT-+OCT+, REC, GLO+HOME at power-on) are the M8 ones (m8 / m6 suites)")
    gl = btrun("m9c-help", 5600, m5.press_raw(4600, "GLO"))
    gp = btrun("m9c-help-play", 6600, m5.press_raw(4500, "PLAY") + m5.press_raw(5600, "GLO"))
    check(gl["stats"]["ui_view"] == V_HELP and gp["stats"]["ui_view"] == V_HELP,
          "GLO: the controls of SETUP (m9c-help.png) and of the play screen (m9c-help-play.png)")
    rep += lines
    return st["ok"]


def test_m9cpu(all_songs, rep):
    """the budget: every style, FULL, complexity 3 (a chorus plays tier 4), every layer on, the heaviest preset (as made) on
    4 voices with its effects held in the chorus, at 250 BPM, with a seamless switch (a style change) made in the run: EAS +
    synth render of a block over the block's time"""
    lines, st, check = m5.checker("M9 budget: BACKTRACK's CPU worst case (tier 4, a chorus, FULL, a live switch)")
    from concurrent.futures import ThreadPoolExecutor
    heavy = PRESETS.index("MIAMI PAD")

    def one(sty):
        sc = ["4300:e4:1"] + ([f"4350:e3:{sty}"] if sty else []) + ["4400:e1:2", "4450:e0:150"] + \
            m5.press_raw(4600, "PLAY") + [f"5300:e5:{heavy}", "17000:e3:1"] + [f"17600:{k}" for k in KEYS4] + \
            [f"24500:{k}^" for k in KEYS4]
        r = btrun(f"m9cpu-{sty}", 25000, sc)
        s = r["stats"]
        return sty, (s["render_max"] + s["synth_render_max"]) / s["budget"], s["render_max"] / s["budget"], \
            s["synth_voices_max"], s["bt_cplx"], s["swaps"], r["halt"], s["bt_tier_now"]
    with ThreadPoolExecutor(3) as ex:
        res = list(ex.map(one, range(11)))
    worst = max(res, key=lambda x: x[1])
    lines.append("  - per style (POP ROCK INDIE BALLAD SHUFFLE STRIDE WALTZ COUNTRY BOSSA FUNK REGGAE): " +
                 ", ".join(f"{v * 100:.0f} % ({nv} v, tier {t}, {sw} sw{', HALT' if h else ''})" for _, v, _, nv, _, sw, h, t in res))
    check(worst[1] <= 0.60 and all(nv == 4 and sw >= 1 and not h and c == 3 for _, _, _, nv, c, sw, h, _ in res),
          f"the worst: style {worst[0] + 1}, complexity 3, {PRESETS[heavy]} x 4 voices, a switch in the run: EAS + synth "
          f"{worst[1] * 100:.0f} % of a block (EAS {worst[2] * 100:.0f} %; limit 60 %)")
    rep += lines
    st["worst"] = worst
    return st["ok"]


def test_m9sweep(all_songs, rep):
    """every style x 3 structures x complexity 1-3 x progressions 1, 9, 20, 35, at 250 BPM, with a live style change and a live
    progression change in each run: to its end (FULL) or a loop round, no crash, no block over its time, no count-in"""
    lines, st, check = m5.checker("M9: every style x structure x complexity x 4 progressions, with live changes")
    from concurrent.futures import ThreadPoolExecutor
    jobs = [(p, sty, stc, cx) for p in SAMPLE_PROGS for sty in range(11) for stc in range(3) for cx in (1, 2, 3)]
    import os
    if os.environ.get("M9_SWEEP_ONLY"):          # (a re-run of some runs: "style,structure")
        sty_, stc_ = map(int, os.environ["M9_SWEEP_ONLY"].split(","))
        jobs = [j for j in jobs if j[1] == sty_ and j[2] == stc_]
    bars_of = {p: compiled_bt(p)["bar_count"] for p in SAMPLE_PROGS + tuple(p + 1 for p in SAMPLE_PROGS)}

    def one(j):
        p, sty, stc, cx = j
        bpb = 3 if sty == 6 else 4
        bars = max(bars_of[p], bars_of[p + 1])
        # (the style change at 6.5 s may take WALTZ to a 4/4 style: the run is timed for 4 beats a bar then)
        ms = int(5600 + (bars + 2) * 4 * 60000 / 250 + 3000) if stc == 2 else 17500
        sc = ([f"4300:e2:{p - 1}"] if p > 1 else []) + ([f"4350:e3:{sty}"] if sty else []) + \
            ([f"4400:e1:{stc}"] if stc else []) + ([f"4420:e4:{cx - 2}"] if cx != 2 else [])
        sc += ["4450:e0:150"] + m5.press_raw(4700, "PLAY") + ["6500:e3:1", "10500:e2:1"]   # (two switches: bar 4 / 8, then 8 / 12)
        r = btrun(f"m9sw-{p}-{sty}-{stc}-{cx}", ms, sc)
        s = r["stats"]
        load = (s["render_max"] + s["synth_render_max"]) / s["budget"] if s["budget"] else 9
        ok = not r["halt"] and load < 1.0 and s["countins"] == 1 and s["swaps"] >= 2 and s["bt_cplx"] == cx and \
            (s["ends"] >= 1 if stc == 2 else True)
        return j, ok, load, s["ends"], s["loops"], s["swaps"]
    with ThreadPoolExecutor(3) as ex:
        res = list(ex.map(one, jobs))
    bad = [(j, round(l, 2), e, lp, sw) for j, ok, l, e, lp, sw in res if not ok]
    worst = max(l for _, _, l, _, _, _ in res)
    check(not bad, f"{len(res)} runs (11 styles x LOOP, VERSE/CHORUS, FULL x complexity 1-3 x progressions {SAMPLE_PROGS}, 250 "
                   f"BPM, a style and a progression change in each): each reached its end or looped, switched, no crash, no "
                   f"count-in, no block over its time (worst {worst * 100:.0f} %) {bad[:5] or ''}")
    rep += lines
    return st["ok"]


PARTS = {"m9p1": test_m9p1, "m9p2": test_m9p2, "m9p4": test_m9p4, "m9census": test_m9census, "m9cpu": test_m9cpu,
         "m9sweep": test_m9sweep}


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
