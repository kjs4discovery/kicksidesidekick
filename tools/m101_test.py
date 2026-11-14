#!/usr/bin/env python3
# m101_test.py -- Milestone M10.1: headless emulator tests (built on tools/m10_test.py's runner and stats; same structure).
# Never touches the FM-1. Licence: TBD (see D017)
#
#   .venv/bin/python -I tools/m101_test.py [--only m101p1,m101p2,..] [--pkg PKG] [--out DIR]   -> build/emu/m101/report.md
import argparse
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import m5_test as m5                     # noqa: E402
import m8_test as m8                     # noqa: E402
import m10_test as m10                   # noqa: E402

btrun, bt_flash = m8.btrun, m8.bt_flash
V_LIST, V_PLAY, V_HELP, V_SETTINGS, V_MODE, V_SYNTH, V_SETUP = m8.V_LIST, m8.V_PLAY, m8.V_HELP, m8.V_SETTINGS, \
    m8.V_MODE, m8.V_SYNTH, m8.V_SETUP
P = m5.press_raw
O = m5.OPEN_MS
BAR = 1920
SYNTH_PRE, synth_run, named = m10.SYNTH_PRE, m10.synth_run, m10.named

# --------------------------------------------------------------------------------------------- stats
# M10.1 appends two texts after M10's (player.h): the SYNTH title as drawn last, the bottom instruction row as drawn last
_stats10 = m5.stats


def stats101(ram):
    s = _stats10(ram)
    if s is None:
        return s
    i = ram.find(b"PLST")
    while i >= 0 and struct.unpack_from("<I", ram, i + 4)[0] not in (3, 4):
        i = ram.find(b"PLST", i + 1)
    base = i + m5.STATS.size + m5.STATS4.size + 128 + (18 + 14) * 4
    s["mel_sound"], s["vols"] = s["pad0"], s["pad1"]       # (player.h: M10.1 named the two spare words)
    at = base + 4 * len(m10.M10_FIELDS) + sum(n for _, n in m10.M10_STRINGS)
    for name, n in (("title_now", 48), ("legend_now", 64)):
        s[name] = ram[at:at + n].split(b"\0")[0].decode("latin-1")
        at += n
    return s


m5.stats = stats101


def _margins_clear(img, y0, y1):
    return m10._margins_clear(img, y0, y1)


def learn_run(name, ms, extra, i=None, **kw):
    """SONG LEARN with a song open (the play screen, stopped); extra: events after the opening"""
    i = SONG_I if i is None else i
    return m5.run(name, ms, m5.open_song(i) + extra, **kw)


SONG_I = 0


