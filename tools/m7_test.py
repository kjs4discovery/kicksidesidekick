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
             "fx_active synth_engine synth_bpm synth_level levels drum_playing").split()
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


# --------------------------------------------------------------------------------------------- part 2
SY_MENU, SY_HELP, SY_SET = 0xF2, 0xF3, 0xF4        # player_stats.synth_page for the screens that are not pages


def syrun(name, ms, sc, flash="songs"):
    return m5.run(name, ms, SYNTH + sc, mode=None, flash=flash)


def presses(t, names, gap=200):
    return [x for k, n in enumerate(names) for x in m5.press_raw(t + gap * k, n)]


def test_m7p2(all_songs, rep):
    lines, st, check = m5.checker("M7 part 2: the SYNTH UI on the parameter table")
    m = syrun("m7p2-main", 5200, [])
    s = m["stats"]
    check(s["mode"] == 2 and s["ui_view"] == V_SYNTH and s["synth_page"] == 0 and s["synth_bpm"] == 120 and
          s["drum_pattern"] == 0 and s["drum_on"] == 0,
          "SYNTH: the main screen (the preset big, the drum pattern ROCK1, 120 BPM, the instruction row; m7p2-main.png)")
    a = syrun("m7p2-preset", 5600, ["4600:e6:3"])
    b = syrun("m7p2-preset-back", 5600, ["4600:e6:-1"])
    check(a["stats"]["synth_preset"] == 3 and b["stats"]["synth_preset"] > 3,
          f"PRESET: the preset (+3: {a['stats']['synth_preset'] + 1}; -1 from the first: the last, "
          f"{b['stats']['synth_preset'] + 1})")
    t = syrun("m7p2-tempo", 6200, [f"{4600 + 120 * k}:e0:1" for k in range(10)])
    tf = syrun("m7p2-tempo-fast", 5600, ["4600:e0:4"])
    check(t["stats"]["synth_bpm"] == 130 and tf["stats"]["synth_bpm"] > 124,
          f"SELECT: the tempo (10 slow clicks: {t['stats']['synth_bpm']} BPM; turned quickly, 5 a click: "
          f"{tf['stats']['synth_bpm']})")
    q = syrun("m7p2-quick", 5000, ["4600:e4:5"])
    check(q["stats"]["fx_reverb"] == m["stats"]["fx_reverb"] + 10 and q["stats"]["synth_page"] == 0,
          f"K3 on the main screen: the reverb mix ({m['stats']['fx_reverb']} -> {q['stats']['fx_reverb']}), the bottom "
          f"row shows it (m7p2-quick.png)")
    pages = []
    for k in range(10):
        r = syrun(f"m7p2-edit-{k}", 4700 + 220 * k + 300, presses(4600, ["EDIT"] * k, 220))
        pages.append(r["stats"]["synth_page"])
    check(pages == [0, 1, 2, 3, 4, 5, 6, 7, 8, 9][:1] + [1, 2, 3, 4, 5, 6, 7, 8, 9], f"EDIT: the pages in turn, "
          f"after the ninth the main screen again ({pages})")
    for k in (1, 2, 5, 9):               # (each page's screen, for docs/controls.md)
        syrun(f"m7p2-page-{k}", 4700 + 220 * k + 300, presses(4600, ["EDIT"] * k, 220))
    ab = syrun("m7p2-algo-back", 5000, ["4600:e1:-1"])
    af = syrun("m7p2-algo-fwd", 5000, ["4600:e1:2"])
    check(ab["stats"]["synth_page"] == 9 and af["stats"]["synth_page"] == 2,
          f"ALGO from the main screen: back to the last page ({ab['stats']['synth_page']}), forward 2: page "
          f"{af['stats']['synth_page']}")
    ev = syrun("m7p2-env", 5000, m5.press_raw(4600, "ENV"))
    fx = syrun("m7p2-fx", 5000, m5.press_raw(4600, "FX"))
    check(ev["stats"]["synth_page"] == 1 and fx["stats"]["synth_page"] == 6,
          "ENV: the ENVELOPE page; FX: FX 1 (m7p2-fx.png)")
    rv = syrun("m7p2-page-k3", 5200, ["4600:e1:-1", "4800:e4:5"])
    check(rv["stats"]["fx_reverb"] == m["stats"]["fx_reverb"] + 10, "on FX 4, K3: the reverb mix (the page's third)")
    hm = syrun("m7p2-home", 5400, m5.press_raw(4600, "FX") + m5.press_raw(4900, "HOME"))
    check(hm["stats"]["synth_page"] == 0 and hm["stats"]["ui_view"] == V_SYNTH, "HOME (a tap) on a page: the main "
          "screen")
    mn = syrun("m7p2-menu", 5200, m5.press_raw(4600, "SEQ") + ["4800:e0:3"])
    ch = syrun("m7p2-menu-choose", 5600, m5.press_raw(4600, "SEQ") + ["4800:e0:3"] + m5.press_raw(5000, "SEQ"))
    check(mn["stats"]["synth_page"] == SY_MENU and ch["stats"]["synth_page"] == 0 and ch["stats"]["drum_pattern"] == 3
          and ch["stats"]["drum_on"] == 0, "SEQ: the drum pattern menu (m7p2-menu.png); SELECT, SEQ: chosen "
          "(the 4th), back to the main screen, the drums still off")
    pl = syrun("m7p2-menu-play", 5600, m5.press_raw(4600, "SEQ") + ["4800:e0:1"] + m5.press_raw(5000, "PLAY"))
    hb = syrun("m7p2-menu-home", 5600, m5.press_raw(4600, "SEQ") + ["4800:e0:1"] + m5.press_raw(5000, "HOME"))
    check(pl["stats"]["drum_pattern"] == 1 and pl["stats"]["drum_on"] == 1 and hb["stats"]["drum_pattern"] == 0 and
          hb["stats"]["synth_page"] == 0, "in the menu PLAY chooses and plays; HOME goes back unchanged")
    dr = syrun("m7p2-play", 5600, m5.press_raw(4600, "PLAY"))
    d2 = syrun("m7p2-play-off", 5600, presses(4600, ["PLAY", "PLAY"], 300))
    check(dr["stats"]["drum_on"] == 1 and d2["stats"]["drum_on"] == 0, "PLAY: the drums on, PLAY again: off")
    hp = syrun("m7p2-help", 5200, m5.press_raw(4600, "GLO"))
    se = syrun("m7p2-settings", 5400, presses(4600, ["GLO", "GLO"]))
    check(hp["stats"]["synth_page"] == SY_HELP and se["stats"]["synth_page"] == SY_SET,
          "GLO: SYNTH's controls (m7p2-help.png); GLO again: SYNTH's settings (m7p2-settings.png)")
    kit = syrun("m7p2-kit", 6400, presses(4600, ["GLO", "GLO"]) + ["5100:e0:1", "5300:e1:1"]
                + m5.press_raw(5600, "GLO"))
    check(kit["stats"]["drum_kit"] == 1 and kit["stats"]["synth_page"] == 0 and kit["stats"]["settings_saves"] >= 1,
          "settings: Drum kit 808 (ALGO), GLO: saved, the main screen")
    m5.FLASH["kit808"] = kit["flash"]
    kp = syrun("m7p2-kit-power", 5200, [], flash="kit808")
    check(kp["stats"]["drum_kit"] == 1, "the kit kept over a power-off (OBJ_SETTINGS)")
    ar = syrun("m7p2-arp", 7000, m5.press_raw(4600, "ARP") + ["5000:C4", "5000:G4", "6800:C4^", "6800:G4^"])
    check(ar["stats"]["arp_pattern"] == 1 and ar["stats"]["arp_steps"] >= 6, f"ARP: the arpeggiator on (UP 1/8: "
          f"{ar['stats']['arp_steps']} steps in 1.8 s at 120 BPM)")
    bad = []
    for n in ("LFO", "SCL", "SAVE", "REC"):
        r = syrun(f"m7p2-free-{n}", 5000, m5.press_raw(4600, n))
        if not (r["stats"]["popup_open"] == 1 and r["stats"]["popup_btn"] == B[n]):
            bad.append(n)
    check(not bad, f"LFO, SCL, SAVE, REC: the NOT IMPLEMENTED pop-up (m7p2-free-SAVE.png) {bad or ''}")
    k = syrun("m7p2-keys", 6400, ["5000:C4", "5000:E4", "5000:G4", "6200:C4^", "6200:E4^", "6200:G4^"])
    snd = m5.rms(k["left"][int(5.2 * m5.RATE):int(6.1 * m5.RATE)])
    check(k["stats"]["synth_notes"] == 3 and snd > 300 and k["stats"]["synth_peak"] < 8388607,
          f"the keys play the preset (3 voices, rms {snd:.0f}, no clipping)")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 3
