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
    for name, n in (("title_now", 48), ("legend_now", 64), ("cells_now", 48)):
        s[name] = ram[at:at + n].split(b"\0")[0].decode("latin-1")
        at += n
    return s


m5.stats = stats101


def _margins_clear(img, y0, y1):
    return m10._margins_clear(img, y0, y1)


def obj_image(base_flash, recs, tag):
    """tools/m10_test.py's obj_image (a flash image with a user-preset object in copy A of 0xDE000; recs: [(slot, engine, name, level
    byte)]), but into its own file: m10's always writes old-presets.bin, and parts running at once would overwrite each other's image"""
    import zlib
    flash = bytearray(Path(base_flash).read_bytes())
    slots = [bytearray(64) for _ in range(30)]
    for slot, eng, name, level in recs:
        r = slots[slot - 1]
        r[0], r[1] = 1, eng
        r[2:2 + len(name)] = name.encode()
        for k in range(37):
            r[14 + k] = 64 if k != 35 else level
    payload = struct.pack("<IBBH", 0x53525055, 1, 30, 0) + b"".join(bytes(x) for x in slots)
    hdr = struct.pack("<IHHIII2I", 0x30583058, 2, 0, 1, len(payload), zlib.crc32(payload), 0, 0)
    hdr += struct.pack("<I", zlib.crc32(hdr))
    flash[0xDE000:0xE0000] = b"\xff" * 0x2000
    flash[0xDE000:0xDE000 + len(hdr)] = hdr
    flash[0xDE000 + 256:0xDE000 + 256 + len(payload)] = payload
    out = m5.OUT / "flash" / f"m101-presets-{tag}.bin"
    out.write_bytes(flash)
    return out


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
    flash_long = obj_image(m5.FLASH["songs"], [(30, 1, "WWWWWWWWWWWW", 0xFF)], "p1")
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
    sink = sum(1 for x in range(4, 236) for y in range(216, 236) if simg.getpixel((x, y)) != simg.getpixel((0, 0)))   # M10.3: the version, lower right
    check(S["mode"]["ui_view"] == V_MODE and ink > 150 and _margins_clear(mimg, 0, 20) and sink > 20,
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
    want = {"vp-le-K1": ("Drums Complexity Level", r"2 of \d"), "vp-le-K2": ("Bass Complexity Level", r"2 of \d"), "vp-le-K3": ("Harmony Complexity Level", r"2 of \d"),
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
    check(s1["part"] == 0 and vols(s1) == [0, 0, 0, 0] and s1["vp_title"] == "Track Selected" and s1["vp_value"] == "Drums" and
          S["s-env"]["part"] == 2 and S["s-env"]["vp_title"] == "Harmony Volume" and vols(S["s-env"])[2] == 1,
          f"the first press on a layer that is not your part selects it (FX: part {s1['part']}, volumes {vols(s1)}, pop-up "
          f"{s1['vp_title']!r} {s1['vp_value']!r}); harmony is your part at the start, so ENV once is its volume "
          f"({S['s-env']['vp_title']!r} {S['s-env']['vp_value']!r}) (m101p2-s-fx.png, m101p2-s-env.png)")
    check(vols(s2) == [1, 0, 0, 0] and s2["vp_title"] == "Drums Volume" and s2["vp_value"] == "QUIET" and s2["part"] == 0 and
          vols(s3)[0] == 2 and s3["vp_value"] == "MUTED" and vols(s4)[0] == 0 and s4["vp_value"] == "FULL" and s4["part"] == 0,
          f"the second press cycles its volume: QUIET {vols(s2)}, MUTED {vols(s3)}, FULL {vols(s4)} (pop-ups "
          f"{s2['vp_value']}, {s3['vp_value']}, {s4['vp_value']}; m101p2-s-fx2.png)")
    hs, hp = S["hold-sel"], S["hold-then-press"]
    check(hs["part"] == 1 and vols(hs) == [0, 0, 0, 0] and hs["vp_title"] == "Track Selected" and
          hp["part"] == 1 and vols(hp)[1] == 1,
          f"a hold of 1.5 s on a layer that is not your part is ONE select (part {hs['part']}, volumes {vols(hs)}: no volume change at the "
          f"release); a press after it cycles its volume ({vols(hp)})")
    k1, k2 = S["keep"], S["keep2"]
    check(k1["part"] == 2 and vols(k1) == [1, 0, 1, 0] or vols(k1)[0] == 1,
          f"volumes are kept when the part moves: harmony QUIET, drums QUIET, back to harmony: {vols(k1)}, part {k1['part']}")
    check(S["s-scl-fx"]["part"] == 0 and S["s-lfo"]["part"] == 3 and S["s-lfo"]["vp_value"] == "Melody" and vols(S["s-lfo"]) == [0, 0, 0, 0],
          f"the melody's button selects it as your part like the others (part {S['s-lfo']['part']}, no volume change)")
    check(S["bt-lfo"]["part"] == 3 and S["bt-lfo"]["vp_title"] == "Track Selected" and S["bt-fx"]["part"] == 0 and
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
    two = obj_image(m5.FLASH["songs"], [(5, 1, "FIVE", 0xFF), (20, 1, "TWENTY", 0xFF)], "p2")
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


def _chord_per_bar(c):
    """the chord sounding at the start of each bar (an event only marks a change)"""
    bt = int(c["time_signature"][0]) * c["ppq"]
    ev = sorted((e["tick"], c["chords"][e["chord"]]["symbol"]) for e in c["chord_events"])
    out = []
    for b in range(c["bar_count"]):
        cur = [sym for t, sym in ev if t <= b * bt]
        out.append(cur[-1] if cur else None)
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
    check(all(cb[b] == ["A7"] for b in (1, 5, 13, 17)) and all(cb[b] == ["D"] for b in (0, 2, 6)) and cb[3] == ["A7", "D"] and
          cb[7] == ["A7", "D"],
          f"camptown-races: the 'doo-dah' bars (FM-1 bars 2, 6, 14, 18) are A7 ({[cb[b] for b in (1, 5, 13, 17)]}); bars 1, 3, 4 as before")
    rb = _chords_by_bar(row)
    check(rb[3] == ["G"] and rb[11] == ["G"] and rb[19] == ["G"] and rb[7] == ["D", "A7"],
          f"michael-row: verse bar 3 (FM-1 bars 3, 11, 19) is G alone ({rb[3]}), the second line's bar 3 still D A7 ({rb[7]})")
    sb = _chord_per_bar(sf)
    want = ["Dm", "Dm", "C", "Dm", "F", "Dm", "G", "Dm", "Dm", "Dm", "F", "F", "C", "C", "Dm", "C", "C", "Dm"]
    check(sf["bar_count"] == 36 and sb[:18] == want and sb[18:] == want,
          f"scarborough-fair: 18 bars a verse, 36 in all ({sf['bar_count']}); the chart's chord in each bar: {' '.join(sb[:18])}")
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


# --------------------------------------------------------------------------------------------- part 3
BT_PRE = m8.BT


def prep_images(tag="p3"):
    """a flash with two songs, two saved presets (U05, U20) and a changed setting (ZOOM) -> its path (tag: the caller's own files,
    parts running at once must not share them)"""
    send = m5.run(f"m101{tag}-prep-send", 120000, [], flash="empty_quiet",
                  host=f"{m5.SEND} send --emu-pipe {ROOT}/songs/saints {ROOT}/songs/camptown-races")
    settings = P(4600, "GLO") + P(4800, "GLO") + ["5000:e0:4", "5300:e1:1"] + P(5800, "GLO")
    st = m5.run(f"m101{tag}-prep-settings", 6600, settings, flash=send["flash"])
    return obj_image(st["flash"], [(5, 1, "FIVE", 0xFF), (20, 1, "TWENTY", 0xFF)], f"before-{tag}")


def test_m101p3(all_songs, rep):
    lines, st, check = m5.checker("M10.1 part 3: FACTORY RESET (writes: the user runs it; here the emulator only)")
    from concurrent.futures import ThreadPoolExecutor
    before = prep_images("p3")
    boot0 = m5.run("m101p3-boot-before", 5200, [], flash=str(before))
    B0 = boot0["stats"]
    B0["preset_used"] = synth_run("m101p3-boot-before-sy", 5200, [], flash=str(before))["stats"]["preset_used"]   # (counted in SYNTH)
    check(B0["list_n"] == 2 and B0["preset_used"] == 2 and (B0["settings"] >> 16) & 15 == 1,
          f"the image before: {B0['list_n']} songs, {B0['preset_used']} saved presets, ZOOM changed (settings {B0['settings']:#x})")
    default = m5.run("m101p3-boot-default", 5200, [], flash="empty")["stats"]
    host = f"{m5.SEND} factory-reset --emu-pipe --yes"
    rs = m5.run("m101p3-reset", 40000, [], flash=str(before), host=host)
    flash = Path(rs["flash"]).read_bytes()
    store_blank = all(b == 0xFF for b in flash[m5.AREA[0]:m5.AREA[1]])
    pre_blank = all(b == 0xFF for b in flash[0xDE000:0xE0000])
    check("factory reset done" in rs["out"] and rs["stats"]["ui_view"] == 10 and not rs["halt"],
          f"fm1song factory-reset --yes answers 'factory reset done: switch the FM-1 off and on' and the unit shows the DONE screen "
          f"(view {rs['stats']['ui_view']}; m101p3-reset.png)")
    check(store_blank and pre_blank and flash[0xDC000:0xDE000] == Path(before).read_bytes()[0xDC000:0xDE000],
          f"the song store (0x97000..0xDBFFF) and the preset object (0xDE000..0xDFFFF, both sectors) are blank; the panel calibration "
          f"(0xDC000) and the firmware are untouched (flash operations {rs['ops']})")
    boot = m5.run("m101p3-boot-after", 5200, [], flash=str(rs["flash"]))
    B = boot["stats"]
    B["preset_used"] = synth_run("m101p3-boot-after-sy", 5200, [], flash=str(rs["flash"]))["stats"]["preset_used"]
    check(B["list_n"] == 0 and B["preset_used"] == 0 and (B["settings"]) == default["settings"] and B["ui_view"] == V_LIST and not boot["halt"],
          f"after a power cycle: no songs ({B['list_n']}), no presets ({B['preset_used']}), the settings are the defaults "
          f"({B['settings']:#x} = {default['settings']:#x}), a normal boot")
    # refused in SYNTH and BACKTRACK (as sends are: result 9); nothing written
    before_bytes = Path(before).read_bytes()
    hostd = f"sleep 10; {host}"          # (wall-clock seconds: long enough for the mode to be entered even when other runs load the machine)
    sy = m5.run("m101p3-refused-synth", 30000, SYNTH_PRE, mode=None, flash=str(before), host=hostd)
    bt = m5.run("m101p3-refused-bt", 30000, BT_PRE, mode=None, flash=str(before), host=hostd)
    same = [Path(x["flash"]).read_bytes() == before_bytes for x in (sy, bt)]
    check(all(same) and all("NOT reset" in x["out"] and "SYNTH or BACKTRACK" in x["out"] for x in (sy, bt)) and sy["ops"] == bt["ops"] == 0,
          f"refused in SYNTH and in BACKTRACK (result 9), nothing written ({sy['ops']} / {bt['ops']} flash operations, the flash image "
          f"byte for byte the same: {same})")
    # without --yes: nothing is sent at all
    no = m5.run("m101p3-no-yes", 7000, [], flash=str(before), host=f"{m5.SEND} factory-reset --emu-pipe")
    check("ERASES EVERY SONG" in no["out"] and no["ops"] == 0 and Path(no["flash"]).read_bytes() == before_bytes,
          "fm1song factory-reset without --yes only explains what it would erase (no frame sent)")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 4
M10_COMMIT = "8d2f517"      # the tree M10.1 started from (the M10 package's source plus the review commits)
ORDER = ["E-PIANO", "E-GUITAR", "E-SITAR", "PARKER", "POTATO", "EDS", "STAB 84", "NEON PAD", "DUSK STRINGS", "OCT BOUNCE", "SUNSET LEAD",
         "SYNC SCREAM", "CHROME BRASS", "GLASS PLUCK", "VAPOR KEYS", "RISER", "CHIP BLIP", "DRIVE BASS", "ZUNJET", "LASER ARP",
         "NIGHT DRIVE", "SAW STRINGS", "HORIZON PWM", "TAPE CHOIR", "GRID PULSE", "OUTRUN BASS", "MIAMI PAD", "CHROME ARP", "NEON BELLS",
         "VHS STRINGS"]
ZUNJET_ROW = """PR("ZUNJET", AN, 4, 70, 105, 40, 92, 26, 78, 0, 5, 80, 72, 52, 24, 64, 36, 0, ARP_UP, RATE_8, 0, 64,
      64, 40, 0, 0, 0, 0, 0, 0, 3, 64, 16, 0, 64, 64, 30, 97, 0),"""


def preset_rows(text):
    import re
    rows = {}
    for m in re.finditer(r'PR\("([^"]+)",(.*?)\),\n', text, flags=re.S):
        rows[m.group(1)] = " ".join(m.group(2).split())
    return rows


def test_m101p4(all_songs, rep):
    lines, st, check = m5.checker("M10.1 part 4: preset order, E-GUITAR, EDS, ZUNJET in HAZE PAD's place")
    from concurrent.futures import ThreadPoolExecutor
    new = (ROOT / "firmware/app/vsynth.c").read_text()
    try:
        old = subprocess.run(["git", "show", f"{M10_COMMIT}:firmware/app/vsynth.c"], cwd=ROOT, capture_output=True, text=True,
                             check=True).stdout
    except subprocess.CalledProcessError:
        old = None
    rn = preset_rows(new)
    check(list(rn) == ORDER and m8.PRESETS == ORDER,
          f"the stock list in vsynth.c is E-PIANO, E-GUITAR, E-SITAR, PARKER, POTATO, EDS, STAB 84, then NEON PAD ... VHS STRINGS in today's order "
          f"(30 presets, ZUNJET where HAZE PAD was: after DRIVE BASS, before LASER ARP)")
    if old:
        ro = preset_rows(old)
        same = [n for n in rn if n in ro and rn[n] == ro[n]]
        changed = sorted(n for n in rn if n not in same)
        check(changed == ["E-GUITAR", "EDS", "NIGHT DRIVE", "POTATO", "ZUNJET"],   # (M10.2: EDS and NIGHT DRIVE swapped their sounds; M10.3: POTATO's CRUSH 127)
              f"only E-GUITAR, EDS, NIGHT DRIVE, POTATO and ZUNJET differ from M10's table (the other 25 rows byte for byte): changed {changed}")
        eg_old = [x.strip() for x in ro["E-GUITAR"].split(",")]
        eg_new = [x.strip() for x in rn["E-GUITAR"].split(",")]
        diff = [(a, b) for a, b in zip(eg_old, eg_new) if a != b]
        check(len(eg_old) == len(eg_new) and diff == [("0", "12"), ("90", "35")],
              f"E-GUITAR: attack 0 -> 12 and fuzz 90 -> 35, every other value (LEVEL too) unchanged: {diff}")
    exp = " ".join(ZUNJET_ROW.split())
    exp = exp[exp.index('",') + 2:].rstrip(",").rstrip(")").strip()
    check(rn.get("ZUNJET", "").strip() == exp, "ZUNJET's row is the one typed in the plan, value for value (LEVEL 97 as the user has it)")
    # on the screen: the list and the pages
    jobs = {"start": lambda: synth_run("m101p4-start", 5200, []), "back": lambda: synth_run("m101p4-back", 5200, ["5000:e6:-1"])}
    for k in (1, 3, 4, 5, 6, 7, 17, 18, 19, 25, 29):
        jobs[f"at{k}"] = (lambda k: lambda: synth_run(f"m101p4-at{k}", 5300, [f"5000:e6:{k}"]))(k)
    edit = lambda n: [x for i in range(n) for x in P(5000 + 300 * i, "EDIT")]
    jobs["eg-env"] = lambda: synth_run("m101p4-eg-env", 5700, ["4700:e6:1"] + edit(1))
    jobs["eg-fx3"] = lambda: synth_run("m101p4-eg-fx3", 5700, ["4700:e6:1"] + P(5000, "FX") + P(5300, "FX") + P(5600, "FX"))
    for i, nm in enumerate(("env", "filter", "sound-a", "sound-b", "arp", "fx1", "fx2", "fx3", "fx4")):
        jobs[f"zj-{nm}"] = (lambda i, nm: lambda: synth_run(f"m101p4-zj-{nm}", 5500 + 300 * i, ["4700:e6:18"] + edit(i + 1)))(i, nm)
    jobs["bt"] = lambda: btrun("m101p4-bt", 5200, [])
    jobs["eds"] = lambda: synth_run("m101p4-eds", 7000, ["4700:e6:5", "5000:C4", "5000:E4", "5000:G4", "6500:C4^", "6500:E4^", "6500:G4^"])
    jobs["eds-low"] = lambda: synth_run("m101p4-eds-low", 7000, ["4700:e6:5", "4800:OCTDN!", "4900:OCTDN!", "5000:C4", "6500:C4^"])
    with ThreadPoolExecutor(3) as ex:
        futs = {k: ex.submit(f) for k, f in jobs.items()}
        R = {k: f.result() for k, f in futs.items()}
    S = {k: r["stats"] for k, r in R.items()}
    got = {k: S[f"at{k}"]["preset_name"] for k in (1, 3, 4, 5, 6, 7, 17, 18, 19, 25, 29)}
    check(S["start"]["preset_name"] == "E-PIANO" and S["start"]["synth_preset"] == 0 and S["back"]["preset_name"] == "VHS STRINGS" and
          all(got[k] == ORDER[k] for k in got),
          f"on the screen (PRESET knob): the list in that order {got}; SYNTH starts on E-PIANO (position 0 as in M10), one back wraps to "
          f"{S['back']['preset_name']!r} (m101p4-at5.png)")
    check(S["bt"]["bt_sound"] == 0 and S["bt"]["ui_view"] == V_SETUP,
          f"BACKTRACK's default melody sound is E-PIANO as in M10 (list position {S['bt']['bt_sound']}); the kits name no preset")
    check(S["eg-env"]["cells_now"] == "12 70 50 30" and S["eg-fx3"]["cells_now"].split()[-1] == "35" and S["eg-fx3"]["title_now"] == "FX 3 - E-GUITAR",
          f"E-GUITAR on its pages: ENVELOPE {S['eg-env']['cells_now']!r} (attack 12), FX 3 {S['eg-fx3']['cells_now']!r} (fuzz 35; m101p4-eg-env.png)")
    want = {"env": "4 70 105 40", "filter": "92 26 +14 LP", "sound-a": "SYNC 80 72 52", "sound-b": "24 64 36 0", "arp": "UP 1/8 1 64",
            "fx1": "64 40 0 0", "fx2": "0 0 0 0", "fx3": "1/8D 64 16 0", "fx4": "64 64 30 97"}
    bad = {k: S[f"zj-{k}"]["cells_now"] for k, v in want.items() if S[f"zj-{k}"]["cells_now"] != v}
    check(not bad, f"ZUNJET on its nine pages shows the user's values (FILTER ENV AMOUNT +14, TYPE LP, WAVE SYNC, ARP UP 1/8 octaves 1, DELAY "
                   f"TIME 1/8D, LEVEL 97): {[S['zj-' + k]['cells_now'] for k in want]} {bad or ''} (m101p4-zj-filter.png)")
    e = S["eds"]
    rms = m5.rms(R["eds"]["left"][int(5.2 * m5.RATE):int(6.4 * m5.RATE)])
    check(e["synth_preset"] == 5 and e["synth_engine"] == 0 and e["synth_notes"] == 3 and rms > 300 and e["synth_peak"] < 8388607 and
          e["arp_pattern"] == 0,
          f"EDS renders: ANALOG, ARP off, 3 voices, rms {rms:.0f}, peak {e['synth_peak']} (below 8,388,607: no clipping)")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 6
def library_paths():
    return [l.strip() for l in (ROOT / "songs/library.txt").read_text().splitlines() if l.strip() and not l.lstrip().startswith("#")]


def test_m101p6(all_songs, rep):
    lines, st, check = m5.checker("M10.1 part 6: songs/library.txt and tools/fresh_install.sh (the whole script on the emulator; never the unit)")
    lib = library_paths()
    songs = [p for p in lib if not p.startswith("songs/backtracks/")]
    bts = [p for p in lib if p.startswith("songs/backtracks/")]
    folders = sorted(f"songs/{p.name}" for p in m5.song_dirs())
    check(songs == folders and len(bts) == 42 and all((ROOT / p / "song.yaml").is_file() for p in lib) and not any("private" in p for p in lib) and
          bts == sorted(f"songs/backtracks/{p.name}" for p in (ROOT / "songs/backtracks").iterdir() if (p / "song.yaml").is_file()),
          f"songs/library.txt: {len(songs)} public songs (every folder of songs/ but private/) and {len(bts)} backtracks, songs first, no private path")
    check((ROOT / "songs/library.txt").read_text().startswith("# the songs and backtracks a new FM-1 gets; the website will use this list too"),
          "its header comment says what it is")
    before = prep_images("p6")
    host = f"env FM1_FRESH_EMU=1 {ROOT}/tools/fresh_install.sh"
    r = m5.run("m101p6-fresh", 600000, [], flash=str(before), host=host)
    out = r["out"]
    listed = re.findall(r"^  (\S+)\s+\d+ bytes\s+\d+ blocks\s+(\S+)$", out, flags=re.M)
    want = {Path(p).name for p in lib}
    slugs = {m5.read_compiled(m5.compile_folder(ROOT / p)[0])["slug"] for p in lib}
    check("factory reset done" in out and f"DONE: {len(lib)} of {len(lib)} entries" in out and not r["halt"],
          f"the whole script on the emulator (no install step, no power cycle): the reset ran, {len(lib)} entries sent, the check says "
          f"{re.findall(r'DONE: [^\n]*', out)[:1]}")
    check(len(listed) == len(lib) and all(s == "ok" for _, s in listed) and {n for n, _ in listed} == slugs and "private" not in out and
          "creep" not in {n for n, _ in listed},
          f"fm1song list afterwards: {len(listed)} entries, every one 'ok', exactly the library's ({len(slugs - {n for n, _ in listed})} missing), "
          f"no private song")
    boot = m5.run("m101p6-boot", 5200, [], flash=str(r["flash"]))
    sy = synth_run("m101p6-boot-sy", 5200, [], flash=str(r["flash"]))
    bt = m5.run("m101p6-boot-bt", 5200, m8.BT, mode=None, flash=str(r["flash"]))
    check(boot["stats"]["list_n"] == len(songs) and bt["stats"]["bt_n"] == len(bts) and sy["stats"]["preset_used"] == 0 and not boot["halt"],
          f"after a power cycle: {boot['stats']['list_n']} songs in SONG LEARN, {bt['stats']['bt_n']} backtracks in BACKTRACK, "
          f"{sy['stats']['preset_used']} saved presets (the old ones were erased)")
    # the script refuses to go on when a step fails, and says how to go back
    sh = (ROOT / "tools/fresh_install.sh").read_text()
    check("Fallback: install the last good firmware again" in sh and "ask()" in sh and sh.count('ask "') >= 3 and "factory-reset --yes" in sh,
          "the script asks y/n before each write, stops at the first failure and prints the fallback line")
    rep += lines
    return st["ok"]


PARTS = {"m101p1": test_m101p1, "m101p2": test_m101p2, "m101p3": test_m101p3, "m101p4": test_m101p4, "m101p5": test_m101p5, "m101p6": test_m101p6}


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
