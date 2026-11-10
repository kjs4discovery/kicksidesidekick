#!/usr/bin/env python3
# m6_test.py -- Milestone M6: headless emulator tests of the new features (built on tools/m5_test.py's runner, its
# prepared flash images and presses). Never touches the FM-1. Licence: TBD (see D017)
#
#   .venv/bin/python -I tools/m6_test.py [--only m6p1,m6p2,..] [--pkg PKG] [--out DIR]   -> build/emu/m6/report.md
#
#   m6p1  learning-mode fixes: HOME held 2 s / Settings > Switch mode -> the mode screen (from LEARN and SYNTH); every
#         level of every layer equally loud, imported or generated (the private songs too); the layer boxes'
#         contrast; the melody roll's piano keyboard
#   m6p2  the views: ukulele chord boxes, melody guitar / ukulele tab, harmony bar view, zoom, the chord boxes'
#         rhythm icon
#   m6p3  SYNTH round 2: reverb, bit-crush, arp rates and RANDOM, the SHIFT layer, the drum jam
# The full suite (Part 4) is this plus tools/m5_test.py (all of M2..M5's checks) and tools/m51_test.py.
import argparse
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import m5_test as m5                     # noqa: E402

V_LIST, V_PLAY, V_SETTINGS, V_MODE, V_SYNTH, V_WIPE = 0, 1, 3, 6, 7, 8
M6_FIELDS = ("mode_switches zoom uke roll_keys jam jam_notes shift fx_reverb fx_crush arp_pattern "
             "arp_rate").split()
PRIV = sorted(p for p in (ROOT / "songs/private").glob("*/") if (p / "song.yaml").is_file()) \
    if (ROOT / "songs/private").is_dir() else []
O = m5.OPEN_MS
SYNTH = ["4050:e0:1", "4200:PLAY", "4260:PLAY^"]   # the mode screen: SELECT to SYNTH, PLAY

_stats5 = m5.stats


def stats6(ram):
    """m5_test's stats, plus M6's 16 words after M5's (player.h)"""
    s = _stats5(ram)
    if s is None:
        return s
    i = ram.find(b"PLST")
    while i >= 0 and struct.unpack_from("<I", ram, i + 4)[0] not in (3, 4):
        i = ram.find(b"PLST", i + 1)
    v = struct.unpack_from("<16I", ram, i + m5.STATS.size + m5.STATS4.size)
    s.update(zip(M6_FIELDS, v))
    return s


m5.stats = stats6


def contrast(a, b):
    def lum(c):
        def f(x):
            x /= 255
            return x / 12.92 if x <= 0.03928 else ((x + 0.055) / 1.055) ** 2.4
        return 0.2126 * f(c[0]) + 0.7152 * f(c[1]) + 0.0722 * f(c[2])
    la, lb = lum(a), lum(b)
    return (max(la, lb) + 0.05) / (min(la, lb) + 0.05)


def rgb565(c):
    """the colour as the LCD shows it (RGB565, the low bits dropped)"""
    return (c[0] >> 3 << 3, c[1] >> 2 << 2, c[2] >> 3 << 3)


def ui_rgb(name):
    m = re.search(rf"#define {name} RGB\((\d+), (\d+), (\d+)\)", (ROOT / "firmware/app/ui.c").read_text())
    return tuple(int(x) for x in m.groups())


def hold_home(t, ms=2300):
    return [f"{t}:HOME!", f"{t + ms}:HOME^!"]


def priv_flash(all_songs):
    """a flash with every song, ours and the private ones (when on this machine) -> its name in m5.FLASH"""
    if "all" in m5.FLASH:
        return "all"
    folders = " ".join(str(f) for f in sorted((ROOT / "songs").iterdir()) if (f / "song.yaml").is_file())
    r = m5.run("m6-send-all", 300000, [], flash="empty_quiet",
               host=f"{m5.SEND} send --emu-pipe {folders} " + " ".join(str(f) for f in PRIV))
    m5.FLASH["all"] = r["flash"]
    return "all"


def all_compiled():
    cs = [m5.read_compiled(m5.compile_folder(f)[0]) for f in sorted((ROOT / "songs").iterdir())
          if (f / "song.yaml").is_file()] + [m5.read_compiled(m5.compile_folder(f)[0]) for f in PRIV]
    cs.sort(key=lambda c: c["title"].lower())
    return cs