KEYS8 = ("C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5")


def fx_max(t):
    """FX 1..4 pages, every knob turned to its top (and on FX 4 the level left): the script from time t"""
    sc = m5.press_raw(t, "FX")
    for pg in range(4):
        for k in range(4 if pg < 3 else 3):
            sc.append(f"{t + 200 + 3000 * pg + 700 * k}:e{2 + k}:64")   # (64 clicks arrive over ~0.6 s)
        sc.append(f"{t + 200 + 3000 * pg + 2900}:e1:1")
    return sc


def test_m7p3(all_songs, rep):
    lines, st, check = m5.checker("M7 part 3: the effects")
    names = ["fuzz", "crush", "tremolo", "phaser", "chorus", "delay", "reverb"]
    d = syrun("m7p3-dry", 6600, ["4600:e6:10"] + m5.press_raw(4700, "FX") + ["4900:e3:-64", "5000:e3:-64"]
              + m5.press_raw(5100, "HOME") + ["5300:e4:-64", "5400:e5:-64", "5600:C4", "6400:C4^"])
    check(d["stats"]["fx_active"] == 0, f"DRIVE BASS with its chorus, reverb and delay at 0: no effect runs "
          f"(ran {d['stats']['fx_active']:#04x})")
    a = syrun("m7p3-all", 19500, ["4500:e6:4"] + fx_max(4700) + [f"17000:{k}" for k in KEYS8]
              + [f"19100:{k}^" for k in KEYS8])
    s = a["stats"]
    on = [n for i, n in enumerate(names) if s["fx_active"] >> i & 1]
    load = s["synth_render_max"] / s["budget"] if s["budget"] else 9
    check(s["fx_active"] == 0x7F and s["synth_voices_max"] == 8 and not a["halt"] and s["synth_peak"] <= 8388607,
          f"every effect at its top ({', '.join(on)}), 8 voices of SYNC SCREAM: the synth render {load * 100:.0f} % of "
          f"a block (emulated), no crash")
    snd = m5.rms(a["left"][int(17.2 * m5.RATE):int(18.9 * m5.RATE)])
    check(snd > 300, f"... and it sounds (rms {snd:.0f})")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 4
