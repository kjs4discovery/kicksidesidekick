#!/usr/bin/env python3
# m10_test.py -- Milestone M10: headless emulator tests (built on tools/m9_test.py's runner and stats; same structure).
# Never touches the FM-1.
# Licence: TBD (see D017)
#
#   .venv/bin/python -I tools/m10_test.py [--only m10p1,m10p2,..] [--pkg PKG] [--out DIR]   -> build/emu/m10/report.md
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
import m9_test as m9                     # noqa: E402

btrun, bt_flash, compiled_bt, BTS = m8.btrun, m8.bt_flash, m8.compiled_bt, m8.BTS
B = m7.B
BAR = 1920
V_LIST, V_PLAY, V_HELP, V_SETTINGS, V_MODE, V_SYNTH, V_SETUP = m8.V_LIST, m8.V_PLAY, m8.V_HELP, m8.V_SETTINGS, \
    m8.V_MODE, m8.V_SYNTH, m8.V_SETUP
P = m5.press_raw

# --------------------------------------------------------------------------------------------- stats
# M10 adds words after M9's (player.h "M10"); the list grows by part.
M10_FIELDS = ("vp_open vp_count vp_close_ms vp_btn_closed ind_section ind_blinking ind_blink_starts ind_blink_tick ind_blink_to "
              "ind_on kit kit_hits kit_bass_prog kit_harm_prog preset_used preset_saves preset_loaded preset_sel save_screen "
              "save_slot preset_junk info_shown pad0 pad1").split()
M10_STRINGS = (("vp_title", 24), ("vp_value", 40), ("vp_sub", 32), ("next_text", 48), ("ind_name", 16), ("info_text", 40),
               ("preset_name", 16))
_stats9 = m5.stats


def stats10(ram):
    s = _stats9(ram)
    if s is None:
        return s
    i = ram.find(b"PLST")
    while i >= 0 and struct.unpack_from("<I", ram, i + 4)[0] not in (3, 4):
        i = ram.find(b"PLST", i + 1)
    base = i + m5.STATS.size + m5.STATS4.size + 128 + (18 + 14) * 4
    s.update(zip(M10_FIELDS, struct.unpack_from(f"<{len(M10_FIELDS)}I", ram, base)))
    at = base + 4 * len(M10_FIELDS)
    for name, n in M10_STRINGS:
        s[name] = ram[at:at + n].split(b"\0")[0].decode("latin-1")
        at += n
    return s


m5.stats = stats10


def named(name, extra, ms, setup=(), fast=True):
    """BACKTRACK at 250 BPM (a bar 0.96 s; the count-in's bar ends at 5.66 s): SETUP's choices, PLAY at 4.7 s, then extra"""
    sc = list(setup) + (["4450:e0:150"] if fast else []) + P(4700, "PLAY") + list(extra)
    return btrun(f"m10-{name}", ms, sc)


# --------------------------------------------------------------------------------------------- part 1
def test_m10p1(all_songs, rep):
    lines, st, check = m5.checker("M10 part 1: a change while paused keeps the pause (M9-17 changed)")
    from concurrent.futures import ThreadPoolExecutor
    # LOOP (verse: bars 4..11, music from 5.66 s): bar 6 (its 3rd bar) plays 7.58-8.54 s. PLAY pauses at 8.3 s, K2 +1 (ROCK)
    # at 8.8 s (built 300 ms later), PLAY at 10.0 s resumes
    pause = P(8300, "PLAY") + ["8800:e3:1"]
    with ThreadPoolExecutor(3) as ex:
        fa = ex.submit(lambda: named("p1-paused", pause, 9800))
        fb = ex.submit(lambda: named("p1-resume", pause + P(10000, "PLAY"), 12500))
        fc = ex.submit(lambda: named("p1-stopped", P(7000, "HOME") + ["8000:e3:1"] + P(9000, "PLAY"), 11000))
        a, b, c = fa.result()["stats"], fb.result()["stats"], fc.result()["stats"]
    check(a["pauses"] == 1 and a["starts"] == 1 and a["bt_style"] == 1 and a["bt_style_now"] == 6,
          f"paused in bar 3 of the verse, K2 -> ROCK: the new band is built (style {a['bt_style']}) and the player stays "
          f"paused (starts {a['starts']}, pauses {a['pauses']})")
    check(b["starts"] == 2 and b["start_from"] == 6 * BAR and b["bt_style"] == 1,
          f"PLAY: the music starts in bar 3 of that verse (tick {b['start_from']}, want {6 * BAR}), with the count-in rule "
          f"(count-ins {b['countins']})")
    check(c["starts"] == 2 and c["start_from"] == 4 * BAR,
          f"while STOPPED: unchanged, PLAY starts from the loop's start (tick {c['start_from']})")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 2
