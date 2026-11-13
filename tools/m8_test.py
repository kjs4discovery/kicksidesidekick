#!/usr/bin/env python3
# m8_test.py -- Milestone M8: headless emulator tests of BACKTRACK mode and the renames (built on tools/m5_test.py's
# runner, its prepared flash images and presses, and tools/m6_test.py's / m7_test.py's). Never touches the FM-1.
# Licence: TBD (see D017)
#
#   .venv/bin/python -I tools/m8_test.py [--only m8p0,m8p1,..] [--pkg PKG] [--out DIR]   -> build/emu/m8/report.md
#
#   m8p0  measure (Part 0): the song buffer's use in LEARN for the largest song, LEARN's busiest song's CPU (EAS),
#         SYNTH's 4 voices of the heaviest preset with and without delay / reverb
#   m8p1  renames (D050): the mode screen's three choices and where each goes, HOME held / Switch mode back there, the
#         cursor on SONG LEARN, SYNTH starts on E-PIANO, the new titles, SONG LEARN lists no backtracks
#   m8p4  BACKTRACK: SETUP (defaults, K1..K4, SELECT, kept until power-off), PLAY (the structures' loops, WALTZ in 3/4,
#         the key re-spells, the keys play the melody sound on 4 voices), HOME back to SETUP, sends refused, leaving
#   m8p5  the scale views and key lights follow the chord; the Scale guide setting; EDIT between the two views
#   m8p6  the melody sound: K4 through the presets, kept until power-off
#   m8sweep  every style x 3 structures x 3 progressions plays (its end, or a loop round) with no crash, no late block
#   m8census the button census (D044) of SETUP and BACKTRACK's play screen; 24 quick HOME taps; the rescues' mode screen
#   m8cpu  the budget: the busiest style, FULL, every layer at its top, the heaviest preset's 4 voices with its effects
import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools/fm1song/.."))
import m5_test as m5                     # noqa: E402
import m6_test as m6                     # noqa: E402
import m7_test as m7                     # noqa: E402
from fm1song import binformat as bf      # noqa: E402

O = m5.OPEN_MS
PRESETS = [l.split('"')[1] for l in (ROOT / "firmware/app/vsynth.c").read_text().splitlines()
           if l.strip().startswith('PR("')]
KEYS4 = ("C4", "E4", "G4", "B4")


def need_bytes(c, blob=None):
    """the song buffer a compiled 2.x song needs in LEARN (library.c: the largest level of each layer, 2 events a note,
    8 bytes an event); 1.x: its decoded events"""
    total = 0
    for name, ly in c["layers"].items():
        most = 0
        for v, evs in ly["variants"].items():
            most = max(most, sum(1 for e in evs if e[1] == 0x90) * 2)
        total += most
    return total * 8