def kit_set(t, kit, vol=None):
    """SYNTH's settings: the drum kit (0 ACOUSTIC, 1 808, 2 CHIP), the drum volume (0..10); back at t + 1500"""
    sc = presses(t, ["GLO", "GLO"])
    if vol is not None:
        sc.append(f"{t + 450}:e1:{vol - 7}")
    if kit:
        sc += [f"{t + 700}:e0:1", f"{t + 900}:e1:{kit}"]
    return sc + m5.press_raw(t + 1300, "GLO")


def test_m7p4(all_songs, rep):
    lines, st, check = m5.checker("M7 part 4: the drums")
    seg = lambda r, a, b: r["left"][int(a * m5.RATE):int(b * m5.RATE)]
    res = {}
    for kit, name in enumerate(("ACOUSTIC", "808", "CHIP")):
        r = syrun(f"m7p4-{name.lower()}", 10200, kit_set(4600, kit) + m5.press_raw(6200, "PLAY"))
        s = r["stats"]
        res[name] = (s["drum_hits"], m5.rms(seg(r, 6.4, 10.2)), s["drum_voices_max"], s["drum_kit"])
        check(s["drum_on"] == 1 and s["drum_kit"] == kit and 25 <= s["drum_hits"] <= 28 and res[name][1] > 100 and
              not r["halt"] and (kit == 0 or 1 <= s["drum_voices_max"] <= 6),
              f"kit {name}: PLAY, ROCK1 at 120 BPM, 4 s: {s['drum_hits']} hits (13 a bar, 2 s a bar: 26), rms "
              f"{res[name][1]:.0f}, its own voices at most {s['drum_voices_max']} (m7p4-{name.lower()}.png)")
    r = syrun("m7p4-tempo", 10200, ["4600:e0:30"] + m5.press_raw(6200, "PLAY"))
    bpm = r["stats"]["synth_bpm"]
    want = 13 * 4 * bpm / 240
    check(bpm > 150 and abs(r["stats"]["drum_hits"] - want) <= 4,
          f"at {bpm} BPM: {r['stats']['drum_hits']} hits in 4 s ({want:.0f} wanted): the drums follow the tempo")
    # a pattern chosen while playing: from the next bar. ROCK1 bars are 2 s at 120 BPM; PLAY at 6.2 s; POP1 (index 3)
    # chosen at 7.0 s: still ROCK1 at 7.9 s, POP1 after 8.2 s
    ch = m5.press_raw(6200, "PLAY") + m5.press_raw(6800, "SEQ") + ["6900:e0:3"] + m5.press_raw(7000, "SEQ")
    a = syrun("m7p4-next-bar-a", 7900, ch)
    b = syrun("m7p4-next-bar-b", 8500, ch)
    check(a["stats"]["drum_pattern"] == 3 and a["stats"]["drum_playing"] == 0 and b["stats"]["drum_playing"] == 3,
          f"a pattern chosen mid-bar (POP1): ROCK1 plays to the bar's end ({a['stats']['drum_playing']}), then POP1 "
          f"({b['stats']['drum_playing']})")
    off = syrun("m7p4-off", 9000, m5.press_raw(6200, "PLAY") + m5.press_raw(7000, "PLAY"))
    q = m5.rms(seg(off, 8.5, 9.0))
    check(off["stats"]["drum_on"] == 0 and q < 30, f"PLAY again: the drums stop (rms after {q:.0f})")
    lo = syrun("m7p4-vol0", 10200, kit_set(4600, 1, vol=0) + m5.press_raw(6200, "PLAY"))
    hi = syrun("m7p4-vol10", 10200, kit_set(4600, 1, vol=10) + m5.press_raw(6200, "PLAY"))
    l0, l1 = m5.rms(seg(lo, 6.4, 10.2)), m5.rms(seg(hi, 6.4, 10.2))
    check(l0 < 30 and l1 > 1.2 * res["808"][1], f"drum volume 0: silent (rms {l0:.0f}); 10: louder than 7 ({l1:.0f} vs "
                                                f"{res['808'][1]:.0f})")
    ks = syrun("m7p4-keys", 10200, kit_set(4600, 1) + m5.press_raw(6200, "PLAY") + [f"7000:{k}" for k in KEYS8]
               + [f"9500:{k}^" for k in KEYS8])
    check(ks["stats"]["synth_voices_max"] == 8 and 1 <= ks["stats"]["drum_voices_max"] <= 6,
          f"8 keys over the 808 drums: 8 synth voices and {ks['stats']['drum_voices_max']} drum voices (never the "
          f"synth's)")
    lines.append(f"  - loudness, ROCK1, drum volume 7: " + ", ".join(f"{k} rms {v[1]:.0f}" for k, v in res.items()))
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 5
def test_m7p5(all_songs, rep):
    lines, st, check = m5.checker("M7 part 5: engines and the 30 presets")
    ch4 = ["5000:C4", "5000:E4", "5000:G4", "5000:B4", "6400:C4^", "6400:E4^", "6400:G4^", "6400:B4^"]
    every = os.environ.get("M7_ALL_PRESETS") is not None
    bad, out, eng = [], [], {}
    for i in range(0, 30, 1 if every else 3):
        r = syrun(f"m7p5-preset-{i}", 6600, ([f"4600:e6:{i}"] if i else []) + ch4)
        s = r["stats"]
        v = max(m5.rms(r["left"][int(t * m5.RATE):int((t + 0.1) * m5.RATE)]) for t in
                [4.8 + 0.02 * k for k in range(80)])   # (the loudest 100 ms, as the laptop's level matching; the
        # recorded audio runs ~0.1 s ahead of the script)
        out.append((i, round(v)))
        eng[i] = s["synth_engine"]
        if s["synth_preset"] != i or (v < 300 and i != 8) or s["synth_peak"] >= 8388607 or r["halt"]:   # (RISER: slow)
            bad.append((i, s["synth_preset"], round(v)))
    check(not bad, f"the presets by PRESET ({'all 30' if every else 'every 3rd'}): each sounds, none clips {out} "
                   f"{bad or ''}")
    n = syrun("m7p5-count", 5000, ["4600:e6:-1"])
    check(n["stats"]["synth_preset"] == 29, f"30 presets (PRESET back from the first: number "
                                            f"{n['stats']['synth_preset'] + 1})")
    ep = syrun("m7p5-epiano-sound", 5600, ["4600:e6:19"] + presses(4900, ["EDIT", "EDIT", "EDIT"], 200))
    an = syrun("m7p5-analog-sound", 5600, presses(4900, ["EDIT", "EDIT", "EDIT"], 200))
    check(ep["stats"]["synth_engine"] == 1 and an["stats"]["synth_engine"] == 0 and ep["stats"]["synth_page"] == 3 and
          m5.ImageChops.difference(ep["lcd"], an["lcd"]).getbbox() is not None,
          "E-PIANO is a PHASE preset: its SOUND A page shows PHASE's parameters (m7p5-epiano-sound.png), NEON PAD's "
          "ANALOG's (m7p5-analog-sound.png)")
    pk = syrun("m7p5-phase-chord", 6600, ["4600:e6:19"] + ch4)
    v = m5.rms(pk["left"][int(5.1 * m5.RATE):int(6.3 * m5.RATE)])
    check(v > 300 and pk["stats"]["synth_voices_max"] == 4, f"E-PIANO (PHASE): a 4-note chord, 4 voices (rms {v:.0f})")
    lv = [x for i, x in out if i not in (8,)]
    spread = max(lv) / min(lv) if lv and min(lv) else 99
    check(spread < 1.6, f"the presets level-matched (LEVEL): the chord's loudest 100 ms within {spread:.2f}x of each other "
                        f"(RISER left out: its 6 s attack; the 100 ms loudest of each, laptop: within 0.3 dB)")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 6