# --------------------------------------------------------------------------------------------- part 1
def test_m101p1(all_songs, rep):
    global SONG_I
    lines, st, check = m5.checker("M10.1 part 1: the name, edit page titles, hint pages 6 s, LOOP, value pop-ups")
    from concurrent.futures import ThreadPoolExecutor
    slugs = [c["slug"] for c in all_songs]
    SONG_I = slugs.index("saints")
    sy_pre = ["4050:e0:2", "4200:PLAY", "4260:PLAY^"]
    # a flash with one saved preset in slot 30 with a 12-character name (the longest the editor holds)
    flash_long = m10.obj_image(m5.FLASH["songs"], [(30, 1, "WWWWWWWWWWWW", 0xFF)])
    jobs = {
        "splash": lambda: m5.run("m101p1-splash", 2500, [], mode=None),
        "mode": lambda: m5.run("m101p1-mode", 4700, [], mode=None),
        "mode-bt": lambda: m5.run("m101p1-mode-bt", 5200, ["4050:e0:1", "4400:PLAY", "4460:PLAY^"], mode=None, flash=bt_flash()),
        "mode-sy": lambda: m5.run("m101p1-mode-sy", 5200, ["4050:e0:2", "4400:PLAY", "4460:PLAY^"], mode=None),
        "mode-learn": lambda: m5.run("m101p1-mode-learn", 5200, ["4400:PLAY", "4460:PLAY^"], mode=None),
        "t-env": lambda: synth_run("m101p1-t-env", 5600, P(5000, "ENV")),
        "t-fx2": lambda: synth_run("m101p1-t-fx2", 5800, P(5000, "FX") + P(5300, "FX"), ),
        "t-user": lambda: synth_run("m101p1-t-user", 5900, ["4600:e6:30"] + P(5200, "ENV"), flash=str(flash_long)),
        "t-user-fx4": lambda: synth_run("m101p1-t-user-fx4", 6200, ["4600:e6:30"] + P(5000, "FX") + P(5200, "FX") + P(5400, "FX") + P(5600, "FX"),
                                        flash=str(flash_long)),
        # the bottom row: SYNTH main and SONG LEARN stopped, at 5.2 / 5.9 / 6.1 / 8.9 / 9.1 s
        "h-sy-52": lambda: synth_run("m101p1-h-sy-52", 5200, []),
        "h-sy-59": lambda: synth_run("m101p1-h-sy-59", 5900, []),
        "h-sy-61": lambda: synth_run("m101p1-h-sy-61", 6100, []),
        "h-sy-89": lambda: synth_run("m101p1-h-sy-89", 8900, []),
        "h-sy-91": lambda: synth_run("m101p1-h-sy-91", 9100, []),
        "h-le-59": lambda: learn_run("m101p1-h-le-59", 5900, []),
        "h-le-61": lambda: learn_run("m101p1-h-le-61", 6100, []),
        "h-le-89": lambda: learn_run("m101p1-h-le-89", 8900, []),
        "h-le-91": lambda: learn_run("m101p1-h-le-91", 9100, []),
        # LOOP in BACKTRACK
        "loop": lambda: named("m101p1-loop", [], 5660 + 20 * 960, setup=[]),
        "loop-play": lambda: named("m101p1-loop-play", [], 5660 + 2 * 960, setup=[]),
        "full": lambda: named("m101p1-full", [], 5660 + 960 + 500, setup=["4400:e1:2"]),
        "vc": lambda: named("m101p1-vc", [], 5660 + 15 * 960 + 500, setup=["4400:e1:1"]),
    }
    # pop-ups: SONG LEARN (K1-K3, SELECT, PRESETS, ALGO) and SYNTH (PRESET, SELECT, K1-K4)
    for k, e in (("K1", "e2"), ("K2", "e3"), ("K3", "e4")):
        jobs[f"vp-le-{k}"] = (lambda k, e: lambda: learn_run(f"m101p1-vp-le-{k}", O + 900, [f"{O + 500}:{e}:1"]))(k, e)
    jobs["vp-le-SEL"] = lambda: learn_run("m101p1-vp-le-SEL", O + 900, [f"{O + 500}:e0:3"])
    jobs["vp-le-PRE"] = lambda: learn_run("m101p1-vp-le-PRE", O + 900, [f"{O + 500}:e6:1"])
    jobs["vp-le-ALG"] = lambda: learn_run("m101p1-vp-le-ALG", O + 900, [f"{O + 500}:e1:2"])
    jobs["vp-le-none"] = lambda: learn_run("m101p1-vp-le-none", O + 900, [])
    jobs["vp-sy-PRE"] = lambda: synth_run("m101p1-vp-sy-PRE", 5500, ["5000:e6:3"])
    jobs["vp-sy-SEL"] = lambda: synth_run("m101p1-vp-sy-SEL", 5500, ["5000:e0:5"])
    for k, e in (("K1", "e2"), ("K2", "e3"), ("K3", "e4"), ("K4", "e5")):
        jobs[f"vp-sy-{k}"] = (lambda k, e: lambda: synth_run(f"m101p1-vp-sy-{k}", 5500, [f"5000:{e}:4"]))(k, e)
    jobs["vp-sy-page"] = lambda: synth_run("m101p1-vp-sy-page", 5800, P(5000, "ENV") + [f"5300:e2:4"])
    with ThreadPoolExecutor(3) as ex:
        futs = {k: ex.submit(f) for k, f in jobs.items()}
        R = {k: f.result() for k, f in futs.items()}
    S = {k: r["stats"] for k, r in R.items()}
    # ---- 1. the name
    app_h = (ROOT / "firmware/app/app.h").read_text()
    words = (ROOT / "firmware/app/words.tsv").read_text()
    left = [str(p.relative_to(ROOT)) for d in ("firmware/app", "tools", "docs/controls.md") for p in
            ((ROOT / d).rglob("*") if (ROOT / d).is_dir() else [ROOT / d])
            if p.is_file() and p.suffix in (".c", ".h", ".tsv", ".toml") and "build" not in p.parts
            and "Tuneka" in p.read_text(errors="ignore")]
    check('#define APP_NAME "KickSide - Your Musical SideKick"' in app_h and
          "KickSide - Your Musical SideKick\tKickSide, your musical sidekick" in words and not left,
          f"the name is \"KickSide - Your Musical SideKick\" in app.h and words.tsv (spoken form \"KickSide, your musical sidekick\"); "
          f"\"Tuneka\" is gone from the firmware sources {left or ''}")
    simg = R["splash"]["lcd"].convert("RGB").resize((240, 240))
    mimg = R["mode"]["lcd"].convert("RGB").resize((240, 240))
    bg = mimg.getpixel((0, 0))
    ink = sum(1 for x in range(4, 236) for y in range(2, 18) if mimg.getpixel((x, y)) != bg)
    sink = sum(1 for x in range(4, 236) for y in range(190, 214) if simg.getpixel((x, y)) != simg.getpixel((0, 0)))
    check(S["mode"]["ui_view"] == V_MODE and ink > 150 and _margins_clear(mimg, 0, 20) and sink > 150 and _margins_clear(simg, 188, 216),
          f"the name fits on one line across the mode screen ({ink} text pixels, 4 px margins clear) and on the splash ({sink}) "
          f"(m101p1-mode.png, m101p1-splash.png)")
    check(S["mode"]["mode_sel"] == 0 and S["mode-learn"]["ui_view"] == V_LIST and S["mode-bt"]["ui_view"] == V_SETUP and
          S["mode-sy"]["ui_view"] == V_SYNTH and 'NAME[MODES] = {"SONG LEARN", "BACKTRACK", "SYNTH"}' in
          (ROOT / "firmware/app/synthui.c").read_text(),
          f"the mode order is unchanged: SONG LEARN (view {S['mode-learn']['ui_view']}), BACKTRACK ({S['mode-bt']['ui_view']}), "
          f"SYNTH ({S['mode-sy']['ui_view']}); the cursor starts on SONG LEARN")
    # ---- 2. the edit page titles
    long_img = R["t-user"]["lcd"].convert("RGB").resize((240, 240))
    check(S["t-env"]["title_now"] == "ENVELOPE - E-PIANO" and S["t-fx2"]["title_now"] == "FX 2 - E-PIANO" and
          S["t-user"]["title_now"] == "ENVELOPE - U30 WWWWWWWWWWWW" and S["t-user-fx4"]["title_now"] == "FX 4 - U30 WWWWWWWWWWWW",
          f"an edit page's title names the preset: {S['t-env']['title_now']!r}, {S['t-fx2']['title_now']!r}, a saved one with a "
          f"12-character name {S['t-user']['title_now']!r}, {S['t-user-fx4']['title_now']!r} (m101p1-t-env.png, m101p1-t-user.png)")
    bg = long_img.getpixel((0, 0))
    right = sum(1 for x in range(200, 236) for y in range(6, 24) if long_img.getpixel((x, y)) != bg)
    check(_margins_clear(long_img, 0, 30) and right > 20 and
          all(long_img.getpixel((x, y)) == bg for x in range(4, 8) for y in range(0, 30)) or _margins_clear(long_img, 0, 30),
          f"the long title ({S['t-user']['title_now']!r}) is drawn whole inside the 4 px margins, beside the page number "
          f"({right} px of the page number at the right)")
    # the title text and the page number do not touch: a clear column between the title's right end and the page number
    cols = [x for x in range(4, 236) if any(long_img.getpixel((x, y)) != bg for y in range(2, 26))]
    gaps = [b - a for a, b in zip(cols, cols[1:])]
    check(gaps and max(gaps) >= 6, f"a gap of {max(gaps) if gaps else 0} px separates the title from the page number")
    # ---- 3. the bottom row's pages last 6 s
    def leg(k):
        return S[k]["legend_now"]
    check(leg("h-sy-52") == leg("h-sy-59") != leg("h-sy-61") == leg("h-sy-89") == leg("h-sy-91"),
          f"SYNTH: the bottom row shows one page at 5.2 s and 5.9 s ({leg('h-sy-59')!r}), the next at 6.1 s ({leg('h-sy-61')!r}) and "
          f"still at 8.9 s and 9.1 s ({leg('h-sy-91')!r})")
    check(leg("h-le-59") != leg("h-le-61") == leg("h-le-89") == leg("h-le-91") and "pick part" in leg("h-le-61") + leg("h-le-59"),
          f"SONG LEARN (stopped): one page at 5.9 s ({leg('h-le-59')!r}), the next at 6.1 s ({leg('h-le-61')!r}), the same at 8.9 s and 9.1 s")
    src = "".join((ROOT / "firmware/app" / f).read_text() for f in ("ui.c", "synthui.c", "bt.c", "btview.c"))
    check("3000u" not in src and src.count("HINT_PAGE_MS") >= 4 and "#define HINT_PAGE_MS 6000u" in app_h,
          "one named constant HINT_PAGE_MS 6000u in every place; no 3000u left in ui.c, synthui.c, bt.c")
    # ---- 4. LOOP
    check(S["loop"]["ind_name"] == "LOOP" and S["loop"]["ind_blink_starts"] == 0 and S["loop-play"]["ind_name"] == "LOOP" and
          S["full"]["ind_name"] != "LOOP" and S["vc"]["ind_name"] == "VERSE" and S["vc"]["ind_blink_starts"] >= 1,
          f"BACKTRACK LOOP structure: the indicator says {S['loop']['ind_name']!r}, never blinks (blinks {S['loop']['ind_blink_starts']}); "
          f"FULL says {S['full']['ind_name']!r}; VERSE/CHORUS says {S['vc']['ind_name']!r} and still blinks ({S['vc']['ind_blink_starts']}) "
          f"(m101p1-loop.png)")
    # ---- 5. the pop-ups
    want = {"vp-le-K1": ("Drums Level", r"2 of \d"), "vp-le-K2": ("Bass Level", r"2 of \d"), "vp-le-K3": ("Harmony Level", r"2 of \d"),
            "vp-le-SEL": ("Tempo", r"\d+ BPM"), "vp-le-PRE": ("Start", r".+"), "vp-le-ALG": ("Start", r"BAR \d+"),
            "vp-sy-PRE": ("Preset", r".+"), "vp-sy-SEL": ("Tempo", r"1\d\d BPM"), "vp-sy-K1": ("CUTOFF", r".+"),
            "vp-sy-K2": ("RESONANCE", r".+"), "vp-sy-K3": ("REVERB MIX", r".+"), "vp-sy-K4": ("DELAY MIX", r".+")}
    bad = []
    for k, (t, v) in want.items():
        r = S[k]
        if not (r["vp_open"] and r["vp_title"] == t and re.fullmatch(v, r["vp_value"]) and r["vp_count"] >= 1):
            bad.append((k, r["vp_open"], r["vp_title"], r["vp_value"]))
    check(not bad and not S["vp-le-none"]["vp_open"] and not S["vp-sy-page"]["vp_open"],
          f"a pop-up after each listed control: {[(k, S[k]['vp_title'], S[k]['vp_value']) for k in want]} {bad or ''}; none without a "
          f"turn; none on an edit page's K1 ({S['vp-sy-page']['vp_open']}) (m101p1-vp-le-K1.png, m101p1-vp-sy-K1.png)")
    check(not S["vp-sy-K1"]["legend_now"].startswith("CUTOFF"),
          f"SYNTH's bottom row no longer shows the turned value (it shows {S['vp-sy-K1']['legend_now']!r})")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 2