# --------------------------------------------------------------------------------------------- part 0
def test_m8p0(all_songs, rep):
    lines, st, check = m5.checker("M8 part 0: measure first")
    cs = m6.all_compiled()
    needs = sorted(((need_bytes(c), c["slug"]) for c in cs), reverse=True)
    lines.append("  - the song buffer (192 KiB = 196,608 B) in LEARN, the largest level of each layer: " +
                 ", ".join(f"{s} {n:,}" for n, s in needs))
    fl = m6.priv_flash(all_songs) if m6.PRIV else "songs"
    from concurrent.futures import ThreadPoolExecutor

    def learn(i):
        c = cs[i]
        sc = ([f"4150:e0:{i}"] if i else []) + m5.press_raw(O, "PLAY") + ["1500:pot:4096"]
        sc += [f"{O + 800}:e2:2", f"{O + 900}:e3:3", f"{O + 1000}:e4:4"]      # every layer at its top level
        r = m5.run(f"m8p0-learn-{c['slug']}", m5.PLAY_MS + 30000, sc + m5.press_raw(m5.PLAY_MS, "PLAY"), flash=fl)
        s = r["stats"]
        return c["slug"], s["render_max"] / s["budget"] if s["budget"] else 9, max(s["voices"]), s["levels"]
    with ThreadPoolExecutor(3) as ex:
        res = list(ex.map(learn, range(len(cs))))
    res.sort(key=lambda x: -x[1])
    lines.append("  - LEARN, every layer at its top level, 30 s, EAS render / block: " +
                 ", ".join(f"{s} {v * 100:.0f} % ({n} voices)" for s, v, n, _ in res))
    busiest = res[0]

    def synth(job):
        name, extra = job
        i = PRESETS.index("MIAMI PAD")
        sc = [f"4600:e6:{i}"] + extra + [f"6500:{k}" for k in KEYS4] + [f"9500:{k}^" for k in KEYS4]
        r = m7.syrun(f"m8p0-synth-{name}", 10000, sc)
        s = r["stats"]
        return name, s["synth_render_max"] / s["budget"] if s["budget"] else 9, s["fx_active"], s["synth_voices_max"]
    jobs = [("as-made", []), ("no-delay-reverb", ["5000:e4:-127", "5300:e5:-127"]),
            ("delay-reverb-top", ["5000:e4:127", "5300:e5:127"])]
    with ThreadPoolExecutor(3) as ex:
        sy = list(ex.map(synth, jobs))
    # every effect at its top (FX pages), 4 voices
    i = PRESETS.index("MIAMI PAD")
    r = m7.syrun("m8p0-synth-all-fx", 19500, [f"4500:e6:{i}"] + m7.fx_max(4700) + [f"17000:{k}" for k in KEYS4]
                 + [f"19100:{k}^" for k in KEYS4])
    s = r["stats"]
    sy.append(("every-effect-top", s["synth_render_max"] / s["budget"], s["fx_active"], s["synth_voices_max"]))
    lines.append("  - SYNTH, MIAMI PAD x 4 voices, synth render / block: " +
                 ", ".join(f"{n} {v * 100:.0f} % (fx 0x{fx:02x}, {nv} voices)" for n, v, fx, nv in sy))
    worst_synth = max(v for _, v, _, _ in sy)
    lines.append(f"  - sum, the busiest song's EAS ({busiest[0]}) + 4 voices with every effect: "
                 f"{(busiest[1] + worst_synth) * 100:.0f} % of a block (limit 60 %)")
    check(all(nv == 4 for _, _, _, nv in sy), "4 synth voices in every SYNTH run")
    rep += lines
    st["numbers"] = dict(needs=needs, learn=res, synth=sy)
    return st["ok"]


# --------------------------------------------------------------------------------------------- M8's own
V_LIST, V_PLAY, V_HELP, V_SETTINGS, V_MODE, V_SYNTH, V_SETUP = 0, 1, 2, 3, 6, 7, 9
PV_SCALE_KB, PV_SCALE_FB = 11, 12
M8_FIELDS = ("bt_n bt_prog bt_style bt_structure bt_key bt_bpm bt_sound bt_metre scale_mask tone_mask next_mask "
             "scale_kind bt_energy bt_style_now mode_sel list_n bt_loop_from bt_loop_to").split()
_stats7 = m5.stats


def stats8(ram):
    """m7_test's stats, plus M8's 16 words after M7's (player.h)"""
    import struct
    s = _stats7(ram)
    if s is None:
        return s
    i = ram.find(b"PLST")
    while i >= 0 and struct.unpack_from("<I", ram, i + 4)[0] not in (3, 4):
        i = ram.find(b"PLST", i + 1)
    v = struct.unpack_from(f"<{len(M8_FIELDS)}I", ram, i + m5.STATS.size + m5.STATS4.size + 128)
    s.update(zip(M8_FIELDS, v))
    return s


m5.stats = stats8
BT = ["4050:e0:1", "4200:PLAY", "4260:PLAY^"]       # the mode screen: SELECT to BACKTRACK, PLAY
BTS = sorted(p for p in (ROOT / "songs/backtracks").iterdir() if (p / "song.yaml").is_file())


def bt_flash():
    """the songs and the 30 backtracks (sent over USB, in SONG LEARN) -> its name in m5.FLASH"""
    if "bt" not in m5.FLASH:
        r = m5.run("m8-send-backtracks", 300000, [], flash="songs",
                   host=f"{m5.SEND} send --emu-pipe " + " ".join(str(p) for p in BTS))
        m5.FLASH["bt"] = r["flash"]
    return "bt"


def btrun(name, ms, sc):
    return m5.run(name, ms, BT + sc, mode=None, flash=bt_flash())


def compiled_bt(number):
    return m5.read_compiled(m5.compile_folder(BTS[number - 1])[0])


def bar_tick(c, bar, bpb=4):
    return bar * bpb * 480