# --------------------------------------------------------------------------------------------- part 1
def test_m6p1(all_songs, rep):
    lines, st, check = m5.checker("M6 part 1: learning-mode fixes")
    g = [c["slug"] for c in all_songs].index("amazing-grace")
    # HOME held 2 s: the mode screen, from the list, from a song playing, from SYNTH; and back again
    a = m5.run("m6p1-home-list", 7500, hold_home(4600))
    check(a["stats"]["ui_view"] == V_MODE and a["stats"]["mode"] == 0 and a["stats"]["mode_switches"] == 1,
          "the song list, HOME held 2 s: the mode screen (m6p1-home-list.png)")
    s = m5.run("m6p1-home-short", 5600, m5.press_raw(4600, "HOME"))
    check(s["stats"]["ui_view"] == V_LIST and s["stats"]["mode_switches"] == 0,
          "HOME pressed briefly: the list's folders as before, not the mode screen")
    p = m5.run("m6p1-home-play", m5.PLAY_MS + 5200, m5.open_song(g) + m5.press_raw(m5.PLAY_MS, "PLAY")
               + hold_home(m5.PLAY_MS + 2000))
    tail = m5.rms(p["left"][int((m5.PLAY_MS + 4800) / 1000 * m5.RATE):])
    check(p["stats"]["ui_view"] == V_MODE and tail < 30,
          f"a song playing, HOME held 2 s: the song stops (rms after {tail:.0f}), the mode screen")
    back = m5.run("m6p1-home-learn-again", m5.PLAY_MS + 9000, m5.open_song(g) + m5.press_raw(m5.PLAY_MS, "PLAY")
                  + hold_home(m5.PLAY_MS + 1000) + ["9600:e0:-1", "9800:PLAY!", "9860:PLAY^!",
                                                    "10200:PLAY!", "10260:PLAY^!", "10700:PLAY!", "10760:PLAY^!"])
    snd = m5.rms(back["left"][int(12.0 * m5.RATE):])
    check(back["stats"]["mode"] == 1 and back["stats"]["ui_view"] == V_PLAY and snd > 50,
          f"... then LEARN again, the song opened and played: sound (rms {snd:.0f})")
    sy = m5.run("m6p1-home-synth", 9500, SYNTH + ["5000:C4", "5600:C4^"] + hold_home(6000) + ["8500:e0:-1",
                                                                                        "8700:PLAY!", "8760:PLAY^!"],
                mode=None)
    check(sy["stats"]["mode"] == 1 and sy["stats"]["ui_view"] == V_LIST and sy["stats"]["mode_switches"] == 1,
          "SYNTH, HOME held 2 s: the mode screen; LEARN: the song list (the synth gave the song buffer back)")
    sp = m5.run("m6p1-synth-learn-play", 13500, SYNTH + ["5000:C4", "5600:C4^"] + hold_home(6000)
                + ["8500:e0:-1", "8700:PLAY!", "8760:PLAY^!", "9200:PLAY!", "9260:PLAY^!", "9800:PLAY!",
                   "9860:PLAY^!"], mode=None)
    snd = m5.rms(sp["left"][int(12.2 * m5.RATE):])
    check(sp["stats"]["ui_view"] == V_PLAY and snd > 50 and not sp["halt"],
          f"... a song opened after SYNTH plays (rms {snd:.0f}), no crash")
    ss = m5.run("m6p1-synth-again", 10000, SYNTH + hold_home(5000) + ["7600:e0:1", "7800:PLAY!", "7860:PLAY^!",
                                                                     "8500:C4", "9500:C4^"], mode=None)
    snd = m5.rms(ss["left"][int(8.7 * m5.RATE):int(9.4 * m5.RATE)])
    check(ss["stats"]["mode"] == 2 and snd > 300, f"SYNTH, the mode screen, SYNTH again: it plays (rms {snd:.0f})")
    sc = m5.press(4600, "GLO") + m5.press(4800, "GLO") + ["5000:e0:10", "5300:e1:1"]
    se = m5.run("m6p1-settings-switch", 6500, sc)
    check(se["stats"]["ui_view"] == V_MODE and se["stats"]["mode_switches"] == 1,
          "Settings > Switch mode: the mode screen")
    w = m5.run("m6p1-wipe-combo", 6000, ["0:GLO", "0:HOME", "4500:GLO^", "4500:HOME^"], mode=None)
    check(w["stats"]["ui_view"] == V_WIPE and w["stats"]["mode_switches"] == 0,
          "GLO + HOME held at power-on (4.5 s): the WIPE question stays (the held HOME never counts)")
    # the layer boxes: every text on its box at least 4.5:1 (the AMBER theme; the LCD's RGB565)
    LINE, GRAY, AMB, HI, W, BLK = (34, 34, 40), (150, 150, 158), (232, 136, 30), (255, 184, 70), (255, 255, 255), (0, 0, 0)
    QBG, QFG, NONE = ui_rgb("LB_QUIET_BG"), ui_rgb("LB_QUIET_FG"), ui_rgb("LB_NONE_FG")
    pairs = {"FULL: name": (W, LINE), "FULL: level": (HI, LINE), "FULL: FULL": (GRAY, LINE),
             "QUIET: name": (W, QBG), "QUIET: level": (HI, QBG), "QUIET: QUIET": (QFG, QBG),
             "MUTED: name, MUTE": (AMB, BLK), "your part's name": (HI, LINE), "your part's name, QUIET": (HI, QBG),
             "no such layer": (NONE, BLK)}
    crs = {k: contrast(rgb565(a), rgb565(b)) for k, (a, b) in pairs.items()}
    check(min(crs.values()) >= 4.5, "the layer boxes, every state: each text at least 4.5:1 on its box ("
          + ", ".join(f"{k} {v:.1f}" for k, v in crs.items()) + "; M5's QUIET: 1.5)")
    m5.run("m6p1-quiet", m5.PLAY_MS + 2500, m5.open_song(g) + m5.tap(O + 200, "SCL", 1) + m5.tap(O + 600, "FX", 2)
           + m5.press_raw(m5.PLAY_MS, "PLAY"))
    # the melody roll: the piano keyboard on the left
    r = m5.run("m6p1-roll", m5.PLAY_MS + 4000, m5.open_song(g) + m5.hold(O + 200, "LFO")
               + [x for k in range(3) for x in m5.press_raw(O + 1400 + 250 * k, "EDIT")] + m5.press_raw(m5.PLAY_MS, "PLAY"))
    img = r["lcd"].resize((240, 240))
    strip = [img.getpixel((x, y)) for x in range(0, 20) for y in range(60, 150)]
    whites = sum(1 for p in strip if min(p) > 150)
    darks = sum(1 for p in strip if max(p) < 40)
    check(r["stats"]["view"] == 5 and r["stats"]["roll_keys"] >= 13 and whites > 300 and darks > 50,
          f"the melody roll: a vertical piano keyboard on the left, {r['stats']['roll_keys']} keys (one per pitch row; "
          f"m6p1-roll.png)")
    # loudness: every level of every layer, ours and the private songs (imported parts included)
    if os.environ.get("M6_QUICK"):       # (a fast check: skipped)
        rep += lines
        return st["ok"]
    fl = priv_flash(all_songs)
    cs = all_compiled()
    res = m5.level_loudness(cs, flash=fl, prefix="m6p1-lv")
    worst = []
    for slug, v in res.items():
        for layer, xs in v.items():
            ys = [x for x in xs if x > 30]
            ratio = max(ys) / min(ys) if len(ys) > 1 else 1
            worst.append((ratio, slug, layer))
            lines.append(f"  - {slug} {layer}: rms by level {[round(x) for x in xs]}, loudest / softest {ratio:.2f}")
    worst.sort(reverse=True)
    check(worst[0][0] <= 1.6, f"every layer's levels within 1.6x of each other in all {len(res)} songs, the private "
                              f"ones too (worst: {worst[0][1]} {worst[0][2]} {worst[0][0]:.2f})")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 2