def hold(ms, name, dur):
    return [f"{ms}:{name}!", f"{ms + dur}:{name}^!"]


def vols(s):
    return [(s["vols"] >> (2 * k)) & 3 for k in range(4)]


def test_m101p2(all_songs, rep):
    global SONG_I
    lines, st, check = m5.checker("M10.1 part 2: layer buttons, HOME climbs to the mode screen, K4 = Melody Sound")
    from concurrent.futures import ThreadPoolExecutor
    slugs = [c["slug"] for c in all_songs]
    SONG_I = slugs.index("saints")
    t0 = O + 300
    BTN = ("FX", "SCL", "ENV", "LFO")
    jobs = {}

    def presses(seq, step=300):
        return [x for k, b in enumerate(seq) for x in P(t0 + step * k, b)]
    seqs_ = {"s-fx": ("FX",), "s-fx2": ("FX", "FX"), "s-fx3": ("FX", "FX", "FX"), "s-fx4": ("FX", "FX", "FX", "FX"),
             "s-env": ("ENV",), "s-env2": ("ENV", "ENV"), "s-scl-fx": ("SCL", "FX"), "s-lfo": ("LFO",),
             # volumes kept when the part moves: harmony (yours) QUIET; FX = part drums; FX = drums QUIET; ENV = part harmony
             "keep": ("ENV", "FX", "FX", "ENV"), "keep2": ("ENV", "FX", "FX", "ENV", "ENV")}
    for k, sq in seqs_.items():
        jobs[k] = (lambda k, sq: lambda: learn_run(f"m101p2-{k}", t0 + 300 * len(sq) + 400, presses(sq)))(k, sq)
    jobs["hold-sel"] = lambda: learn_run("m101p2-hold-sel", t0 + 2500, hold(t0, "SCL", 1500))
    jobs["hold-then-press"] = lambda: learn_run("m101p2-hold-then-press", t0 + 3200, hold(t0, "SCL", 1500) + P(t0 + 2000, "SCL"))
    # BACKTRACK: the same rule on its layer buttons (the melody: select only)
    jobs["bt-lfo"] = lambda: named("m101p2-bt-lfo", P(6000, "LFO"), 6500)
    jobs["bt-fx"] = lambda: named("m101p2-bt-fx", P(6000, "FX"), 6400)
    jobs["bt-fx2"] = lambda: named("m101p2-bt-fx2", P(6000, "FX") + P(6300, "FX"), 6800)
    # HOME climbs: SONG LEARN (play -> song list -> folders -> mode), BACKTRACK (play playing: stops; SETUP; mode), SYNTH
    for n in (1, 2, 3):
        jobs[f"h-le{n}"] = (lambda n: lambda: learn_run(f"m101p2-h-le{n}", t0 + 300 * n + 300, [x for k in range(n) for x in P(t0 + 300 * k, "HOME")]))(n)
        jobs[f"h-bt{n}"] = (lambda n: lambda: named(f"m101p2-h-bt{n}", [x for k in range(n) for x in P(6000 + 400 * k, "HOME")], 6000 + 400 * n + 300))(n)
    jobs["h-setup"] = lambda: btrun("m101p2-h-setup", 5000, P(4500, "HOME"))
    jobs["h-list"] = lambda: m5.run("m101p2-h-list", 5100, P(4400, "HOME") + P(4700, "HOME"))
    jobs["h-sy-page"] = lambda: synth_run("m101p2-h-sy-page", 5400, P(5000, "ENV") + P(5200, "HOME"))
    jobs["h-sy-main"] = lambda: synth_run("m101p2-h-sy-main", 5700, P(5000, "ENV") + P(5200, "HOME") + P(5400, "HOME"))
    jobs["h-sy-main-only"] = lambda: synth_run("m101p2-h-sy-main-only", 5400, P(5000, "HOME"))
    jobs["h-sy-save"] = lambda: synth_run("m101p2-h-sy-save", 5600, P(5000, "SAVE") + P(5300, "HOME"))
    # a hold of 2 s goes straight to the mode screen; the press itself does nothing before that
    jobs["hold-le-mid"] = lambda: learn_run("m101p2-hold-le-mid", t0 + 1200, hold(t0, "HOME", 1500))
    jobs["hold-le-1500"] = lambda: learn_run("m101p2-hold-le-1500", t0 + 1800, hold(t0, "HOME", 1500))
    jobs["hold-le-2300"] = lambda: learn_run("m101p2-hold-le-2300", t0 + 3000, hold(t0, "HOME", 2300))
    jobs["hold-bt-2300"] = lambda: named("m101p2-hold-bt-2300", hold(6000, "HOME", 2300), 9000)
    jobs["hold-sy-mid"] = lambda: synth_run("m101p2-hold-sy-mid", 6400, P(5000, "ENV") + hold(5200, "HOME", 1500))
    jobs["hold-sy-2300"] = lambda: synth_run("m101p2-hold-sy-2300", 8000, P(5000, "ENV") + hold(5200, "HOME", 2300))
    base = P(5000, "SAVE") + ["5400:e2:2", "5450:e3:1", "5500:e0:4"] + P(6000, "SAVE")
    jobs["replace-home"] = lambda: synth_run("m101p2-replace-home", 9800, base + P(7800, "SAVE") + P(8100, "SAVE") + P(8400, "HOME"), flash="songs")
    jobs["replace-ask"] = lambda: synth_run("m101p2-replace-ask", 9000, base + P(7800, "SAVE") + P(8100, "SAVE"), flash="songs")
    with ThreadPoolExecutor(3) as ex:
        futs = {k: ex.submit(f) for k, f in jobs.items()}
        R = {k: f.result() for k, f in futs.items()}
    S = {k: r["stats"] for k, r in R.items()}
    # ---- the layer buttons
    s1, s2, s3, s4 = S["s-fx"], S["s-fx2"], S["s-fx3"], S["s-fx4"]
    check(s1["part"] == 0 and vols(s1) == [0, 0, 0, 0] and s1["vp_title"] == "Your Part" and s1["vp_value"] == "Drums" and
          S["s-env"]["part"] == 2 and S["s-env"]["vp_title"] == "Harmony Volume" and vols(S["s-env"])[2] == 1,
          f"the first press on a layer that is not your part selects it (FX: part {s1['part']}, volumes {vols(s1)}, pop-up "
          f"{s1['vp_title']!r} {s1['vp_value']!r}); harmony is your part at the start, so ENV once is its volume "
          f"({S['s-env']['vp_title']!r} {S['s-env']['vp_value']!r}) (m101p2-s-fx.png, m101p2-s-env.png)")
    check(vols(s2) == [1, 0, 0, 0] and s2["vp_title"] == "Drums Volume" and s2["vp_value"] == "QUIET" and s2["part"] == 0 and
          vols(s3)[0] == 2 and s3["vp_value"] == "MUTED" and vols(s4)[0] == 0 and s4["vp_value"] == "FULL" and s4["part"] == 0,
          f"the second press cycles its volume: QUIET {vols(s2)}, MUTED {vols(s3)}, FULL {vols(s4)} (pop-ups "
          f"{s2['vp_value']}, {s3['vp_value']}, {s4['vp_value']}; m101p2-s-fx2.png)")
    hs, hp = S["hold-sel"], S["hold-then-press"]
    check(hs["part"] == 1 and vols(hs) == [0, 0, 0, 0] and hs["vp_title"] == "Your Part" and
          hp["part"] == 1 and vols(hp)[1] == 1,
          f"a hold of 1.5 s on a layer that is not your part is ONE select (part {hs['part']}, volumes {vols(hs)}: no volume change at the "
          f"release); a press after it cycles its volume ({vols(hp)})")
    k1, k2 = S["keep"], S["keep2"]
    check(k1["part"] == 2 and vols(k1) == [1, 0, 1, 0] or vols(k1)[0] == 1,
          f"volumes are kept when the part moves: harmony QUIET, drums QUIET, back to harmony: {vols(k1)}, part {k1['part']}")
    check(S["s-scl-fx"]["part"] == 0 and S["s-lfo"]["part"] == 3 and S["s-lfo"]["vp_value"] == "Melody" and vols(S["s-lfo"]) == [0, 0, 0, 0],
          f"the melody's button selects it as your part like the others (part {S['s-lfo']['part']}, no volume change)")
    check(S["bt-lfo"]["part"] == 3 and S["bt-lfo"]["vp_title"] == "Your Part" and S["bt-fx"]["part"] == 0 and
          vols(S["bt-fx2"])[0] == 1 and S["bt-fx2"]["vp_title"] == "Drums Volume",
          f"BACKTRACK: the same rule on its layer buttons: LFO selects the melody (part {S['bt-lfo']['part']}), FX selects the drums, again "
          f"{S['bt-fx2']['vp_value']} (m101p2-bt-fx2.png)")
    src = (ROOT / "firmware/app/ui.c").read_text()
    check("hold your part" not in src and "press volume" not in src and "FX-LFO: pick part|again: volume" in src,
          "\"hold your part\" is gone from the instruction rows; the text is \"FX-LFO: pick part|again: volume\"")
    # ---- HOME
    le = [S[f"h-le{n}"] for n in (1, 2, 3)]
    check(le[0]["ui_view"] == V_LIST and le[0]["folder"] >= 1 and le[1]["ui_view"] == V_LIST and le[1]["folder"] == 0 and
          le[2]["ui_view"] == V_MODE and S["h-list"]["ui_view"] == V_MODE,
          f"SONG LEARN: HOME from the play screen: the song list (folder {le[0]['folder']}), then the folders (view {le[1]['ui_view']}, "
          f"folder {le[1]['folder']}), then the mode screen (view {le[2]['ui_view']}); from the folders at once: {S['h-list']['ui_view']} "
          f"(m101p2-h-le3.png)")
    bt = [S[f"h-bt{n}"] for n in (1, 2, 3)]
    check(bt[0]["ui_view"] == V_PLAY and bt[1]["ui_view"] == V_SETUP and bt[2]["ui_view"] == V_MODE and S["h-setup"]["ui_view"] == V_MODE,
          f"BACKTRACK: HOME stops the band (view {bt[0]['ui_view']}), then SETUP ({bt[1]['ui_view']}), then the mode screen "
          f"({bt[2]['ui_view']}); from SETUP at once: {S['h-setup']['ui_view']}")
    check(S["h-sy-page"]["ui_view"] == V_SYNTH and S["h-sy-page"]["synth_page"] == 0 and S["h-sy-main"]["ui_view"] == V_MODE and
          S["h-sy-main-only"]["ui_view"] == V_MODE and S["h-sy-save"]["ui_view"] == V_SYNTH and S["h-sy-save"]["save_screen"] == 0,
          f"SYNTH: HOME from an edit page: the main screen; again: the mode screen ({S['h-sy-main']['ui_view']}); from the main screen at "
          f"once: {S['h-sy-main-only']['ui_view']}; on the SAVE screen HOME goes back one step (view {S['h-sy-save']['ui_view']})")
    rh, ra = S["replace-home"], S["replace-ask"]
    check(rh["save_screen"] == 0 and rh["preset_saves"] == 1 and R["replace-home"]["ops"] == R["replace-ask"]["ops"] == 10 and
          rh["ui_view"] == V_SYNTH and ra["save_screen"] == 2,
          f"REPLACE ... ? and HOME = no: nothing written (flash operations {R['replace-home']['ops']}, saves {rh['preset_saves']}, still in SYNTH)")
    check(S["hold-le-mid"]["ui_view"] == V_PLAY and S["hold-le-1500"]["ui_view"] == V_LIST and S["hold-le-2300"]["ui_view"] == V_MODE and
          S["hold-bt-2300"]["ui_view"] == V_MODE and S["hold-sy-mid"]["synth_page"] == 1 and S["hold-sy-2300"]["ui_view"] == V_MODE,
          f"a HOME held 2 s goes straight to the mode screen from the play screen ({S['hold-le-2300']['ui_view']}), BACKTRACK "
          f"({S['hold-bt-2300']['ui_view']}) and a SYNTH page ({S['hold-sy-2300']['ui_view']}); the press does NOT act first (still on the play "
          f"screen after 1.2 s of holding: view {S['hold-le-mid']['ui_view']}; a SYNTH page: {S['hold-sy-mid']['synth_page']}); "
          f"released at 1.5 s it is one press (view {S['hold-le-1500']['ui_view']})")
    # ---- K4: Melody Sound
    i_pre = {n: k for k, n in enumerate(m8.PRESETS)}
    j2 = {"k4-first": [f"{O + 300}:e5:1"], "k4-two": [f"{O + 300}:e5:1", f"{O + 600}:e5:1"], "k4-back": [f"{O + 300}:e5:-1"],
          "k4-wrap": [f"{O + 300}:e5:-1", f"{O + 600}:e5:1"], "k4-none": [],
          "k4-reopen": [f"{O + 300}:e5:3"] + P(O + 800, "HOME") + P(O + 1100, "PLAY")}
    with ThreadPoolExecutor(3) as ex:
        futs = {k: ex.submit(lambda k, e: learn_run(f"m101p2-{k}", O + 1600, e), k, e) for k, e in j2.items()}
        # the same song played: the melody's note-ons with SONG and with a preset (PLAY at O + 800; 6 s)
        play = {k: ex.submit(lambda k, e: learn_run(f"m101p2-play-{k}", O + 7500, e + P(O + 800, "PLAY")), k, e)
                for k, e in (("song", []), ("preset", [f"{O + 300}:e5:1"]), ("heavy", [f"{O + 300}:e5:{i_pre['MIAMI PAD'] + 1}"]))}
        K = {k: f.result() for k, f in futs.items()}
        Pl = {k: f.result() for k, f in play.items()}
    KS = {k: v["stats"] for k, v in K.items()}
    PS = {k: v["stats"] for k, v in Pl.items()}
    check(KS["k4-first"]["vp_title"] == "Melody Sound" and KS["k4-first"]["vp_value"] == m8.PRESETS[0] and KS["k4-first"]["mel_sound"] == 1 and
          KS["k4-two"]["vp_value"] == m8.PRESETS[1] and KS["k4-back"]["vp_value"] == KS["k4-back"]["vp_value"] and KS["k4-wrap"]["vp_value"] == "SONG" and KS["k4-wrap"]["mel_sound"] == 0 and
          KS["k4-none"]["mel_sound"] == 0,
          f"K4 on SONG LEARN steps SONG, then the preset list: {KS['k4-first']['vp_value']!r}, {KS['k4-two']['vp_value']!r}; one back from "
          f"SONG wraps to the end ({KS['k4-back']['vp_value']!r}), then forward to {KS['k4-wrap']['vp_value']!r} (m101p2-k4-first.png)")
    check(PS["song"]["notes_on"][3] > 0 and PS["preset"]["notes_on"][3] == PS["song"]["notes_on"][3] and
          PS["preset"]["note_on_last"][3] == PS["song"]["note_on_last"][3] and PS["preset"]["notes_on"][:3] == PS["song"]["notes_on"][:3] and
          PS["preset"]["synth_notes"] >= PS["song"]["notes_on"][3] and PS["song"]["synth_notes"] == 0 and not Pl["preset"]["halt"],
          f"the melody's note-ons are the same with a preset (melody {PS['preset']['notes_on'][3]} vs {PS['song']['notes_on'][3]}, last note "
          f"{PS['preset']['note_on_last'][3]}, drums/bass/harmony {PS['preset']['notes_on'][:3]} vs {PS['song']['notes_on'][:3]}) but they sound on "
          f"vsynth ({PS['preset']['synth_notes']} voices started; 0 with SONG)")
    check(KS["k4-reopen"]["mel_sound"] == 0 and KS["k4-reopen"]["ui_view"] == V_PLAY,
          f"the choice is kept until a song is opened: opening the song again is back to SONG (mel_sound {KS['k4-reopen']['mel_sound']})")
    st["cpu_probe"] = PS["heavy"]
    # ---- the CPU: EAS + vsynth with the melody on MIAMI PAD (the heaviest stock preset of M9 / M10's CPU tests), every public song at the
    # busiest levels (drums 3, bass 4, harmony 5), PLAY at 5.2 s, 20 s: the share of a block (limit 60 %)
    pubs = [c["slug"] for c in all_songs if (ROOT / "songs" / c["slug"]).is_dir()]
    heavy = i_pre["MIAMI PAD"] + 1
    cj = {}
    for slug in pubs:
        i = slugs.index(slug)
        cj[slug] = (lambda i, slug: lambda: m5.run(f"m101p2-cpu-{slug}", 23000,
            m5.open_song(i) + [f"{4800 + 40 * (k + 4 * b)}:e{2 + b}:1" for b, n in enumerate((2, 3, 4)) for k in range(n)]
            + [f"5300:e5:{heavy}"] + P(5600, "PLAY"), ))(i, slug)
    with ThreadPoolExecutor(3) as ex:
        cf = {k: ex.submit(f) for k, f in cj.items()}
        C = {k: f.result() for k, f in cf.items()}
    cpu = {k: (v["stats"]["render_max"] + v["stats"]["synth_render_max"]) / v["stats"]["budget"] for k, v in C.items()}
    worst = max(cpu, key=cpu.get)
    lines.append(f"  - CPU (EAS + vsynth, melody on MIAMI PAD, levels 3 / 4 / 5): worst {worst} {cpu[worst] * 100:.0f} %; " +
                 ", ".join(f"{k} {v * 100:.0f}" for k, v in sorted(cpu.items(), key=lambda kv: -kv[1])[:5]) + " ...")
    check(all(v <= 0.60 for v in cpu.values()) and all(not C[k]["halt"] and C[k]["stats"]["mel_sound"] == heavy and
                                                         C[k]["stats"]["levels"] & 0xFFF == 0x543 for k in C),
          f"the melody on vsynth stays under 60 % of a block in every public song at its busiest levels: worst {worst} {cpu[worst] * 100:.0f} %")
    st["cpu_worst"] = (worst, cpu[worst])
    # ---- BACKTRACK's K4 keeps a reference (closes M10-14): a preset saved in a lower slot does not shift it
    two = m10.obj_image(m5.FLASH["songs"], [(5, 1, "FIVE", 0xFF), (20, 1, "TWENTY", 0xFF)])
    fl = m5.OUT / "flash" / "two-presets.bin"
    fl.write_bytes(Path(two).read_bytes())
    n_stock = len(m8.PRESETS)
    sc = ["4050:e0:1", "4200:PLAY", "4260:PLAY^", f"4500:e5:{n_stock + 1}"] + P(4800, "HOME") + ["5000:e0:2"] + P(5100, "PLAY") + \
        P(5400, "SAVE") + P(5800, "SAVE") + P(6200, "HOME") + ["6400:e0:1"] + P(6500, "PLAY")
    r = m5.run("m101p2-bt-ref", 7200, sc, mode=None, flash=str(fl))
    rs = r["stats"]
    check(rs["bt_sound"] == n_stock + 2 and rs["preset_used"] == 3 and rs["ui_view"] == V_SETUP,
          f"BACKTRACK's K4 is a reference: U20 chosen (list index {n_stock + 1}), a preset saved into the first empty slot from SYNTH, "
          f"back in BACKTRACK it is still U20 (now list index {rs['bt_sound']}, {n_stock + 2} expected; {rs['preset_used']} presets)")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 5