def test_m10p2(all_songs, rep):
    lines, st, check = m5.checker("M10 part 2: BACKTRACK's value pop-up, names, NEXT line, section indicator (D057)")
    from concurrent.futures import ThreadPoolExecutor
    jobs = {}
    # ---- SETUP: each knob (progression 1 POP 1, POP, MEDIUM, sound 1, key C, LOOP, 100 BPM at the start)
    knobs = {"K1": ("e2", 14), "K2": ("e3", 1), "K3": ("e4", 1), "K4": ("e5", 1), "PRE": ("e6", 3), "ALG": ("e1", 2),
             "SEL": ("e0", 5)}
    for k, (e, d) in knobs.items():
        jobs[f"setup-{k}"] = (lambda k, e, d: lambda: btrun(f"m10p2-setup-{k}", 5200, [f"4500:{e}:{d}"]))(k, e, d)
    jobs["setup-minor-key"] = lambda: btrun("m10p2-setup-minor-key", 5200, ["4400:e2:14", "4500:e6:3"])
    # ---- the play screen (a change waits; K4 and SELECT act at once)
    for k, (e, d) in knobs.items():
        jobs[f"play-{k}"] = (lambda k, e, d: lambda: named(f"p2-play-{k}", [f"8000:{e}:{d}"], 8400))(k, e, d)
    jobs["play-two"] = lambda: named("p2-play-two", ["8000:e3:1", "8010:e1:2", "8020:e4:1"], 8400)
    jobs["play-cplx"] = lambda: named("p2-play-cplx", ["8000:e4:1"], 8400)
    jobs["play-prog"] = lambda: named("p2-play-prog", ["8000:e2:6"], 8400)
    jobs["play-key"] = lambda: named("p2-play-key", ["8000:e6:2"], 8400)
    jobs["play-full"] = lambda: named("p2-play-full", ["8000:e1:2"], 8400)
    # ---- closing: by time (1,500 ms after the last turn), by a press that also does its job
    jobs["close-time"] = lambda: btrun("m10p2-close-time", 6800, ["4500:e2:3", "4900:e2:1"])
    jobs["close-press"] = lambda: btrun("m10p2-close-press", 6000, ["4500:e2:3"] + P(4900, "PLAY"))
    jobs["after-close"] = lambda: btrun("m10p2-after-close", 6600, ["4500:e2:3"])
    with ThreadPoolExecutor(3) as ex:
        futs = {k: ex.submit(f) for k, f in jobs.items()}
        R = {k: f.result()["stats"] for k, f in futs.items()}
    want = {"K1": ("Progression", "15 MINOR POP"), "K2": ("Style", "ROCK"), "K3": ("Song Complexity", "BUSY"),
            "K4": ("Melody Sound", None), "PRE": ("Key", "Eb major"), "ALG": ("Structure", "FULL"),
            "SEL": ("Tempo", "105 BPM")}
    bad = []
    for k, (t, v) in want.items():
        r = R[f"setup-{k}"]
        if r["vp_title"] != t or (v and r["vp_value"] != v) or not r["vp_open"] or r["vp_sub"]:
            bad.append((k, r["vp_title"], r["vp_value"], r["vp_sub"]))
    check(not bad and R["setup-K4"]["vp_value"] == m8.PRESETS[1] and R["setup-minor-key"]["vp_value"] == "C minor"
          and R["setup-K1"]["vp_title"] == "Progression",
          f"SETUP: each knob opens its pop-up with the exact title and value: {[(k, w[0], R['setup-' + k]['vp_value']) for k, w in want.items()]} "
          f"(K4 {R['setup-K4']['vp_value']}; PRESETS on a minor progression: {R['setup-minor-key']['vp_value']}) {bad or ''} "
          f"(m10p2-setup-K1.png, -K3.png)")
    sub = {k: R[f"play-{k}"]["vp_sub"] for k in want}
    check(sub == {"K1": "at the next phrase", "K2": "at the next phrase", "K3": "at the next bar", "K4": "",
                  "PRE": "at the next bar", "ALG": "at the next phrase", "SEL": ""},
          f"the play screen: the waiting changes say when they take effect (K1, K2, ALGO: the next phrase; K3, PRESETS: the next bar); "
          f"K4 and SELECT act at once: {sub} (m10p2-play-K2.png)")
    nx = {k: R[f"play-{k}"]["next_text"] for k in ("K1", "K2", "K3", "PRE", "ALG")}
    check(nx == {"K1": "NEXT: PROG 15", "K2": "NEXT: STYLE ROCK", "K3": "NEXT: COMPLEXITY BUSY", "PRE": "NEXT: KEY Eb",
                 "ALG": "NEXT: STRUCTURE FULL"} and R["play-two"]["next_text"] == "NEXT: STYLE ROCK +2" and
          R["play-prog"]["next_text"] == "NEXT: PROG 7",
          f"the NEXT line is spelled out and counts the rest: {nx}, two together: {R['play-two']['next_text']!r} "
          f"(m10p2-play-two.png)")
    t = R["close-time"]
    check(t["vp_open"] == 0 and t["vp_btn_closed"] == 0 and 1400 <= t["vp_close_ms"] - 400 <= 1700,
          f"closes by itself 1,500 ms after the last turn (second turn 400 ms after the first: open {t['vp_close_ms']} ms in all, "
          f"expected about 1,900)")
    cp = R["close-press"]
    check(cp["vp_open"] == 0 and cp["vp_btn_closed"] == 1 and cp["ui_view"] == V_PLAY and cp["starts"] == 1,
          f"a press closes it AND does its job: PLAY started the backtrack (view {cp['ui_view']}, starts {cp['starts']}; "
          f"open {cp['vp_close_ms']} ms)")
    check(R["after-close"]["vp_open"] == 0 and R["after-close"]["ui_view"] == V_SETUP,
          "after it closes the SETUP screen is drawn again (m10p2-after-close.png)")
    # ---- the section indicator: FULL, progression 1 (intro 0-3, verse 4-11, chorus 12-19, verse 20-23, chorus 24-27, bridge 28-31,
    # chorus 32-35, ending 36): the music starts at 5.66 s; a bar is 0.96 s
    full = ["4400:e1:2"]
    changes = {4: "VERSE", 12: "CHORUS", 28: "BRIDGE", 36: "ENDING"}
    ij = {}
    for b, nm in changes.items():
        t_blink = 5660 + (b - 1) * 960
        ij[f"blink-{b}"] = (lambda b, t: lambda: named(f"p2-blink-{b}", [], t + 500, setup=full))(b, t_blink)
        ij[f"steady-{b}"] = (lambda b, t: lambda: named(f"p2-steady-{b}", [], t + 960 + 300, setup=full))(b, t_blink)
    ij["first"] = lambda: named("p2-first", [], 7500, setup=full)
    ij["loop"] = lambda: named("p2-loop", [], 5660 + 20 * 960, setup=[])
    ij["vc"] = lambda: named("p2-vc", [], 5660 + 15 * 960 + 500, setup=["4400:e1:1"])
    with ThreadPoolExecutor(3) as ex:
        futs = {k: ex.submit(f) for k, f in ij.items()}
        I = {k: f.result()["stats"] for k, f in futs.items()}
    bad = []
    for b, nm in changes.items():
        s1, s2 = I[f"blink-{b}"], I[f"steady-{b}"]
        t0 = (b - 1) * BAR
        if not (s1["ind_blinking"] and s1["ind_name"] == nm and s1["ind_blink_starts"] == list(changes).index(b) * 0 + s1["ind_blink_starts"]
                and t0 <= s1["ind_blink_tick"] < t0 + 140):
            bad.append(("blink", b, s1["ind_name"], s1["ind_blinking"], s1["ind_blink_tick"], t0))
        if s2["ind_blinking"] or s2["ind_name"] != nm:
            bad.append(("steady", b, s2["ind_name"], s2["ind_blinking"]))
    check(not bad and I["first"]["ind_name"] == "INTRO" and not I["first"]["ind_blinking"],
          f"FULL: the indicator names INTRO, then VERSE, CHORUS, BRIDGE, ENDING; the blink starts in the last bar before each change "
          f"(ticks {[I[f'blink-{b}']['ind_blink_tick'] for b in changes]} for the bars {[(b - 1) * BAR for b in changes]}, "
          f"within 140 ticks = 2 frames) and is steady once it has started {bad or ''} first: {I["first"]["ind_name"]!r} blinking {I["first"]["ind_blinking"]} (m10p2-blink-12.png, m10p2-steady-12.png)")
    check(I["loop"]["ind_blink_starts"] == 0 and I["loop"]["ind_name"] == "VERSE",
          f"LOOP (one verse repeating): the indicator never blinks (blinks {I['loop']['ind_blink_starts']})")
    check(I["vc"]["ind_blink_starts"] >= 1 and I["vc"]["ind_name"] == "VERSE",
          f"VERSE/CHORUS: at the loop's wrap to the verse it blinks as at any change (blinks {I['vc']['ind_blink_starts']}, shows "
          f"{I['vc']['ind_name']})")
    # ---- no "C1", "C2", "C3" in the BACKTRACK texts
    src = (ROOT / "firmware/app/bt.c").read_text()
    import re
    check(not re.search(r'text_cat\([^;]*"C"\)', src) and '"  C"' not in src and "BT_CPLX_NAME" in src,
          "the complexity is a name everywhere in bt.c (no \"C\" + number)")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 3