def test_m8p1(all_songs, rep):
    lines, st, check = m5.checker("M8 part 1: renames (D050)")
    sp = m5.run("m8p1-splash", 2500, [], mode=None)
    check(sp["stats"]["ui_view"] >= 16, "the splash: Notami - SongSidekick, m8 (m8p1-splash.png)")
    a = m5.run("m8p1-mode", 4700, [], mode=None)
    check(a["stats"]["ui_view"] == V_MODE and a["stats"]["mode"] == 0 and a["stats"]["mode_sel"] == 0,
          "after the splash: the mode screen, the cursor on SONG LEARN (m8p1-mode.png)")
    res = {}
    for k, (name, view, mode) in enumerate((("SONG LEARN", V_LIST, 1), ("BACKTRACK", V_SETUP, 3), ("SYNTH", V_SYNTH, 2))):
        r = m5.run(f"m8p1-choose-{k}", 5200, ([f"4100:e0:{k}"] if k else []) + ["4400:PLAY", "4460:PLAY^"], mode=None,
                   flash=bt_flash())
        res[name] = r["stats"]["ui_view"] == view and r["stats"]["mode"] == mode
    check(all(res.values()), f"the three choices, in order: SONG LEARN -> the song list, BACKTRACK -> its SETUP, SYNTH "
                             f"-> SYNTH {res}")
    clamp = m5.run("m8p1-mode-clamp", 4700, ["4100:e0:9"], mode=None)
    check(clamp["stats"]["mode_sel"] == 2, "SELECT stops at SYNTH (the last)")
    back = {}
    for k, name in enumerate(("SONG LEARN", "BACKTRACK", "SYNTH")):
        r = m5.run(f"m8p1-home-{k}", 8200, ([f"4100:e0:{k}"] if k else []) + ["4400:PLAY", "4460:PLAY^"] +
                   m6.hold_home(5000), mode=None, flash=bt_flash())
        back[name] = r["stats"]["ui_view"] == V_MODE and r["stats"]["mode_sel"] == 0 and r["stats"]["mode_switches"] == 1
    check(all(back.values()), f"HOME held 2 s from each mode: the same 3-choice screen, the cursor on SONG LEARN {back}")
    sw = m5.run("m8p1-switch", 7000, m5.press_raw(4500, "GLO") + m5.press_raw(4700, "GLO") + ["4900:e0:10"] +
                m5.press_raw(5300, "PLAY"))
    check(sw["stats"]["ui_view"] == V_MODE and sw["stats"]["mode_switches"] == 1,
          "Settings > Switch mode: the mode screen (m8p1-switch.png)")
    sy = m7.syrun("m8p1-synth-start", 5200, [])
    check(sy["stats"]["synth_preset"] == 0 and sy["stats"]["synth_engine"] == 1 and PRESETS[:3] == ["E-PIANO",
          "E-GUITAR", "E-SITAR"], f"SYNTH starts on preset 1, E-PIANO (PHASE); presets 1-3: {PRESETS[:3]}")
    titles = {c["slug"]: c["title"] for c in all_songs}
    check(titles.get("twelve-bar-blues") == "12-Bar Blues" and titles.get("four-chords") == "Four Chord Tune",
          f"the titles: {titles.get('twelve-bar-blues')!r}, {titles.get('four-chords')!r} (slugs unchanged)")
    ls = m5.run("m8p1-learn-list", 5400, [], flash=bt_flash())
    check(ls["stats"]["list_n"] == len(all_songs) and ls["stats"]["ui_view"] == V_LIST,
          f"SONG LEARN with the 30 backtracks stored: All songs lists {ls['stats']['list_n']} (the {len(all_songs)} "
          f"songs, no backtrack; m8p1-learn-list.png)")
    rep += lines
    return st["ok"]