BUTTONS = list(B)
# the screens of the census: name -> (the script that opens it from power-on, mode for m5.run, its exclusions: buttons
# allowed to do nothing there, D044: the mode screen and the WIPE question are not given the pop-up)
P = m5.PLAY_MS


def census_screens(g):
    sy = lambda sc: (SYNTH + sc, None)
    return {
        "LEARN folders": ((m5.press_raw(4400, "HOME"), "LEARN"), set()),
        "LEARN songs": (([], "LEARN"), set()),
        "LEARN play, stopped": ((m5.open_song(g) + m5.press_raw(O + 300, "ARP"), "LEARN"), set()),
        "LEARN play, playing": ((m5.open_song(g) + m5.press_raw(P, "PLAY"), "LEARN"), set()),
        "LEARN help": ((m5.press_raw(4400, "GLO"), "LEARN"), set()),
        "LEARN settings": ((presses(4400, ["GLO", "GLO"]), "LEARN"), set()),
        "LEARN views": ((presses(4400, ["GLO", "GLO"]) + ["4900:e0:7"] + m5.press_raw(5100, "PLAY"), "LEARN"), set()),
        "mode screen": (([], None), set(BUTTONS) - {"PLAY", "EDIT"}),
        "SYNTH main": (sy([]), set()),
        "SYNTH page": (sy(m5.press_raw(4600, "ENV")), set()),
        "SYNTH menu": (sy(m5.press_raw(4600, "SEQ")), set()),
        "SYNTH help": (sy(m5.press_raw(4600, "GLO")), set()),
        "SYNTH settings": (sy(presses(4600, ["GLO", "GLO"])), set()),
    }