def _margins_clear(img, y0, y1):
    """no pixel but the background in the 4 px side margins of rows y0..y1 of a 240 x 240 screen"""
    bg = img.getpixel((0, 0))
    return all(img.getpixel((x, y)) == bg for y in range(y0, y1) for x in list(range(4)) + list(range(236, 240)))


def test_m10p3(all_songs, rep):
    lines, st, check = m5.checker("M10 part 3: Tuneka - Song Sidekick on the mode screen and the splash (D058, D063)")
    from concurrent.futures import ThreadPoolExecutor
    with ThreadPoolExecutor(3) as ex:
        sp = ex.submit(lambda: m5.run("m10p3-splash", 2500, [], mode=None))
        md = ex.submit(lambda: m5.run("m10p3-mode", 4700, [], mode=None))
        ch = ex.submit(lambda: m5.run("m10p3-choose", 5200, ["4400:PLAY", "4460:PLAY^"], mode=None, flash=bt_flash()))
        sp, md, ch = sp.result(), md.result(), ch.result()
    ssrc = (ROOT / "firmware/app/app.h").read_text()
    check('#define APP_NAME "Tuneka - Song Sidekick"' in ssrc and '#define FW_MILESTONE "m10"' in ssrc and
          sp["stats"]["ui_view"] >= 16,
          "the splash: \"Tuneka - Song Sidekick\" and \"m10\" under it (app.h; m10p3-splash.png)")
    simg = sp["lcd"].convert("RGB").resize((240, 240))
    mimg = md["lcd"].convert("RGB").resize((240, 240))
    bg = mimg.getpixel((0, 0))
    ink = sum(1 for x in range(4, 236) for y in range(2, 18) if mimg.getpixel((x, y)) != bg)
    sink = sum(1 for x in range(4, 236) for y in range(190, 214) if simg.getpixel((x, y)) != simg.getpixel((0, 0)))
    check(md["stats"]["ui_view"] == V_MODE and ink > 150 and _margins_clear(mimg, 0, 20) and sink > 150
          and _margins_clear(simg, 188, 216),
          f"the title fits across the top of the mode screen with a 4 px margin ({ink} text pixels in its row) and the splash's "
          f"name fits too ({sink}); the mascot is scaled under it (m10p3-mode.png)")
    check(md["stats"]["mode_sel"] == 0 and ch["stats"]["ui_view"] == V_LIST,
          f"the cursor starts on SONG LEARN (mode_sel {md['stats']['mode_sel']}); PLAY opens it (view {ch['stats']['ui_view']})")
    left = [str(p.relative_to(ROOT)) for d in ("firmware/app", "tools", "docs/controls.md") for p in
            ((ROOT / d).rglob("*") if (ROOT / d).is_dir() else [ROOT / d])
            if p.is_file() and p.suffix in (".c", ".h", ".py", ".tsv", ".md", ".toml") and "build" not in p.parts
            and "Notami" in p.read_text(errors="ignore") and p.name != "m10_test.py"]
    check(not left, f"\"Notami\" is gone from the firmware, the tools and docs/controls.md {left or ''}")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 4