def test_m8p4(all_songs, rep):
    lines, st, check = m5.checker("M8 part 4: BACKTRACK, setup and play")
    s0 = btrun("m8p4-setup", 5000, [])
    s = s0["stats"]
    check(s["ui_view"] == V_SETUP and s["mode"] == 3 and s["bt_n"] == len(BTS) and s["bt_prog"] == 1 and s["bt_style"] == 0
          and s["bt_structure"] == 0 and s["bt_key"] == 0 and s["bt_bpm"] == 100,
          f"SETUP at entry: {len(BTS)} backtracks, 1 POP 1, POP, LOOP, C, 100 BPM (m8p4-setup.png) "
          f"[{s['bt_n']}, {s['bt_prog']}, {s['bt_style']}, {s['bt_structure']}, {s['bt_key']}, {s['bt_bpm']}]")
    # (M9, D053: the one knob map: K3 the complexity, K4 the melody sound, PRESETS the key, ALGO the structure; m9p4 checks
    # every row; here the choices the M8 checks go on from)
    t = btrun("m8p4-turn", 5600, ["4500:e2:14", "4700:e3:6", "4900:e1:1", "5100:e6:3"])
    s = t["stats"]
    check(s["bt_prog"] == 15 and s["bt_style"] == 6 and s["bt_structure"] == 1 and s["bt_key"] == 3 and s["bt_bpm"] == 96,
          f"K1 +14: 15 MINOR POP; K2 +6: WALTZ (its tempo 96); ALGO: VERSE/CHORUS; PRESETS +3: the key 3 up "
          f"(m8p4-turn.png) [{s['bt_prog']}, {s['bt_style']}, {s['bt_structure']}, {s['bt_key']}, {s['bt_bpm']}]")
    tp = btrun("m8p4-tempo", 5600, ["4500:e0:7", "4700:e3:1"])
    check(tp["stats"]["bt_bpm"] == 107, f"SELECT: the tempo (+7: 107); a style turned after it keeps it "
                                        f"({tp['stats']['bt_bpm']})")
    kept = btrun("m8p4-kept", 9500, ["4500:e2:2", "4700:e3:1"] + m6.hold_home(5000) + ["8000:e0:1", "8300:PLAY!",
                                                                                    "8360:PLAY^!"])
    check(kept["stats"]["ui_view"] == V_SETUP and kept["stats"]["bt_prog"] == 3 and kept["stats"]["bt_style"] == 1,
          "the choices kept: BACKTRACK left (HOME held) and chosen again: the same progression and style")
    # PLAY: LOOP = the first verse (bars 5-12 of a 37-bar backtrack); VERSE/CHORUS = verse 1 and chorus 1 (5-20)
    lp = btrun("m8p4-loop", 9000, m5.press_raw(4500, "PLAY"))
    vc = btrun("m8p4-vc", 9000, ["4400:e1:1"] + m5.press_raw(4500, "PLAY"))
    fu = btrun("m8p4-full", 9000, ["4400:e1:2"] + m5.press_raw(4500, "PLAY"))
    a, b, f = lp["stats"], vc["stats"], fu["stats"]
    check(a["ui_view"] == V_PLAY and a["starts"] == 1 and m7.level(a, 2) == 2 and
          (m5_loop(a) == (4 * 1920, 12 * 1920)), f"PLAY: plays; LOOP loops the first verse, bars 5-12 "
          f"({m5_loop(a)}; m8p4-loop.png)")
    check(m5_loop(b) == (4 * 1920, 20 * 1920), f"VERSE/CHORUS: verse 1 and chorus 1, bars 5-20 ({m5_loop(b)})")
    check(m5_loop(f) == (0, 0) and f["start_from"] == 0, f"FULL: no loop, from the intro ({m5_loop(f)}, from "
                                                          f"{f['start_from']})")
    snd = m5.rms(lp["left"][int(6.5 * m5.RATE):int(8.5 * m5.RATE)])
    check(snd > 300 and a["notes_on"][0] and a["notes_on"][1] and a["notes_on"][2] and not lp["halt"],
          f"the band plays: drums, bass, harmony (rms {snd:.0f})")
    # LOOP's jump: a run long enough for one round (8 bars at 200 BPM: 9.6 s)
    lj = btrun("m8p4-loop-jump", 19000, ["4400:e0:100"] + m5.press_raw(4600, "PLAY"))
    check(lj["stats"]["loops"] >= 1 and lj["stats"]["loop_from"] == 4 * 1920 and lj["stats"]["loop_to"] == 12 * 1920,
          f"LOOP at 200 BPM: the jump made at bar 13 back to bar 5 (loops {lj['stats']['loops']})")
    # WALTZ: 3/4 (the metre the player and the screen use), a bar a bar
    wz = btrun("m8p4-waltz", 9000, ["4400:e3:6"] + m5.press_raw(4600, "PLAY"))
    check(wz["stats"]["bt_metre"] == 3 and wz["stats"]["bt_style_now"] == 0 and m5_loop(wz["stats"]) ==
          (4 * 1440, 12 * 1440), f"WALTZ: the backtrack in 3/4 (loop bars 5-12 at 1,440 ticks a bar; m8p4-waltz.png)")
    # the key: K4 +3 (Eb): the harmony sounds 3 up; the chords spelled in Eb
    ky = btrun("m8p4-key", 9000, ["4400:e6:3"] + m5.press_raw(4600, "PLAY"))
    kk = ky["stats"]
    check(kk["transpose"] == 3 and kk["note_on_last"][2] % 12 in (3, 7, 10, 2, 5),
          f"PRESETS +3: the key Eb (transposed {kk['transpose']}); the play screen spells its chords in Eb (m8p4-key.png)")
    # the keys: the melody sound (the synth), 4 voices at most
    six = ["C4", "D4", "E4", "F4", "G4", "A4"]
    kz = btrun("m8p4-keys", 9000, m5.press_raw(4500, "PLAY") + [f"7000:{k}" for k in six] +
               [f"8200:{k}^" for k in six])
    check(kz["stats"]["synth_notes"] >= 6 and kz["stats"]["synth_voices_max"] == 4,
          f"the keys play the melody sound: {kz['stats']['synth_notes']} notes, {kz['stats']['synth_voices_max']} "
          f"voices at most (4)")
    # HOME: stop, then SETUP
    hm = btrun("m8p4-home", 8000, m5.press_raw(4500, "PLAY") + m5.press_raw(6500, "HOME") + m5.press_raw(7000, "HOME"))
    check(hm["stats"]["ui_view"] == V_SETUP, "HOME on the play screen: stop, then SETUP")
    # a send in BACKTRACK: refused (the synth has part of the buffer); leaving: SONG LEARN plays a song
    f = m5.OUT / "runs" / "saints-bt.fm1s"
    f.write_bytes(m5.compile_folder(ROOT / "songs/saints")[0])
    snd = m5.run("m8p4-send-refused", 30000, BT, mode=None, flash=bt_flash(),
                 host=f"{sys.executable} -I {ROOT}/tools/m2_host.py synth-send {f} 3")
    check("result 9" in snd["out"] and m5.records(snd["flash"]) == m5.records(m5.FLASH["bt"]),
          "a song sent in BACKTRACK: refused, nothing written (switch off, SONG LEARN, send)")
    g = [c["slug"] for c in all_songs].index("amazing-grace")
    lv = btrun("m8p4-leave", 14000, m5.press_raw(4500, "PLAY") + m6.hold_home(6000) + ["8600:e0:-2", "8800:PLAY!",
               "8860:PLAY^!"] + ([f"9200:e0:{g}"] if g else []) + ["9600:PLAY!", "9660:PLAY^!", "11000:PLAY!",
                                                                    "11060:PLAY^!"])
    v = m5.rms(lv["left"][int(12.5 * m5.RATE):int(13.8 * m5.RATE)])
    check(lv["stats"]["mode"] == 1 and lv["stats"]["ui_view"] == V_PLAY and v > 200 and not lv["halt"],
          f"BACKTRACK left while playing: SONG LEARN opens a song and it plays (rms {v:.0f})")
    rep += lines
    return st["ok"]