WATCH = ("ui_view mode synth_page drum_on drum_pattern arp_pattern keys_octave settings folder view loop_step "
         "popups starts pauses transpose part drum_kit synth_bpm").split()


def test_m7p6(all_songs, rep):
    lines, st, check = m5.checker("M7 part 6: the button census, the rescues, HOME taps, leaving SYNTH")
    g = [c["slug"] for c in all_songs].index("amazing-grace")
    scr = census_screens(g)
    end = {"LEARN play, stopped": O + 1700, "LEARN play, playing": P + 1300}
    table, bad = [], []
    from concurrent.futures import ThreadPoolExecutor

    def one(job):
        name, b = job
        (sc, mode), _ = scr[name]
        t = max([int(e.split(":")[0]) for e in sc] + [4300]) + 400 if sc else (4600 if mode else 4300)
        t = max(t, end.get(name, 0))
        if b is None:
            return name, b, m5.run(f"m7p6-c-{name.replace(' ', '-').replace(',', '')}", t + 500, sc, mode=mode)
        return name, b, m5.run(f"m7p6-c-{name.replace(' ', '-').replace(',', '')}-{b}", t + 500,
                               sc + m5.press_raw(t, b), mode=mode)
    jobs = [(n, None) for n in scr] + [(n, b) for n in scr for b in BUTTONS]
    with ThreadPoolExecutor(3) as ex:
        res = {(n, b): r for n, b, r in ex.map(one, jobs)}
    for name, (_, excl) in scr.items():
        base = res[(name, None)]
        row = []
        for b in BUTTONS:
            r = res[(name, b)]
            s, s0 = r["stats"], base["stats"]
            if r["halt"] or s is None:
                row.append(f"{b}: CRASH")
                bad.append((name, b, "crash"))
                continue
            if s["popup_open"] and s["popup_btn"] == B[b] and s["popups"] > s0["popups"]:
                what = "pop-up"
            elif any(s[k] != s0[k] for k in WATCH) or m5.ImageChops.difference(r["lcd"], base["lcd"]).getbbox():
                what = "job"
            else:
                what = "nothing"
            if what == "pop-up" and name in ("mode screen",):
                bad.append((name, b, "a pop-up where D044 has none"))
            if what == "nothing" and b not in excl:
                bad.append((name, b, "nothing"))
            row.append(f"{b} {what}")
        table.append(f"  - {name}: " + ", ".join(row))
    check(not bad, f"the button census: {len(scr)} screens x 14 buttons, each its job or the pop-up, none does nothing "
                   f"(but the mode screen's, D044), no crash {bad or ''}")
    lines += table
    # the power-on rescues with M7: fm1-base's two, and ours (GLO + HOME: the WIPE question), no pop-up anywhere
    oc = m5.run("m7p6-boot-oct-both", 4000, ["0:OCTDN", "0:OCTUP"], mode=None)
    rc = m5.run("m7p6-boot-rec", 4000, ["0:REC"], mode=None)
    wp = m5.run("m7p6-boot-wipe", 6000, ["0:GLO", "0:HOME", "5600:GLO^", "5600:HOME^"], mode=None)
    check(oc["stats"] is not None and oc["stats"]["ui_view"] == 0xFF and not oc["halt"] and
          (rc["stats"] is None or rc["stats"]["ui_view"] == 0xFF) and not rc["halt"],
          "OCT- + OCT+ and REC held at power-on: fm1-base's panel set-up / update mode, our screens never start "
          "(m7p6-boot-oct-both.png, m7p6-boot-rec.png)")
    check(wp["stats"]["ui_view"] == V_WIPE and wp["stats"]["popups"] == 0 and wp["stats"]["mode_switches"] == 0,
          "GLO + HOME held at power-on: the WIPE question, no pop-up (m7p6-boot-wipe.png)")
    # 24 quick HOME taps (with PLAY between) in SYNTH: never the mode screen
    taps = [x for k in range(24) for x in m5.press_raw(4600 + 251 * k + k % 3, "PLAY" if k % 2 else "HOME")]
    tp = syrun("m7p6-synth-home-taps", 4600 + 251 * 24 + 400, taps)
    check(tp["stats"]["ui_view"] == V_SYNTH and tp["stats"]["mode_switches"] == 0,
          "SYNTH: 24 quick HOME / PLAY taps: never the mode screen")
    # leaving SYNTH with the drums on: silent on the mode screen; LEARN: a song plays
    lj = syrun("m7p6-drums-leave", 14000, m5.press_raw(4600, "PLAY") + m6.hold_home(5200) + [
        "7800:e0:-1", "8000:PLAY!", "8060:PLAY^!"] + ([f"8300:e0:{g}"] if g else []) +
        ["8600:PLAY!", "8660:PLAY^!", "9100:PLAY!", "9160:PLAY^!"])
    q = m5.rms(lj["left"][int(7.5 * m5.RATE):int(7.9 * m5.RATE)])
    snd = m5.rms(lj["left"][int(12.0 * m5.RATE):int(14.0 * m5.RATE)])
    check(q < 30 and lj["stats"]["mode"] == 1 and lj["stats"]["ui_view"] == V_PLAY and snd > 50 and not lj["halt"],
          f"the drums on, HOME held: silence on the mode screen (rms {q:.0f}); LEARN, a song: it plays (rms {snd:.0f})")
    rep += lines
    return st["ok"]


