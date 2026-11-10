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
             "arp_rate jam_style jam_level").split()
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
    # (the 0.6.0-m6 suite's find: a hold's start, now | 1, could be 1 ms ahead of now, and the next pass in that ms
    # read it as held 49 days: a HOME tap went to the mode screen) 24 taps at every ms parity, open/closing a song
    taps = [x for k in range(24) for x in m5.press_raw(4600 + 251 * k + k % 3, "PLAY" if k % 2 else "HOME")]
    tp = m5.run("m6p1-home-taps", 4600 + 251 * 24 + 400, taps)
    check(tp["stats"]["ui_view"] != V_MODE and tp["stats"]["mode_switches"] == 0,
          "24 quick HOME / PLAY taps (song opened and closed 12 times): never the mode screen")
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


# --------------------------------------------------------------------------------------------- part 3
def seg(r, a, b):
    return r["left"][int(a * m5.RATE):int(b * m5.RATE)]


def test_m6p3(all_songs, rep):
    lines, st, check = m5.checker("M6 part 3: SYNTH, round 2")
    S = SYNTH
    run = lambda name, ms, sc: m5.run(name, ms, S + sc, mode=None)
    preset = lambda i: [f"4600:e0:{i}"] if i else []
    shift = lambda t, enc, n: [f"{t}:EDIT", f"{t + 100}:{enc}:{n}", f"{t + 1200}:EDIT^"]   # (the turn takes a while)
    # the reverb (SHIFT + K1): CHROME BRASS with its delay off (PRESET), one note, its tail
    note = ["4700:e6:-64", "5200:C4", "5600:C4^"]
    note = ["4600:e6:-64", "6200:C4", "6600:C4^"]
    dry = run("m6p3-reverb-dry", 7400, preset(5) + shift(4700, "e2", -64) + note)
    wet = run("m6p3-reverb-wet", 7400, preset(5) + shift(4700, "e2", 64) + note)
    td, tw = m5.rms(seg(dry, 6.8, 7.3)), m5.rms(seg(wet, 6.8, 7.3))
    check(dry["stats"]["fx_reverb"] == 0 and wet["stats"]["fx_reverb"] == 127 and tw > 4 * td + 50,
          f"the reverb (SHIFT + K1, 0..127; {dry['stats']['fx_reverb']}, {wet['stats']['fx_reverb']}): the tail 0.2..0.7 s "
          f"after the note, rms {td:.0f} dry, {tw:.0f} at 127")
    # the bit-crush (SHIFT + K2)
    c0 = run("m6p3-crush-0", 7400, preset(19) + ["6200:C4", "7300:C4^"])
    c1 = run("m6p3-crush-127", 7400, preset(19) + shift(4700, "e3", 64) + ["6200:C4", "7300:C4^"])
    a, b = seg(c0, 6.5, 7.2), seg(c1, 6.5, 7.2)
    diff = m5.rms([x - y for x, y in zip(a, b)])
    check(c1["stats"]["fx_crush"] == 127 and diff > 0.2 * m5.rms(a) and c1["stats"]["synth_peak"] < 8388607,
          f"the bit-crush (SHIFT + K2, {c1['stats']['fx_crush']}): the sound changed (difference rms {diff:.0f} of "
          f"{m5.rms(a):.0f}), no clipping (peak {c1['stats']['synth_peak']})")
    cb = run("m6p3-chip-blip", 5200, preset(9))
    check(cb["stats"]["fx_crush"] == 70, f"CHIP BLIP comes with its bit-crush ({cb['stats']['fx_crush']})")
    # SHIFT on the screen while held
    sh = run("m6p3-shift", 5200, ["4800:EDIT"])
    check(sh["stats"]["shift"] == 1, "EDIT held: SHIFT, K1-K4's second set on the screen (m6p3-shift.png)")
    # the arp: rates (GLO, or SHIFT + K3), 1/8T, RANDOM (SEQ x3 from UP)
    keys2 = ["5000:C4", "5000:G4", "7000:C4^", "7000:G4^"]
    on = ["4600:ARP", "4700:ARP^"]
    r8t = run("m6p3-arp-8t", 7200, on + ["4800:GLO", "4900:GLO^"] + keys2)   # 1/8 -> 1/8T
    rsh = run("m6p3-arp-shift-rate", 7200, on + shift(4750, "e4", 2) + keys2)   # SHIFT + K3: 1/8 -> 1/8T
    rr = run("m6p3-arp-random", 7200, on + [x for k in range(3) for x in (f"{4750 + 120 * k}:SEQ", f"{4800 + 120 * k}:SEQ^")]
             + ["5000:C4", "5000:E4", "5000:G4", "5000:B4", "7000:C4^", "7000:E4^", "7000:G4^", "7000:B4^"])
    s8, ss, sr = r8t["stats"], rsh["stats"], rr["stats"]
    check(s8["arp_rate"] == 2 and 11 <= s8["arp_steps"] <= 13 and ss["arp_rate"] == 2 and
          sr["arp_pattern"] == 4 and sr["arp_steps"] >= 7,
          f"the arpeggiator: 1/8T by GLO ({s8['arp_steps']} steps in 2 s at 120 BPM, 12 wanted) or by SHIFT + K3; "
          f"RANDOM by SEQ ({sr['arp_steps']} steps)")
    # the drum jam: PLAY on, the style (FX), its level (SCL), the tempo (ARP held + SELECT), looped, under the synth
    jam = run("m6p3-jam", 9000, ["4600:PLAY", "4700:PLAY^", "6000:C4", "6000:E4", "6000:G4", "8800:C4^", "8800:E4^",
                                 "8800:G4^"])
    js = jam["stats"]
    drums = m5.rms(seg(jam, 4.9, 5.9))
    both = m5.rms(seg(jam, 6.2, 8.7))
    check(js["jam"] == 1 and js["jam_notes"] > 8 and drums > 100 and both > drums and js["mode"] == 2,
          f"PLAY: the drum jam (POP L2 at 120 BPM), {js['jam_notes']} drum notes in 4.4 s (rms {drums:.0f}), the synth "
          f"over it (rms {both:.0f}; m6p3-jam.png)")
    fast = run("m6p3-jam-loop", 17000, ["4500:ARP", "4600:e0:80", "6000:ARP^", "6100:PLAY", "6200:PLAY^"])
    fs = fast["stats"]
    check(fs["bpm"] == 200 and fs["loops"] >= 1 and fs["jam"] == 1,
          f"ARP held + SELECT: the jam's tempo too ({fs['bpm']} BPM); bars 1-8 looped ({fs['loops']} loop jumps in "
          f"10 s)")
    sty = run("m6p3-jam-style", 8000, ["4600:FX!", "4700:FX^!", "4800:SCL!", "4900:SCL^!", "5000:PLAY", "5100:PLAY^"])
    s1 = sty["stats"]
    check(s1["jam"] == 1 and s1["jam_notes"] > 8 and s1["jam_style"] == 5 and s1["jam_level"] == 3 and
          m5.rms(seg(sty, 6, 8)) > 100,
          "FX: the next style (TRAIN), SCL: the next level (L3); PLAY: it plays (m6p3-jam-style.png)")
    lv = run("m6p3-jam-level", 9000, ["4600:PLAY", "4700:PLAY^", "6500:SCL!", "6600:SCL^!"])
    check(lv["stats"]["jam"] == 1 and lv["stats"]["jam_level"] == 3 and not lv["halt"] and m5.rms(seg(lv, 7.5, 9)) > 100,
          "SCL while it plays: L3 from the next beat, the drums go on")
    off = run("m6p3-jam-off", 7600, ["4600:PLAY", "4700:PLAY^", "6000:PLAY", "6100:PLAY^"])
    check(off["stats"]["jam"] == 0 and m5.rms(seg(off, 7.0, 7.6)) < 30, "PLAY again: the drums stop")
    # CPU: 8 voices of SYNC SCREAM, chorus, delay, reverb, and the jam at L3
    keys8 = ("C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5")
    cp = run("m6p3-cpu", 9000, preset(4) + shift(4700, "e2", 64) + ["5000:SCL!", "5100:SCL^!", "5200:PLAY", "5300:PLAY^"]
             + [f"6000:{k}" for k in keys8] + [f"8800:{k}^" for k in keys8])
    c = cp["stats"]
    load = (c["synth_render_max"] + c["render_max"]) / c["budget"] if c["budget"] else 9
    check(c["synth_voices_max"] == 8 and c["jam"] == 1 and load < 0.8 and not cp["halt"],
          f"8 voices of SYNC SCREAM with chorus, delay, reverb at 127 and the drum jam at L3: the longest synth render "
          f"plus the longest EAS render {load * 100:.0f} % of a block (emulated CPU)")
    # leaving SYNTH with the jam on: silence, then a song plays in LEARN
    g = [c["slug"] for c in all_songs].index("amazing-grace")
    lj = run("m6p3-jam-leave", 14000, ["4600:PLAY", "4700:PLAY^"] + hold_home(5200) + [
        "7800:e0:-1", "8000:PLAY!", "8060:PLAY^!"] + ([f"8300:e0:{g}"] if g else []) +
        ["8600:PLAY!", "8660:PLAY^!", "9100:PLAY!", "9160:PLAY^!"])
    q = m5.rms(seg(lj, 7.5, 7.9))
    snd = m5.rms(seg(lj, 12.0, 14.0))
    check(q < 30 and lj["stats"]["mode"] == 1 and lj["stats"]["ui_view"] == V_PLAY and snd > 50 and not lj["halt"],
          f"the jam on, HOME held: silence on the mode screen (rms {q:.0f}); LEARN, a song: it plays (rms {snd:.0f})")
    rep += lines
    return st["ok"]


PARTS = {"m6p1": test_m6p1, "m6p2": test_m6p2, "m6p3": test_m6p3}


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