def m5_loop(s):
    return s["bt_loop_from"], s["bt_loop_to"]


def test_m8p5(all_songs, rep):
    lines, st, check = m5.checker("M8 part 5: what can I play now (the scale views, the key lights)")
    pc = lambda *n: sum(1 << x for x in n)
    a = btrun("m8p5-kb-c", 7400, m5.press_raw(4500, "PLAY"))     # (100 BPM, bar 5 from 4.6 s: C in its 1st beat)
    s = a["stats"]
    check(s["view"] == PV_SCALE_KB and s["part"] == 3 and s["scale_mask"] == pc(0, 2, 4, 7, 9) and
          s["tone_mask"] == pc(0, 4, 7), f"nothing is your part: SCALE KEYBOARD, C major pentatonic marked, the chord's "
          f"tones C E G bright (m8p5-kb-c.png) [{s['view']}, {s['scale_mask']:#x}, {s['tone_mask']:#x}]")
    b = btrun("m8p5-kb-g", 9800, m5.press_raw(4500, "PLAY"))     # (bar 6: G)
    check(b["stats"]["tone_mask"] == pc(7, 11, 2), f"the next bar: G's tones G B D (m8p5-kb-g.png) "
                                                   f"[{b['stats']['tone_mask']:#x}]")
    keys_of = lambda m, oct=0: sum(1 << k for k in range(27) if m >> ((53 + k) % 12) & 1)
    check(s["lit_keys"] == keys_of(pc(0, 4, 7)) and s["mid_keys"] == keys_of(pc(2, 9)),
          "the FM-1's key lights: C E G bright, D A at the middle level")
    nx = btrun("m8p5-next", 9100, m5.press_raw(4500, "PLAY"))    # (bar 5's last beat: G next)
    check(nx["stats"]["next_mask"] == pc(7, 11, 2) and nx["stats"]["dim_keys"] == keys_of(pc(11, 2)),
          f"in the chord's last beat the next chord's tones (G B D) dim, on the screen and the keys "
          f"[{nx['stats']['next_mask']:#x}] (m8p5-next.png)")
    fb = btrun("m8p5-fretboard", 7400, m5.press_raw(4500, "PLAY") + m5.press_raw(5000, "EDIT"))
    back = btrun("m8p5-views-round", 7400, m5.press_raw(4500, "PLAY") + m5.press_raw(5000, "EDIT") +
                 m5.press_raw(5300, "EDIT"))
    check(fb["stats"]["view"] == PV_SCALE_FB and back["stats"]["view"] == PV_SCALE_KB,
          "EDIT: SCALE FRETBOARD (m8p5-fretboard.png), EDIT again: SCALE KEYBOARD")
    bl = btrun("m8p5-blues", 7800, ["4300:e2:19"] + m5.press_raw(4900, "PLAY"))
    check(bl["stats"]["scale_mask"] == pc(0, 3, 5, 6, 7, 10) and bl["stats"]["bt_prog"] == 20,
          f"20 12-BAR BLUES: the blues scale (C Eb F Gb G Bb) [{bl['stats']['scale_mask']:#x}] (m8p5-blues.png)")
    mi = btrun("m8p5-minor", 7800, ["4300:e2:14", "4350:e6:2"] + m5.press_raw(4900, "PLAY"))
    check(mi["stats"]["scale_mask"] == pc(11, 2, 4, 6, 9) and mi["stats"]["bt_prog"] == 15,
          f"15 MINOR POP in Bm (PRESETS +2): B minor pentatonic B D E F# A [{mi['stats']['scale_mask']:#x}]")
    # the setting: FULL SCALE, CHORD TONES (Settings row 13 from the top: SELECT 12)
    full = btrun("m8p5-full", 9000, m5.press_raw(4400, "GLO") + m5.press_raw(4600, "GLO") + ["4800:e0:12",
                 "5100:e1:1"] + m5.press_raw(5400, "GLO") + m5.press_raw(5800, "PLAY"))
    ct = btrun("m8p5-chordtones", 9000, m5.press_raw(4400, "GLO") + m5.press_raw(4600, "GLO") + ["4800:e0:12",
               "5100:e1:2"] + m5.press_raw(5400, "GLO") + m5.press_raw(5800, "PLAY"))
    check(full["stats"]["scale_mask"] == pc(0, 2, 4, 5, 7, 9, 11) and ct["stats"]["scale_mask"] == 0 and
          ct["stats"]["tone_mask"] != 0, f"Settings > Scale guide: FULL SCALE (C major) "
          f"[{full['stats']['scale_mask']:#x}]; CHORD TONES: only the chord [{ct['stats']['scale_mask']:#x}] "
          f"(m8p5-full.png, m8p5-chordtones.png)")
    uk = btrun("m8p5-uke", 9000, m5.press_raw(4400, "GLO") + m5.press_raw(4600, "GLO") + ["4800:e0:11",
               "5100:e1:1"] + m5.press_raw(5400, "GLO") + m5.press_raw(5800, "PLAY") + m5.press_raw(6300, "EDIT"))
    check(uk["stats"]["uke"] == 1 and uk["stats"]["view"] == PV_SCALE_FB, "the ukulele setting: the fretboard as a "
                                                                          "ukulele (m8p5-uke.png)")
    hv = btrun("m8p5-harmony-yours", 8400, m5.press_raw(4500, "PLAY") + m5.hold(5000, "ENV") +
               m5.hold(6400, "LFO"))
    hh = btrun("m8p5-harmony", 6800, m5.press_raw(4500, "PLAY") + m5.hold(5000, "ENV"))
    check(hh["stats"]["part"] == 2 and hh["stats"]["view"] not in (PV_SCALE_KB, PV_SCALE_FB) and
          hv["stats"]["part"] == 3 and hv["stats"]["view"] == PV_SCALE_KB,
          "the harmony held: its views (as SONG LEARN; m8p5-harmony.png); LFO held: the melody's scale views again")
    rep += lines
    return st["ok"]