def edits(t, n):
    return [x for k in range(n) for x in m5.press_raw(t + 250 * k, "EDIT")]


def lit(img, box):
    """pixels not near black in box (x0, y0, x1, y1) of the 240 x 240 screen"""
    im = img.resize((240, 240))
    return sum(1 for x in range(box[0], box[2]) for y in range(box[1], box[3]) if max(im.getpixel((x, y))) > 60)


def test_m6p2(all_songs, rep):
    lines, st, check = m5.checker("M6 part 2: the views")
    slugs = [c["slug"] for c in all_songs]
    g = slugs.index("amazing-grace")
    P = m5.PLAY_MS
    r = subprocess.run([sys.executable, "-I", str(ROOT / "tools/guitar_check.py"), "--uke"], capture_output=True,
                       text=True)
    check(r.returncode == 0, "the ukulele table (firmware/app/ukulele.h, our own, G4 C4 E4 A4): " +
          r.stdout.strip().replace("\n", "; "))
    # the setting (Settings > Chord boxes, tab: UKULELE), saved: a flash image with it on
    uke, t = m5.settings_script(4300, [(11, 1)])
    ukp = m5.run("m6p2-ukulele-set", t + 1000, uke)
    m5.FLASH["uke"] = ukp["flash"]
    ukq = m5.run("m6p2-ukulele-power", 4600, [], flash="uke")
    check(ukp["stats"]["uke"] == 1 and ukq["stats"]["uke"] == 1, "Settings > Chord boxes, tab: UKULELE; kept over a "
          "power-off")
    # the harmony's chord boxes: guitar (default), ukulele (the setting), the rhythm icon blinking
    base = m5.open_song(g) + m5.press_raw(O + 100, "ARP")
    gt = m5.run("m6p2-guitar", P + 3300, base + edits(O + 400, 3) + m5.press_raw(P, "PLAY"))
    uk = m5.run("m6p2-ukulele", P + 3300, base + edits(O + 400, 3) + m5.press_raw(P, "PLAY"), flash="uke")
    check(gt["stats"]["view"] == 3 and gt["stats"]["uke"] == 0 and uk["stats"]["view"] == 3 and
          uk["stats"]["uke"] == 1 and m5.ImageChops.difference(gt["lcd"], uk["lcd"]).getbbox() is not None,
          "the setting Chord boxes, tab: GUITAR (default; m6p2-guitar.png) or UKULELE: the harmony's boxes with 4 "
          "strings, G C E A (m6p2-ukulele.png)")
    # the rhythm icon: under the o / x lines, bright at an attack, grey between
    shots = [m5.run(f"m6p2-cue-{k}", P + 2600 + 90 * k, base + edits(O + 400, 3) + m5.press_raw(P, "PLAY")) for k in range(4)]
    cues = [lit(r["lcd"], (8, 32 + 70, 34, 32 + 102)) for r in shots]
    hues = []
    for r in shots:
        im = r["lcd"].resize((240, 240))
        px = [im.getpixel((x, y)) for x in range(8, 34) for y in range(102, 134)]
        hues.append(sum(1 for p in px if p[0] > 200 and p[2] < 120))   # bright amber pixels: C_HI
    check(min(cues) > 20 and max(hues) > 10 and min(hues) < max(hues),
          f"the chord boxes: the rhythm icon under the open-string row, blinking (lit pixels {cues}, bright "
          f"{hues}; m6p2-cue-0.png)")
    # the melody's tab: guitar, ukulele
    mel = m5.open_song(g) + m5.press_raw(O + 100, "ARP") + m5.hold(O + 200, "LFO")
    mt = m5.run("m6p2-melody-guitar-tab", P + 4000, mel + edits(O + 1400, 5) + m5.press_raw(P, "PLAY"))
    mu = m5.run("m6p2-melody-ukulele-tab", P + 4000, mel + edits(O + 1400, 5) + m5.press_raw(P, "PLAY"), flash="uke")
    check(mt["stats"]["view"] == 9 and mu["stats"]["view"] == 9 and lit(mt["lcd"], (0, 40, 240, 140)) > 500 and
          m5.ImageChops.difference(mt["lcd"], mu["lcd"]).getbbox() is not None,
          "the melody: GUITAR TAB (6 strings; m6p2-melody-guitar-tab.png) and, with the setting, UKULELE TAB "
          "(4 strings, A on top, G at the bottom; m6p2-melody-ukulele-tab.png): the fret numbers of playable "
          "positions")
    # the harmony's bar view
    bv = m5.run("m6p2-bar-view", P + 4000, base + edits(O + 400, 5) + m5.press_raw(P, "PLAY"))
    check(bv["stats"]["view"] == 10 and bv["stats"]["roll_keys"] >= 13 and lit(bv["lcd"], (30, 60, 240, 150)) > 300,
          "the harmony: BAR VIEW, the horizontal roll, each chord's notes as stacked bars, the piano keys on the "
          "left (m6p2-bar-view.png)")
    # zoom: SELECT in the scrolling and staff views; the tempo stays (REC: TEMPO, then SELECT)
    roll = mel + edits(O + 1400, 3)
    clicks = lambda t0, n: [f"{t0 + 150 * k}:e0:{1 if n > 0 else -1}" for k in range(abs(n))]
    z0 = m5.run("m6p2-zoom-0", P + 2500, roll + m5.press_raw(P, "PLAY"))
    zi = m5.run("m6p2-zoom-in", P + 2500, roll + clicks(P + 300, -5) + m5.press_raw(P, "PLAY"))
    zo = m5.run("m6p2-zoom-out", P + 2500, roll + clicks(P + 300, 9) + m5.press_raw(P, "PLAY"))
    s0, si, so = z0["stats"], zi["stats"], zo["stats"]
    bt = all_songs[g]["ppq"] * int(all_songs[g]["time_signature"][0])
    check(si["zoom"] < s0["zoom"] < so["zoom"] and si["bpm"] == s0["bpm"] == so["bpm"] and
          si["zoom"] / bt >= 0.3 and so["zoom"] / bt <= 8,
          f"SELECT zooms the melody roll: {si['zoom'] / bt:.1f} (min, m6p2-zoom-in.png) .. {s0['zoom'] / bt:.1f} "
          f"(start) .. {so['zoom'] / bt:.1f} bars on the screen (max, m6p2-zoom-out.png); the tempo unchanged "
          f"({s0['bpm']} BPM)")
    nt = m5.run("m6p2-zoom-notation", P + 2500, mel + edits(O + 1400, 4) + clicks(P + 300, 9) + m5.press_raw(P, "PLAY"))
    n0 = m5.run("m6p2-notation", P + 2500, mel + edits(O + 1400, 4) + m5.press_raw(P, "PLAY"))
    check(nt["stats"]["view"] == 6 and n0["stats"]["zoom"] < nt["stats"]["zoom"] <= 2 * n0["stats"]["zoom"] + 2,
          f"the notation zooms too, up to twice its span (the noteheads keep apart): {n0['stats']['zoom']} -> "
          f"{nt['stats']['zoom']} ticks (m6p2-zoom-notation.png)")
    tp = m5.run("m6p2-tempo-mode", P + 2500, roll + m5.press_raw(P + 300, "REC") + clicks(P + 600, 6)
                + m5.press_raw(P, "PLAY"))
    ch = m5.run("m6p2-tempo-chord", P + 1500, m5.open_song(g) + clicks(O + 300, 3))
    check(tp["stats"]["bpm"] == s0["bpm"] + 6 and tp["stats"]["zoom"] == s0["zoom"] and
          ch["stats"]["bpm"] == s0["bpm"] + 3,
          f"REC, then SELECT: the tempo there ({tp['stats']['bpm']} BPM), the zoom kept; in the other views SELECT "
          f"is the tempo as before ({ch['stats']['bpm']})")
    rep += lines
    return st["ok"]


PARTS = {"m6p1": test_m6p1, "m6p2": test_m6p2}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    ap.add_argument("--pkg")
    ap.add_argument("--out")
    ap.add_argument("--jobs", type=int, default=3)
    ap.add_argument("--quick", action="store_true", help="skip the long loudness runs")
    a = ap.parse_args()
    if a.quick:
        os.environ["M6_QUICK"] = "1"
    m5.OUT = Path(a.out).resolve() if a.out else ROOT / "build/emu/m6"
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
    text = [f"# M6 emulator tests\n\nPackage: {m5.PKG}, sha256 {sha}\n"]
    for k in reports:
        text += reports[k]
    text.append(f"\n**{'ALL PASS' if all(results.values()) else 'FAILURES: ' + ', '.join(k for k, v in results.items() if not v)}**\n")
    (m5.OUT / "report.md").write_text("\n".join(text))
    print("\n".join(text))
    sys.exit(0 if all(results.values()) else 1)


if __name__ == "__main__":
    main()