def cpu_worst(rep):
    """the budget (Part 6): 8 voices of each preset (the heaviest found), then that preset with every effect at its top
    and the busiest drum pattern on each kit: (synth render + EAS render) / one block"""
    lines, st, check = m5.checker("M7 budget: CPU worst case")
    loads = {}
    from concurrent.futures import ThreadPoolExecutor

    def one(i):
        r = syrun(f"m7cpu-preset-{i}", 7400, ([f"4600:e6:{i}"] if i else []) + [f"5000:{k}" for k in KEYS8]
                  + [f"7000:{k}^" for k in KEYS8])
        s = r["stats"]
        return i, s["synth_render_max"] / s["budget"]
    with ThreadPoolExecutor(3) as ex:
        loads = dict(ex.map(one, range(30)))
    heavy = max(loads, key=loads.get)
    lines.append("  - 8 voices, each preset as made: " + ", ".join(f"{i + 1} {v * 100:.0f}%" for i, v in loads.items()))
    words = [x for x in (ROOT / "firmware/app/jam_patterns.h").read_text().split("{\"")[1:]]
    busiest, most = 0, -1
    for k, w in enumerate(words):
        steps = [int(x, 16) for x in w.split("{")[1].split("}")[0].split(",")]
        hits = max(sum(1 for i in range(8) if st_ >> 2 * i & 3) for st_ in steps)
        tot = sum(sum(1 for i in range(8) if st_ >> 2 * i & 3) for st_ in steps) / len(steps)
        if hits * 100 + tot > most:
            most, busiest = hits * 100 + tot, k
    worst = 0
    for kit, name in enumerate(("ACOUSTIC", "808", "CHIP")):
        sc = ([f"4500:e6:{heavy}"] if heavy else []) + fx_max(4700) + kit_set(17000, kit) + \
            m5.press_raw(18600, "SEQ") + [f"18800:e0:{busiest}"] + m5.press_raw(19200, "PLAY") + \
            [f"19400:{k}" for k in KEYS8] + [f"23000:{k}^" for k in KEYS8]
        r = syrun(f"m7cpu-worst-{name.lower()}", 23400, sc)
        s = r["stats"]
        load = (s["synth_render_max"] + s["render_max"]) / s["budget"] if s["budget"] else 9
        worst = max(worst, load)
        check(load <= 0.60 and s["fx_active"] == 0x7F and s["drum_on"] == 1 and s["synth_voices_max"] == 8 and
              not r["halt"], f"{name}: preset {heavy + 1} x 8 voices, every effect at its top, pattern {busiest + 1}: "
              f"synth + EAS render {load * 100:.0f} % of a block (limit 60 %)")
    rep += lines
    return st["ok"], loads, heavy, worst


PARTS = {"m7p1": test_m7p1, "m7p2": test_m7p2, "m7p3": test_m7p3, "m7p4": test_m7p4, "m7p5": test_m7p5,
         "m7p6": test_m7p6, "m7cpu": lambda a, rep: cpu_worst(rep)[0]}


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