def test_m8p6(all_songs, rep):
    lines, st, check = m5.checker("M8 part 6: the melody sound (D049)")
    a = btrun("m8p6-sound", 7400, m5.press_raw(4500, "PLAY") + ["5500:e5:3"])
    check(a["stats"]["bt_sound"] == 3 and a["stats"]["synth_preset"] == 3,
          f"K4 on the play screen: the melody sound, preset 4 {PRESETS[3]} (its name shown; m8p6-sound.png)")
    w = btrun("m8p6-wrap", 7400, m5.press_raw(4500, "PLAY") + ["5500:e5:-1"])
    check(w["stats"]["bt_sound"] == 29, f"K4 back from preset 1: preset 30 ({PRESETS[29]})")
    k = btrun("m8p6-kept", 13000, m5.press_raw(4500, "PLAY") + ["5500:e5:5"] + m5.press_raw(6000, "PLAY") +
              m5.press_raw(6300, "HOME") + m5.press_raw(6800, "HOME") + m6.hold_home(7300) + ["9800:e0:1", "10000:PLAY!",
              "10060:PLAY^!"] + m5.press_raw(10500, "PLAY") + ["11500:C4", "12000:C4^"])
    check(k["stats"]["bt_sound"] == 5 and k["stats"]["synth_notes"] >= 1,
          "the choice kept: back to SETUP, out of BACKTRACK and in again: still preset 6; the keys play it")
    rep += lines
    return st["ok"]


