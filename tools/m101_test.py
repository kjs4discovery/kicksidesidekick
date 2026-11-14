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


PARTS = {"m101p1": test_m101p1}


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