FIXED = ("camptown-races", "michael-row", "oh-susanna", "saints", "scarborough-fair")


def _chords_by_bar(c):
    """{bar index (tick / bar ticks, in halves kept): [chord symbols in the order they start]} from the compiled song"""
    bt = int(c["time_signature"][0]) * c["ppq"]
    out = {}
    for e in c["chord_events"]:
        out.setdefault(e["tick"] // bt, []).append(c["chords"][e["chord"]]["symbol"])
    return out


def test_m101p5(all_songs, rep):
    lines, st, check = m5.checker("M10.1 part 5: the five public song fixes (the planning chat's notes)")
    from concurrent.futures import ThreadPoolExecutor
    by = {c["slug"]: c for c in all_songs}
    camp, row, sus, saints, sf = (by[k] for k in FIXED)
    check(saints["tempo_bpm"] == 135 and sus["tempo_bpm"] == 125 and camp["tempo_bpm"] == 132 and row["tempo_bpm"] == 92,
          f"tempos: saints {saints['tempo_bpm']} (135), oh-susanna {sus['tempo_bpm']} (125); camptown {camp['tempo_bpm']} and michael-row "
          f"{row['tempo_bpm']} unchanged")
    cb = _chords_by_bar(camp)
    check(all(cb[b] == ["A7"] for b in (1, 5, 13, 17)) and all(cb[b] == ["D"] for b in (0, 2, 4, 6)) and cb[3] == ["A7", "D"] and
          cb[7] == ["A7", "D"],
          f"camptown-races: the 'doo-dah' bars (FM-1 bars 2, 6, 14, 18) are A7 ({[cb[b] for b in (1, 5, 13, 17)]}); bars 1, 3, 4 as before")
    rb = _chords_by_bar(row)
    check(rb[3] == ["G"] and rb[11] == ["G"] and rb[19] == ["G"] and rb[7] == ["D", "A7"],
          f"michael-row: verse bar 3 (FM-1 bars 3, 11, 19) is G alone ({rb[3]}), the second line's bar 3 still D A7 ({rb[7]})")
    sb = _chords_by_bar(sf)
    want = ["Dm", "Dm", "C", "Dm", "F", "Dm", "G", "Dm", "Dm", "Dm", "F", "F", "C", "C", "Dm", "C", "C", "Dm"]
    check(sf["bar_count"] == 36 and [sb[b][0] for b in range(18)] == want and [sb[b][0] for b in range(18, 36)] == want and
          all(len(sb[b]) == 1 for b in range(36)),
          f"scarborough-fair: 18 bars a verse, 36 in all ({sf['bar_count']}); one chord per bar, the chart "
          f"{' '.join(sb[b][0] for b in range(18))}")
    check(len(sf["sections"]) == 2 and all(x["bars"] == 18 for x in sf["sections"]), "its form: Verse 1 and Verse 2, 18 bars each")
    cy = ROOT / "songs/private/creep/song.yaml"
    if cy.is_file():                                  # (the user's private import: git-ignored; present on this machine)
        t = cy.read_text()
        check("Dm7 Cm" not in t and t.count("Csus4 Cm7") == 2 and t.count("| C | Csus4 C | Cm | Cm |") == 3,
              "creep (private): bars 8, 16 and 40 are Cm for the whole bar; bars 24 and 48 stay 'Csus4 Cm7'")
    # the song is opened and played from bar 1 to its end at level 1: no halt, notes of every layer
    slugs = [c["slug"] for c in all_songs]
    jobs = {}
    for k in FIXED:
        c = by[k]
        bpb = int(c["time_signature"][0])
        secs = (c["bar_count"] + 1) * bpb * 60.0 / c["tempo_bpm"]
        ms = int(m5.PLAY_MS + secs * 1000 + 3000)
        i = slugs.index(k)
        jobs[k] = (lambda i, k, ms: lambda: m5.run(f"m101p5-{k}", ms, m5.open_song(i) + P(m5.PLAY_MS, "PLAY")))(i, k, ms)
    with ThreadPoolExecutor(3) as ex:
        futs = {k: ex.submit(f) for k, f in jobs.items()}
        R = {k: f.result() for k, f in futs.items()}
    for k in FIXED:
        s = R[k]["stats"]
        check(not R[k]["halt"] and s["ends"] >= 1 and all(n > 0 for n in s["notes_on"]),
              f"{k}: opened and played from bar 1 to its end at level 1 ({s['starts']} start, {s['ends']} end, note-ons {list(s['notes_on'])}, "
              f"no halt)")
    rep += lines
    return st["ok"]


PARTS = {"m101p1": test_m101p1, "m101p2": test_m101p2, "m101p5": test_m101p5}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    ap.add_argument("--pkg")
    ap.add_argument("--out")
    ap.add_argument("--jobs", type=int, default=3)
    a = ap.parse_args()
    m5.OUT = Path(a.out).resolve() if a.out else ROOT / "build/emu/m101"
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
    text = [f"# M10.1 emulator tests\n\nPackage: {m5.PKG}, sha256 {sha}\n"]
    for k in reports:
        text += reports[k]
    text.append(f"\n**{'ALL PASS' if all(results.values()) else 'FAILURES: ' + ', '.join(k for k, v in results.items() if not v)}**\n")
    (m5.OUT / "report.md").write_text("\n".join(text))
    print("\n".join(text))
    sys.exit(0 if all(results.values()) else 1)


if __name__ == "__main__":
    main()