SAMPLE_PROGS = (1, 9, 20)                          # POP 1 (37 bars), CANON (8-bar progression, 65), 12-BAR BLUES (41)


def test_m8sweep(all_songs, rep):
    """every style x 3 structures x 3 sample progressions: plays to its end (FULL) or a loop round, at 250 BPM; no crash,
    no block over its time (EAS + synth)"""
    lines, st, check = m5.checker("M8: every style x structure x 3 progressions")
    from concurrent.futures import ThreadPoolExecutor
    jobs = [(p, sty, stc) for p in SAMPLE_PROGS for sty in range(11) for stc in range(3)]

    def one(j):
        p, sty, stc = j
        c = compiled_bt(p)
        bpb = 3 if sty == 6 else 4
        bars = c["bar_count"] if stc == 2 else [s["bars"] for s in c["sections"]][1] +             ([s["bars"] for s in c["sections"]][2] if stc == 1 else 0)
        ms = int(4700 + (bars + 1) * bpb * 60000 / 250 + 3000)
        sc = ([f"4300:e2:{p - 1}"] if p > 1 else []) + ([f"4350:e3:{sty}"] if sty else []) +             ([f"4400:e4:{stc}"] if stc else []) + ["4450:e0:150"] + m5.press_raw(4700, "PLAY")
        r = btrun(f"m8sw-{p}-{sty}-{stc}", ms, sc)
        s = r["stats"]
        load = (s["render_max"] + s["synth_render_max"]) / s["budget"] if s["budget"] else 9
        ok = not r["halt"] and load < 1.0 and s["bt_metre"] == bpb and (s["ends"] >= 1 if stc == 2 else s["loops"] >= 1)
        return j, ok, load, s["ends"], s["loops"]
    with ThreadPoolExecutor(3) as ex:
        res = list(ex.map(one, jobs))
    bad = [(j, round(l, 2), e, lp) for j, ok, l, e, lp in res if not ok]
    worst = max(l for _, _, l, _, _ in res)
    check(not bad, f"{len(res)} runs (11 styles x LOOP, VERSE/CHORUS, FULL x backtracks {SAMPLE_PROGS}, 250 BPM): each "
                   f"reached its end or looped, no crash, no block over its time (worst {worst * 100:.0f} %) {bad or ''}")
    rep += lines
    return st["ok"]


def test_m8census(all_songs, rep):
    lines, st, check = m5.checker("M8: the button census (D044) of SETUP and BACKTRACK's play screen; HOME taps")
    names = list(m7.B)
    jobs = {"SETUP": ({"PLAY", "GLO", "OCTDN", "OCTUP"}, []),
            "BACKTRACK play": (set(names), m5.press_raw(4500, "PLAY") + m5.press_raw(4900, "PLAY"))}
    bad = []
    for scr, (has, pre) in jobs.items():
        for b in names:
            if b in ("GLO", "HOME"):
                continue
            t0 = 5600 if pre else 4700
            r = btrun(f"m8c-{scr[:5]}-{b}", t0 + 600, pre + m5.press_raw(t0, b))
            pop = r["stats"]["popup_open"] == 1 and r["stats"]["popup_btn"] == m7.B[b]
            if pop == (b in has):
                bad.append((scr, b, pop))
    hm = btrun("m8c-setup-home", 5500, m5.press_raw(4700, "HOME"))
    check(not bad and hm["stats"]["popup_btn"] == m7.B["HOME"] and hm["stats"]["popup_open"] == 1,
          f"SETUP: PLAY, GLO, OCT-/OCT+ have jobs, every other button (HOME a tap) the pop-up; BACKTRACK's play screen: "
          f"every button a job (LFO: the melody is yours) {bad or ''}")
    taps = []
    for k in range(24):
        t = 4800 + 137 * k
        taps += [f"{t}:HOME!", f"{t + 40 + k % 3}:HOME^!"]
    tp = btrun("m8c-home-taps", 9000, m5.press_raw(4500, "PLAY") + taps)
    check(tp["stats"]["mode_switches"] == 0 and tp["stats"]["ui_view"] in (V_SETUP, V_PLAY),
          "24 quick HOME taps in BACKTRACK: never the mode screen")
    gl = btrun("m8c-help", 5600, m5.press_raw(4600, "GLO"))
    gp = btrun("m8c-help-play", 6600, m5.press_raw(4500, "PLAY") + m5.press_raw(5600, "GLO"))
    check(gl["stats"]["ui_view"] == V_HELP and gp["stats"]["ui_view"] == V_HELP,
          "GLO: the controls of SETUP (m8c-help.png) and of the play screen (m8c-help-play.png)")
    rep += lines
    return st["ok"]