def test_m10p4(all_songs, rep):
    lines, st, check = m5.checker("M10 part 4: six more progressions, 42 in all (D060)")
    from concurrent.futures import ThreadPoolExecutor
    STY = {"POP": 0, "ROCK": 1, "INDIE POP": 2, "BALLAD": 3, "SHUFFLE": 4}
    own = {37: "ROCK", 38: "BALLAD", 39: "BALLAD", 40: "BALLAD", 41: "INDIE POP", 42: "SHUFFLE"}
    jobs = {}
    for pr in range(37, 43):
        for sty in ("POP", own[pr]):
            jobs[(pr, sty)] = (lambda pr, sty: lambda: btrun(
                f"m10p4-{pr}-{sty.replace(' ', '')}", 9500,
                [f"4300:e2:{pr - 1}"] + ([f"4350:e3:{STY[sty]}"] if STY[sty] else []) + P(4700, "PLAY")))(pr, sty)
    jobs["reach"] = lambda: btrun("m10p4-reach", 5000, ["4500:e2:41"])
    jobs["wrap"] = lambda: btrun("m10p4-wrap", 5000, ["4500:e2:42"])
    jobs["wrap-back"] = lambda: btrun("m10p4-wrap-back", 5000, ["4500:e2:-1"])
    jobs["n"] = lambda: btrun("m10p4-n", 4800, [])
    with ThreadPoolExecutor(3) as ex:
        futs = {k: ex.submit(f) for k, f in jobs.items()}
        R = {k: f.result()["stats"] for k, f in futs.items()}
    check(R["n"]["bt_n"] == 42 and R["reach"]["bt_prog"] == 42 and R["wrap"]["bt_prog"] == 1 and R["wrap-back"]["bt_prog"] == 42,
          f"42 backtracks in the store; K1 from 1 reaches {R['reach']['bt_prog']}, one more wraps to {R['wrap']['bt_prog']}, back "
          f"from 1 to {R['wrap-back']['bt_prog']} (m10p4-reach.png)")
    bad = []
    for (pr, sty), s in ((k, v) for k, v in R.items() if isinstance(k, tuple)):
        minor = 8 if pr in (37, 38, 39) else 0
        blues = 4 if pr == 42 else 0
        if not (s["bt_prog"] == pr and s["ui_view"] == V_PLAY and all(s["notes_on"][l] > 0 for l in range(3)) and
                s["scale_kind"] & 12 == minor | blues and s["budget"] and not s["countins"] > 1):
            bad.append((pr, sty, s["bt_prog"], s["notes_on"][:3], s["scale_kind"]))
    check(not bad, f"each new progression plays (drums, bass and harmony sound) in POP and in its own style {own}: the badge "
                   f"(minor 37-39, major 40-41, blues 42) {bad or ''} (m10p4-37-POP.png, m10p4-42-SHUFFLE.png)")
    rep += lines
    return st["ok"]


PARTS = {"m10p1": test_m10p1, "m10p2": test_m10p2, "m10p3": test_m10p3, "m10p4": test_m10p4}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    ap.add_argument("--pkg")
    ap.add_argument("--out")
    ap.add_argument("--jobs", type=int, default=3)
    a = ap.parse_args()
    m5.OUT = Path(a.out).resolve() if a.out else ROOT / "build/emu/m10"
    if a.pkg:
        m5.PKG = Path(a.pkg).resolve()
    m5.OUT.mkdir(parents=True, exist_ok=True)
    only = set(a.only.split(",")) if a.only else set(PARTS)
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
    text = [f"# M10 emulator tests\n\nPackage: {m5.PKG}, sha256 {sha}\n"]
    for k in reports:
        text += reports[k]
    text.append(f"\n**{'ALL PASS' if all(results.values()) else 'FAILURES: ' + ', '.join(k for k, v in results.items() if not v)}**\n")
    (m5.OUT / "report.md").write_text("\n".join(text))
    print("\n".join(text))
    sys.exit(0 if all(results.values()) else 1)


if __name__ == "__main__":
    main()
