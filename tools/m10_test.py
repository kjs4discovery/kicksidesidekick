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
M10_STRINGS = (("vp_title", 32), ("vp_value", 40), ("vp_sub", 40), ("next_text", 48), ("ind_name", 16), ("info_text", 40),
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
    check(I["loop"]["ind_blink_starts"] == 0 and I["loop"]["ind_name"] == "LOOP",   # (M10.1-02: LOOP structure says LOOP)
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
    check('#define APP_NAME "KickSide - Your Musical SideKick"' in ssrc and '#define FW_MILESTONE "m10.1"' in ssrc and   # (M10.1-02: the name and milestone follow M10.1)
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


# --------------------------------------------------------------------------------------------- part 5
M9_PKG = ROOT / "build/step-M9/fm1acc-0.9.0-m9.fwsc"


def test_m10p5(all_songs, rep):
    lines, st, check = m5.checker("M10 part 5: the song info line on the Settings screen (D062)")
    import shutil
    from concurrent.futures import ThreadPoolExecutor
    O = m5.OPEN_MS
    slugs = [c["slug"] for c in all_songs]
    i_saints = slugs.index("saints")
    settings = lambda t: P(t, "GLO") + P(t + 400, "GLO")
    # a copy of saints without its info line, sent alone (a song with no INFO, as a private or imported one)
    d = m5.OUT / "saints-noinfo" / "saints-noinfo"
    shutil.rmtree(d.parent, ignore_errors=True)
    shutil.copytree(ROOT / "songs/saints", d)
    y = d / "song.yaml"
    y.write_text("".join(l for l in y.read_text().splitlines(True) if not l.startswith("info:")).replace("slug: saints", "slug: saints-noinfo"))
    with ThreadPoolExecutor(3) as ex:
        a = ex.submit(lambda: m5.run("m10p5-info", O + 2600, m5.open_song(i_saints) + settings(O + 1200)))
        b = ex.submit(lambda: m5.run("m10p5-list", 6600, settings(4600)))
        c0 = ex.submit(lambda: m5.run("m10p5-noinfo-send", 120000, [], flash="empty_quiet",
                                       host=f"{m5.SEND} send --emu-pipe {d}"))
        a, b, c0 = a.result(), b.result(), c0.result()
        c = ex.submit(lambda: m5.run("m10p5-noinfo", O + 2600, m5.open_song(0) + settings(O + 1200), flash=c0["flash"])).result()
    A, Bs, C = a["stats"], b["stats"], c["stats"]
    check(A["ui_view"] == V_SETTINGS and A["info_shown"] == 1 and A["info_text"] == "Traditional spiritual, c. 1896",
          f"a song with INFO, from its play screen: Settings shows its info line {A['info_text']!r} (m10p5-info.png)")
    check(C["ui_view"] == V_SETTINGS and C["info_shown"] == 0 and C["info_text"] == "",
          f"a song without INFO (as a private one): no line (view {C['ui_view']}; m10p5-noinfo.png)")
    check(Bs["ui_view"] == V_SETTINGS and Bs["info_shown"] == 0,
          f"Settings opened from the list (no song loaded): no line (m10p5-list.png)")
    # BACKTRACK and SYNTH: the Settings screens there never show it (BACKTRACK's play screen: GLO GLO)
    bt = btrun("m10p5-bt", 7000, P(4500, "PLAY") + settings(5600))
    check(bt["stats"]["info_shown"] == 0, "BACKTRACK: no info line")
    # the M9 package must open a file that has an INFO chunk (it skips chunks it does not know)
    if M9_PKG.exists():
        send = m5.run("m10p5-m9-send", 120000, [], flash="empty_quiet", pkg=M9_PKG,
                      host=f"{m5.SEND} send --emu-pipe {ROOT}/songs/saints {ROOT}/songs/clementine")
        play = m5.run("m10p5-m9-play", O + 3500, P(O, "PLAY") + P(O + 800, "PLAY") + P(O + 1600, "PLAY"), flash=send["flash"], pkg=M9_PKG)
        s = play["stats"]
        check(not send["halt"] and not play["halt"] and "stored" in send["out"] and s["starts"] >= 1,
              f"the M9 package: a file with an INFO chunk is accepted, opened and played, no crash (stored: "
              f"{'stored' in send['out']}, starts {s['starts']})")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 6
KITS = ("STUDIO", "ACOUSTIC", "ELECTRO", "CHIP")
KIT_PROGS = {0: (0xFF, 0xFF), 1: (32, 24), 2: (38, 89), 3: (38, 4)}      # bass, harmony (GM counted from 0)


def seqs(n, t0=4280):
    """n SEQ presses on SETUP (130 ms apart, short: BACKTRACK's SETUP is up from 4.2 s; PLAY is at 4.7 s)"""
    return [x for k in range(n) for x in (f"{t0 + 130 * k}:SEQ!", f"{t0 + 130 * k + 50}:SEQ^!")]


def test_m10p6(all_songs, rep):
    lines, st, check = m5.checker("M10 part 6: band kits for BACKTRACK (D062)")
    from concurrent.futures import ThreadPoolExecutor
    heavy = m8.PRESETS.index("MIAMI PAD")
    jobs = {}
    for n in range(0, 5):                                           # SEQ pressed n times on SETUP
        jobs[f"seq{n}"] = (lambda n: lambda: btrun(f"m10p6-seq{n}", 4500 + 300 * n + 500,
                                                   [x for k in range(n) for x in P(4500 + 300 * k, "SEQ")]))(n)
    jobs["play-seq"] = lambda: named("p6-play-seq", P(7000, "SEQ"), 7800)                  # SEQ on the play screen: the pop-up
    # every kit in every style, FULL, BUSY (complexity 3): hits per kit, the notes identical across kits
    for sty in range(11):
        for kit in range(4):
            sc = (["4300:e4:1", "4350:e1:2"] + ([f"4380:e3:{sty}"] if sty else []) +
                  seqs(kit))
            jobs[(sty, kit)] = (lambda sc, sty, kit: lambda: named(f"p6-{sty}-{kit}", [], 9500, setup=sc))(sc, sty, kit)
    # the CPU with each kit: the M9 worst case (FULL, BUSY, a chorus, MIAMI PAD x 4 voices, a live switch)
    for sty in (0, 1, 9):
        for kit in range(4):
            sc = ["4300:e4:1"] + ([f"4350:e3:{sty}"] if sty else []) + ["4400:e1:2", "4450:e0:150"] + \
                seqs(kit) + P(4700, "PLAY") + \
                [f"5300:e5:{heavy}", "17000:e3:1"] + [f"17600:{k}" for k in m9.KEYS4] + [f"24500:{k}^" for k in m9.KEYS4]
            jobs[("cpu", sty, kit)] = (lambda sc, sty, kit: lambda: btrun(f"m10p6-cpu-{sty}-{kit}", 25000, sc))(sc, sty, kit)
    with ThreadPoolExecutor(3) as ex:
        futs = {k: ex.submit(f) for k, f in jobs.items()}
        R = {k: f.result() for k, f in futs.items()}
    S = {k: v["stats"] for k, v in R.items()}
    seq = [(S[f"seq{n}"]["kit"], S[f"seq{n}"]["vp_title"], S[f"seq{n}"]["vp_value"]) for n in range(5)]
    check([x[0] for x in seq] == [0, 1, 2, 3, 0] and all(x[1] == "Band Kit" for x in seq[1:]) and
          [x[2] for x in seq[1:]] == ["ACOUSTIC", "ELECTRO", "CHIP", "STUDIO"] and S["seq0"]["vp_open"] == 0 and
          S["seq1"]["ui_view"] == V_SETUP,
          f"SEQ on SETUP steps STUDIO -> ACOUSTIC -> ELECTRO -> CHIP -> STUDIO with the pop-up \"Band Kit\" {seq} "
          f"(m10p6-seq2.png: the KIT row; STUDIO at power-on: reset)")
    ps = S["play-seq"]
    check(ps["popup_open"] == 1 and ps["popup_btn"] == B["SEQ"] and ps["kit"] == 0 and ps["vp_value"] != "ACOUSTIC",
          "SEQ on the play screen stays NOT IMPLEMENTED (the pop-up); the kit does not change")
    bad = []
    for sty in range(11):
        base = S[(sty, 0)]
        for kit in range(4):
            s = S[(sty, kit)]
            drums_vsynth = kit >= 2
            if not (s["kit"] == kit and (s["kit_bass_prog"], s["kit_harm_prog"]) == KIT_PROGS[kit] and
                    s["notes_on"][:3] == base["notes_on"][:3] and all(x > 0 for x in s["notes_on"][:3]) and
                    ((s["kit_hits"] > 0) == drums_vsynth) and (not drums_vsynth or s["kit_hits"] == base["notes_on"][0]
                                                               or s["kit_hits"] > 0) and not R[(sty, kit)]["halt"]):
                bad.append((sty, kit, s["kit"], s["notes_on"][:3], base["notes_on"][:3], s["kit_hits"]))
    check(not bad, f"every kit plays in every one of the 11 styles (FULL, BUSY): hits counted per kit (ELECTRO / CHIP: the drum "
                   f"voices; STUDIO / ACOUSTIC: EAS), the notes (drums, bass, harmony note-ons) identical across the kits {bad[:3] or ''}")
    cpu = {}
    for key in S:
        if isinstance(key, tuple) and key[0] == "cpu":
            s = S[key]
            cpu[(key[1], key[2])] = ((s["render_max"] + s["synth_render_max"]) / s["budget"], s["synth_voices_max"], s["swaps"],
                                     R[key]["halt"], s["kit"])
    worst = {k: max(v[0] for kk, v in cpu.items() if kk[1] == k) for k in range(4)}
    lines.append("  - CPU (EAS + synth render of a block / its time) per kit, worst of POP, ROCK, FUNK: " +
                 ", ".join(f"{KITS[k]} {worst[k] * 100:.0f} %" for k in range(4)))
    check(all(w <= 0.60 for w in worst.values()) and all(not v[3] and v[1] == 4 and v[2] >= 1 and v[4] == kk[1]
                                                          for kk, v in cpu.items()),
          f"the worst case (FULL, BUSY in a chorus, MIAMI PAD x 4 voices, a live switch) stays at or under 60 % with every kit: "
          f"{', '.join(f'{KITS[k]} {worst[k] * 100:.0f} %' for k in range(4))}")
    st["cpu_worst"] = worst
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 7
SYNTH_PRE = ["4050:e0:2", "4200:PLAY", "4260:PLAY^"]       # the mode screen: SELECT to SYNTH, PLAY


def synth_run(name, ms, script, **kw):
    return m5.run(name, ms, SYNTH_PRE + script, mode=None, **kw)


def test_m10p7(all_songs, rep):
    lines, st, check = m5.checker("M10 part 7: saving user presets (D040, D062)")
    from concurrent.futures import ThreadPoolExecutor
    K1, K2 = "e2", "e3"
    with ThreadPoolExecutor(3) as ex:
        o = ex.submit(lambda: synth_run("m10p7-open", 5600, P(5000, "SAVE")))
        e = ex.submit(lambda: synth_run("m10p7-edit", 6400, P(5000, "SAVE") + [f"5400:{K1}:2", f"5450:{K2}:1", f"5500:e0:4"]))
        sv = ex.submit(lambda: synth_run("m10p7-save", 7600, P(5000, "SAVE") + [f"5400:{K1}:2", f"5450:{K2}:1", f"5500:e0:4"] +
                                         P(6000, "SAVE")))
        o, e, sv = o.result(), e.result(), sv.result()
    O, E, S = o["stats"], e["stats"], sv["stats"]
    check(O["save_screen"] == 1 and O["save_slot"] == 0 and O["next_text"] == "E-PIANO       "[:7].strip().ljust(12) or O["next_text"].startswith("E-PIANO"),
          f"SAVE opens the SAVE screen at the first empty slot (U01) with the current preset's name {O['next_text']!r} "
          f"(m10p7-open.png)")
    check(E["next_text"].startswith("E-QIANO") and E["save_slot"] == 4 and E["save_screen"] == 1,
          f"K1 moves the cursor, K2 changes the character (E-PIANO -> {E['next_text']!r}); SELECT chose slot U05 "
          f"(slot index {E['save_slot']}; m10p7-edit.png)")
    check(S["preset_saves"] == 1 and S["preset_used"] == 1 and S["preset_name"] == "U05 E-QIANO" and S["vp_title"] == "SAVED U05"
          and S["save_screen"] == 0 and sv["ops"] == 10,
          f"SAVE again saves: the pop-up {S['vp_title']!r}, the sound goes on as U05 ({S['preset_name']!r}), one erase and 9 "
          f"page writes ({sv['ops']} flash operations; m10p7-save.png)")
    # replace: the slot is taken: SAVE, SAVE (the question), HOME (cancel: nothing written), then SAVE, SAVE, SAVE (replaced)
    base = P(5000, "SAVE") + [f"5400:{K1}:2", f"5450:{K2}:1", "5500:e0:4"] + P(6000, "SAVE")
    with ThreadPoolExecutor(3) as ex:
        q = ex.submit(lambda: synth_run("m10p7-ask", 9000, base + P(7800, "SAVE") + P(8100, "SAVE"), flash="songs"))
        c = ex.submit(lambda: synth_run("m10p7-cancel", 9800, base + P(7800, "SAVE") + P(8100, "SAVE") + P(8400, "HOME"),
                                         flash="songs"))
        r2 = ex.submit(lambda: synth_run("m10p7-replace", 10600, base + P(7800, "SAVE") + [f"8000:{K2}:1"] + P(8100, "SAVE") +
                                          P(8400, "SAVE") + P(8700, "SAVE"), flash="songs"))
        q, c, r2 = q.result(), c.result(), r2.result()
    Q, C, R2 = q["stats"], c["stats"], r2["stats"]
    check(Q["save_screen"] == 2 and Q["preset_saves"] == 1,
          "a taken slot: the second SAVE asks \"REPLACE U05 <name>?  SAVE = yes  HOME = no\" (m10p7-ask.png)")
    check(C["save_screen"] == 0 and C["preset_saves"] == 1 and c["ops"] == q["ops"] == 10 and C["ui_view"] == V_SYNTH,
          f"HOME = no: back, nothing written (flash operations {c['ops']}, saves {C['preset_saves']})")
    check(R2["preset_saves"] == 2 and r2["ops"] == 20 and R2["preset_used"] == 1,
          f"yes: the slot is replaced ({R2['preset_saves']} saves, {r2['ops']} flash operations, still {R2['preset_used']} "
          f"preset; the other copy of the A/B pair)")
    # the preset list: PRESET knob (e6): 30 stock, then the saved ones; BACKTRACK's K4 the same (a flash with the preset in it)
    fl = sv["flash"]
    with ThreadPoolExecutor(3) as ex:
        a = ex.submit(lambda: synth_run("m10p7-list", 5500, [f"5000:e6:30"], flash=fl))
        b = ex.submit(lambda: synth_run("m10p7-list-wrap", 5500, [f"5000:e6:31"], flash=fl))
        bt = ex.submit(lambda: m5.run("m10p7-bt-k4", 5600, ["4050:e0:1", "4200:PLAY", "4260:PLAY^", "4500:e5:30"], mode=None, flash=fl))
        boot = ex.submit(lambda: synth_run("m10p7-boot", 5200, [], flash=fl))
        a, b, bt, boot = a.result(), b.result(), bt.result(), boot.result()
    check(a["stats"]["preset_name"] == "U05 E-QIANO" and b["stats"]["preset_name"] == "E-PIANO" and
          boot["stats"]["preset_used"] == 1,
          f"after a power cycle the preset is there; PRESET +30 reaches {a['stats']['preset_name']!r}, one more wraps to "
          f"{b['stats']['preset_name']!r}")
    check(bt["stats"]["vp_value"] == "U05 E-QIANO" and bt["stats"]["vp_title"] == "Melody Sound",
          f"BACKTRACK's K4 goes through the same list: {bt['stats']['vp_value']!r}")
    # junk in the object's place at boot: all empty, no flash operation, no crash; a hand-made old record loads with defaults
    import random
    import zlib
    flash = bytearray(Path(m5.FLASH["songs"]).read_bytes())
    rnd = random.Random(5)
    flash[0xDE000:0xE0000] = bytes(rnd.randrange(256) for _ in range(0x2000))
    junk = m5.OUT / "flash" / "junk-presets.bin"
    junk.write_bytes(flash)
    jb = synth_run("m10p7-junk", 5200, [], flash=str(junk))
    check(jb["stats"]["preset_used"] == 0 and jb["ops"] == 0 and not jb["halt"] and jb["stats"]["ui_view"] == V_SYNTH,
          f"junk in 0xDE000..0xDFFFF at boot: no preset, no crash, no flash operation ({jb['ops']})")
    old = obj_image(m5.FLASH["songs"], [(1, 1, "OLD ONE", 0xFF)])
    ob = synth_run("m10p7-old", 5600, ["5000:e6:30"], flash=str(old))
    check(ob["stats"]["preset_used"] == 1 and ob["stats"]["preset_name"] == "U01 OLD ONE" and ob["stats"]["synth_level"] == 100,
          f"a record with parameters out of range (an older version's missing ones) loads with the defaults: the LEVEL "
          f"{ob['stats']['synth_level']} (default 100)")
    rep += lines
    return st["ok"]


def obj_image(base_flash, recs):
    """a flash image with a user-preset object (fm1-base's store format: a header with the commit record, the payload at +256)
    in copy A of 0xDE000; recs: [(slot, engine, name, level byte)] for slots 0..: the record's version 1, parameters = the
    level byte last (0xFF: out of range: the default)"""
    import struct
    import zlib
    flash = bytearray(Path(base_flash).read_bytes())
    slots = [bytearray(64) for _ in range(30)]
    for slot, eng, name, level in recs:
        r = slots[slot - 1]
        r[0], r[1] = 1, eng
        r[2:2 + len(name)] = name.encode()
        for k in range(37):
            r[14 + k] = 64 if k != 35 else level
    payload = struct.pack("<IBBH", 0x53525055, 1, 30, 0) + b"".join(bytes(s) for s in slots)
    hdr = struct.pack("<IHHIII2I", 0x30583058, 2, 0, 1, len(payload), zlib.crc32(payload), 0, 0)
    hdr += struct.pack("<I", zlib.crc32(hdr))
    flash[0xDE000:0xE0000] = b"\xff" * 0x2000
    flash[0xDE000:0xDE000 + len(hdr)] = hdr
    flash[0xDE000 + 256:0xDE000 + 256 + len(payload)] = payload
    out = m5.OUT / "flash" / "old-presets.bin"
    out.write_bytes(flash)
    return out


PARTS = {"m10p1": test_m10p1, "m10p2": test_m10p2, "m10p3": test_m10p3, "m10p4": test_m10p4, "m10p5": test_m10p5,
         "m10p6": test_m10p6, "m10p7": test_m10p7}


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