def test_m8cpu(all_songs, rep):
    """the budget: every style, FULL, every layer at its top (drums L3, bass L4, harmony L5), the heaviest preset (as
    made) on 4 voices, at 200 BPM: EAS + synth render of a block"""
    lines, st, check = m5.checker("M8 budget: BACKTRACK's CPU worst case")
    from concurrent.futures import ThreadPoolExecutor
    heavy = PRESETS.index("MIAMI PAD")

    def one(sty):
        sc = [f"4350:e3:{sty}"] if sty else []
        sc += ["4400:e4:2", "4450:e0:100"] + m5.press_raw(4600, "PLAY") + [f"5300:e5:{heavy}"]
        sc += [f"{5500 + 200 * i}:e{enc}:1" for i, enc in enumerate([2, 2, 3, 3, 3, 4, 4, 4, 4])]   # (a level a turn)
        sc += [f"7500:{k}" for k in KEYS4] + [f"16000:{k}^" for k in KEYS4]
        r = btrun(f"m8cpu-{sty}", 16500, sc)
        s = r["stats"]
        return sty, (s["render_max"] + s["synth_render_max"]) / s["budget"], s["render_max"] / s["budget"],             s["synth_voices_max"], s["levels"] & 0xFFF, r["halt"]
    with ThreadPoolExecutor(3) as ex:
        res = list(ex.map(one, range(11)))
    worst = max(res, key=lambda x: x[1])
    lines.append("  - per style (STYLE order: POP ROCK INDIE BALLAD SHUFFLE STRIDE WALTZ COUNTRY BOSSA FUNK REGGAE): " +
                 ", ".join(f"{v * 100:.0f} % ({nv} v, L{lv:03x}{', HALT' if h else ''})" for _, v, _, nv, lv, h in res))
    check(worst[1] <= 0.60 and all(nv == 4 and lv == 0x543 and not h for _, _, _, nv, lv, h in res),
          f"the worst: style {worst[0] + 1}, every layer at its top, {PRESETS[heavy]} x 4 voices: EAS + synth "
          f"{worst[1] * 100:.0f} % of a block (EAS {worst[2] * 100:.0f} %; limit 60 %)")
    rep += lines
    st["worst"] = worst
    return st["ok"]


# (M9: m8census, m8cpu and m8sweep test M8's knob map and levels: superseded by tools/m9_test.py m9census, m9cpu, m9sweep;
# they stay here, on request, for the M8 package)
PARTS = {"m8p0": test_m8p0, "m8p1": test_m8p1, "m8p4": test_m8p4, "m8p5": test_m8p5, "m8p6": test_m8p6,
         "m8census": test_m8census, "m8cpu": test_m8cpu, "m8sweep": test_m8sweep}
M9_SUPERSEDED = {"m8census", "m8cpu", "m8sweep"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    ap.add_argument("--pkg")
    ap.add_argument("--out")
    ap.add_argument("--jobs", type=int, default=3)
    a = ap.parse_args()
    m5.OUT = Path(a.out).resolve() if a.out else ROOT / "build/emu/m8"
    if a.pkg:
        m5.PKG = Path(a.pkg).resolve()
    m5.OUT.mkdir(parents=True, exist_ok=True)
    only = set(a.only.split(",")) if a.only else set(PARTS) - {"m8p0"} - M9_SUPERSEDED   # (m8p0: Part 0's measure, on request)
    all_songs = m5.songs()
    m5.prepare_flash()
    if only - {"m8p0"}:
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
    text = [f"# M8 emulator tests\n\nPackage: {m5.PKG}, sha256 {sha}\n"]
    for k in reports:
        text += reports[k]
    text.append(f"\n**{'ALL PASS' if all(results.values()) else 'FAILURES: ' + ', '.join(k for k, v in results.items() if not v)}**\n")
    (m5.OUT / "report.md").write_text("\n".join(text))
    print("\n".join(text))
    sys.exit(0 if all(results.values()) else 1)


if __name__ == "__main__":
    main()
