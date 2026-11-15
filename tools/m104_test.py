#!/usr/bin/env python3
# m104_test.py -- Milestone M10.4: headless emulator tests (built on tools/m101_test.py's runner and stats; same structure). Short runs only:
# the built-in starter library (a new FM-1, the store wins), and the M10.3 changes that were never run in the emulator (BACKTRACK, SONG LEARN).
# Never touches the FM-1.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   .venv/bin/python -I tools/m104_test.py [--only m104p1,m104p2,..] [--pkg PKG] [--out DIR]   -> DIR/report.md (default build/emu/m104)
import argparse
import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import m5_test as m5                     # noqa: E402
import m8_test as m8                     # noqa: E402
import m10_test as m10                   # noqa: E402
import m101_test as m101                 # noqa: E402

btrun, bt_flash, named = m8.btrun, m8.bt_flash, m10.named
V_LIST, V_PLAY, V_HELP, V_SETTINGS, V_MODE, V_SYNTH, V_SETUP = m8.V_LIST, m8.V_PLAY, m8.V_HELP, m8.V_SETTINGS, \
    m8.V_MODE, m8.V_SYNTH, m8.V_SETUP
P = m5.press_raw
O = m5.OPEN_MS
PLAY_MS = m5.PLAY_MS
RATE = m5.RATE
SHOTS = ROOT / "docs/step-M10.4"

# --------------------------------------------------------------------------------------------- stats
# M10.4 appends five words after M10.1's texts (player.h): the built-ins listed, the last file's source (0 none, 1 store, 2 built-in), the
# route of the last key (0 EAS, 1 vsynth), the last program sent on the keys' channel (0xFF none), the Start pop-up's big section name
M104_FIELDS = ("builtins", "open_src", "key_route", "keys_prog", "big_sub")
_stats101 = m5.stats


def stats104(ram):
    s = _stats101(ram)
    if s is None:
        return s
    i = ram.find(b"PLST")
    while i >= 0 and struct.unpack_from("<I", ram, i + 4)[0] not in (3, 4):
        i = ram.find(b"PLST", i + 1)
    base = i + m5.STATS.size + m5.STATS4.size + 128 + (18 + 14) * 4
    at = base + 4 * len(m10.M10_FIELDS) + sum(n for _, n in m10.M10_STRINGS) + 48 + 64 + 48
    s.update(zip(M104_FIELDS, struct.unpack_from(f"<{len(M104_FIELDS)}I", ram, at)))
    return s


m5.stats = stats104


def cpu(r):
    s = r["stats"]
    return (s["render_max"] + s["synth_render_max"]) / s["budget"]


def play_run(name, i, flash, ms=PLAY_MS + 8000):
    """SONG LEARN: song i of the list opened, PLAY at PLAY_MS, 8 s of it"""
    return m5.run(name, ms, m5.open_song(i) + m5.press(PLAY_MS, "PLAY"), flash=flash)


def run_all(jobs, n=3):
    from concurrent.futures import ThreadPoolExecutor
    with ThreadPoolExecutor(n) as ex:
        futs = {k: ex.submit(f) for k, f in jobs.items()}
        return {k: f.result() for k, f in futs.items()}


def private_copy(tag, slug, title):
    """a user's song for the tests: a copy of saints under another slug and title (the real private songs are never read by a test)"""
    d = m5.OUT / tag / slug
    shutil.rmtree(d.parent, ignore_errors=True)
    shutil.copytree(ROOT / "songs/saints", d, ignore=shutil.ignore_patterns("previews"))
    y = d / "song.yaml"
    y.write_text(y.read_text().replace("slug: saints", f"slug: {slug}").replace("title: When the Saints Go Marching In", f"title: {title}"))
    return d


# --------------------------------------------------------------------------------------------- part 1
def test_m104p1(all_songs, rep):
    lines, st, check = m5.checker("M10.4 part 1: a new FM-1 (empty store): the built-in library (D070 option 1)")
    slugs = [c["slug"] for c in all_songs]
    plays = ("greensleeves", "auld-lang-syne", "amazing-grace")       # the largest file; a pickup; a 3/4 song
    jobs = {
        "boot": lambda: m5.run("m104p1-boot", 5400, [], flash="empty_quiet"),
        "bt": lambda: m5.run("m104p1-bt", 5400, m8.BT, mode=None, flash="empty_quiet"),
        "first": lambda: m5.run("m104p1-first", 5200, [], flash="empty_quiet"),
        "end": lambda: m5.run("m104p1-end", 5200, ["4150:e0:14"], flash="empty_quiet"),
    }
    for slug in plays:
        i = slugs.index(slug)
        jobs[f"b-{slug}"] = (lambda i, slug: lambda: play_run(f"m104p1-b-{slug}", i, "empty_quiet"))(i, slug)
        jobs[f"s-{slug}"] = (lambda i, slug: lambda: play_run(f"m104p1-s-{slug}", i, "songs"))(i, slug)
    # two backtracks, the progression (K1) and the style (K2) changed while playing: the playing band and the next one use both file slots
    sc = ["4450:e0:150"] + P(4700, "PLAY") + ["7000:e2:1"] + ["10500:e2:1", "10600:e3:1"]
    jobs["bt-b"] = lambda: m5.run("m104p1-bt-b", 19000, m8.BT + sc, mode=None, flash="empty_quiet")
    jobs["bt-s"] = lambda: m5.run("m104p1-bt-s", 19000, m8.BT + sc, mode=None, flash=bt_flash())
    R = run_all(jobs)
    S = {k: r["stats"] for k, r in R.items()}
    b, bt = S["boot"], S["bt"]
    check(b["ui_view"] == V_LIST and b["list_n"] == len(all_songs) == 15 and b["builtins"] == 57 and not R["boot"]["halt"],
          f"a new FM-1 (an empty store) boots into SONG LEARN and lists {b['list_n']} songs ({b['builtins']} built-ins in all: the songs and the backtracks) (m104p1-boot.png)")
    check(bt["ui_view"] == V_SETUP and bt["bt_n"] == 42 and bt["builtins"] == 57 and not R["bt"]["halt"],
          f"BACKTRACK on it finds {bt['bt_n']} backtracks (the library: {bt['builtins']} built-ins in all) (m104p1-bt.png)")
    SHOTS.mkdir(parents=True, exist_ok=True)
    shutil.copy(m5.OUT / "m104p1-end.png", SHOTS / "list-footer.png")
    # the footer: with the cursor on the last song the list scrolls so that the footer shows under it
    first = R["first"]["lcd"].convert("RGB").resize((240, 240))
    end = R["end"]["lcd"].convert("RGB").resize((240, 240))
    bar = first.getpixel((5, 50))                      # the cursor's bar colour (row 0 of the list is selected at first)
    black = (0, 0, 0)

    def ink(img, y0, y1):
        return sum(1 for x in range(10, 230) for y in range(y0, y1) if img.getpixel((x, y)) != black)
    check(bar != black and first.getpixel((5, 194)) != bar and end.getpixel((5, 158)) == bar and end.getpixel((5, 194)) == black and
          ink(end, 180, 208) > 60 and S["end"]["list_n"] == 15,
          f"the list's footer: the cursor on the last song (the bar at y 158) and the footer's two lines under it on a row with no bar "
          f"({ink(end, 180, 208)} text pixels at y 180-208) (m104p1-end.png)")
    for slug in plays:
        rb, rs = R[f"b-{slug}"], R[f"s-{slug}"]
        sb, ss = rb["stats"], rs["stats"]
        snd = m5.rms(rb["left"][int((PLAY_MS + 1500) / 1000 * RATE):])
        same = rb["left"] == rs["left"]
        diff = (sum(abs(a - c) for a, c in zip(rb["left"], rs["left"])) / max(1, len(rb["left"])))
        check(sb["starts"] == 1 and sb["open_src"] == 2 and ss["open_src"] == 1 and not rb["halt"] and snd > 50 and
              cpu(rb) <= 0.60 and cpu(rb) <= cpu(rs) + 0.02 and (same or diff < 1.0),
              f"{slug}: opened from the built-ins (source {sb['open_src']}; the store's copy: {ss['open_src']}) and played 8 s: sound rms {snd:.0f}, "
              f"no halt, CPU {cpu(rb) * 100:.0f} % (store copy {cpu(rs) * 100:.0f} %); the audio is {'sample-identical to' if same else f'within {diff:.2f} of'} "
              f"the store copy's")
    bb, bs = S["bt-b"], S["bt-s"]
    sndb = m5.rms(R["bt-b"]["left"][int(14000 / 1000 * RATE):])
    same = R["bt-b"]["left"] == R["bt-s"]["left"]
    check(not R["bt-b"]["halt"] and bb["open_src"] == 2 and bs["open_src"] == 1 and bb["bt_prog"] == 3 and bb["swaps"] >= 2 and
          bb["bt_prepares"] >= 3 and bb["swaps"] == bs["swaps"] and bb["bt_prepares"] == bs["bt_prepares"] and sndb > 50 and cpu(R["bt-b"]) <= 0.60,
          f"two backtracks (progression 1 -> 2 -> 3, then the style) changed while playing: {bb['bt_prepares']} versions built, {bb['swaps']} swaps "
          f"({bs['bt_prepares']} / {bs['swaps']} from the store), no halt, sound after the changes rms {sndb:.0f}, CPU {cpu(R['bt-b']) * 100:.0f} %; "
          f"the audio is {'sample-identical to' if same else 'not sample-identical to'} the store copy's")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 2
def test_m104p2(all_songs, rep):
    lines, st, check = m5.checker("M10.4 part 2: the store wins (oh-susanna sent, then deleted)")
    slugs = [c["slug"] for c in all_songs]
    d = private_copy("zzz-private", "zzz-private", "Zzz Private Test")
    send = m5.run("m104p2-send", 120000, [], flash="empty_quiet", host=f"{m5.SEND} send --emu-pipe {ROOT}/songs/oh-susanna {d}")
    f1 = send["flash"]
    io, isa = slugs.index("oh-susanna"), slugs.index("saints")
    jobs = {
        "boot": lambda: m5.run("m104p2-boot", 5400, [], flash=f1),
        "open-store": lambda: play_run("m104p2-open-store", io, f1, PLAY_MS + 3000),
        "open-built": lambda: play_run("m104p2-open-built", isa, f1, PLAY_MS + 3000),
        "list": lambda: m5.run("m104p2-list", 30000, [], flash=f1, host=f"{m5.SEND} list --emu-pipe"),
        "del": lambda: m5.run("m104p2-delete", 30000, [], flash=f1, host=f"{m5.SEND} delete --emu-pipe oh-susanna"),
        "del-built": lambda: m5.run("m104p2-delete-built", 30000, [], flash=f1, host=f"{m5.SEND} delete --emu-pipe saints"),
    }
    R = run_all(jobs)
    S = {k: r["stats"] for k, r in R.items()}
    after = R["del"]["flash"]
    R2 = run_all({"boot": lambda: m5.run("m104p2-boot-after", 5400, [], flash=after),
                  "open": lambda: play_run("m104p2-open-after", io, after, PLAY_MS + 3000),
                  "list": lambda: m5.run("m104p2-list-after", 30000, [], flash=after, host=f"{m5.SEND} list --emu-pipe")})
    S2 = {k: r["stats"] for k, r in R2.items()}
    check("oh-susanna: stored" in send["out"] and "zzz-private: stored" in send["out"] and not send["halt"],
          "oh-susanna and a user's song (a copy of saints under another slug and title) sent to a new FM-1")
    check(S["boot"]["list_n"] == 16 and S["boot"]["builtins"] == 56 and S["boot"]["ui_view"] == V_LIST,
          f"the list has {S['boot']['list_n']} songs (14 built-in songs + oh-susanna + the user's one): {S['boot']['builtins']} of the 57 built-ins listed (oh-susanna's is replaced), no duplicate")
    check(S["open-store"]["open_src"] == 1 and S["open-store"]["starts"] == 1 and S["open-built"]["open_src"] == 2 and S["open-built"]["starts"] == 1,
          f"oh-susanna opens from the store (source {S['open-store']['open_src']}), saints from the built-ins (source {S['open-built']['open_src']}); both play")
    lo = R["list"]["out"]
    check("oh-susanna" in lo and "zzz-private" in lo and "saints" not in lo and "2 song(s)" in lo and R["list"]["ops"] == 0,
          f"fm1song list shows only the store: the two songs sent, '2 song(s)', none of the 14 built-ins (nothing written)")
    check("oh-susanna: deleted" in R["del"]["out"] and S2["boot"]["list_n"] == 16 and S2["boot"]["builtins"] == 57 and S2["open"]["open_src"] == 2 and
          S2["open"]["starts"] == 1,
          f"after fm1song delete oh-susanna: the list still has {S2['boot']['list_n']} songs, {S2['boot']['builtins']} built-in, and oh-susanna opens "
          f"from the built-ins (source {S2['open']['open_src']})")
    lo2 = R2["list"]["out"]
    check("zzz-private" in lo2 and "oh-susanna" not in lo2 and "1 song(s)" in lo2,
          "fm1song list now: only the user's song ('1 song(s)')")
    check("NOT deleted" in R["del-built"]["out"] and R["del-built"]["ops"] == 0 and S["del-built"]["builtins"] == 56,
          f"deleting a song that is only built in (saints): 'NOT deleted', nothing written ({R['del-built']['ops']} flash operations): "
          f"{[l for l in R['del-built']['out'].splitlines() if 'saints' in l][:1]}")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 3
def test_m104p3(all_songs, rep):
    lines, st, check = m5.checker("M10.4 part 3: BACKTRACK (M10.3): HOME, SEQ while playing, the section indicator")
    jobs = {
        "home-play": lambda: named("m104p3-home-play", P(6500, "HOME"), 7400),
        "home-paused": lambda: named("m104p3-home-paused", P(6300, "PLAY") + P(7000, "HOME"), 7800),
        "home-twice": lambda: named("m104p3-home-twice", P(6300, "HOME") + P(7000, "HOME"), 7800),
        "seq1": lambda: named("m104p3-seq1", P(6500, "SEQ"), 7200),
        "seq2": lambda: named("m104p3-seq2", P(6500, "SEQ") + P(6900, "SEQ"), 7700),
        "seq3": lambda: named("m104p3-seq3", P(6500, "SEQ") + P(6900, "SEQ") + P(7300, "SEQ"), 8300),
        "algo0": lambda: named("m104p3-algo0", [], 6800),
        "algo": lambda: named("m104p3-algo", ["6500:e1:1"], 6800),
    }
    R = run_all(jobs)
    S = {k: r["stats"] for k, r in R.items()}
    check(S["home-play"]["ui_view"] == V_SETUP and S["home-play"]["starts"] == 1 and not R["home-play"]["halt"],
          f"HOME while playing: one tap goes straight to SETUP (view {S['home-play']['ui_view']}; it was playing: starts {S['home-play']['starts']})")
    check(S["home-paused"]["ui_view"] == V_SETUP and S["home-paused"]["pauses"] == 1 and not R["home-paused"]["halt"],
          f"HOME while paused: one tap goes straight to SETUP (view {S['home-paused']['ui_view']}, pauses {S['home-paused']['pauses']})")
    check(S["home-twice"]["ui_view"] == V_MODE and S["home-twice"]["mode_switches"] == 1,
          f"HOME twice (playing -> SETUP, then SETUP -> the mode screen, the M10.1 climb): view {S['home-twice']['ui_view']}")
    s1, s2, s3 = S["seq1"], S["seq2"], S["seq3"]
    check(s1["kit"] == 1 and (s1["kit_bass_prog"], s1["kit_harm_prog"]) == (32, 24) and s1["vp_title"] == "Band Kit" and s1["vp_value"] == "ACOUSTIC" and
          s1["ui_view"] == V_PLAY and not R["seq1"]["halt"],
          f"SEQ while playing: kit {s1['kit']} (ACOUSTIC), bass program {s1['kit_bass_prog']}, harmony program {s1['kit_harm_prog']}, the pop-up "
          f"{s1['vp_title']!r} {s1['vp_value']!r}, still playing (view {s1['ui_view']}) (m104p3-seq1.png)")
    check(s2["kit"] == 2 and (s2["kit_bass_prog"], s2["kit_harm_prog"]) == (38, 89) and s3["kit"] == 3 and (s3["kit_bass_prog"], s3["kit_harm_prog"]) == (38, 4) and
          s3["vp_value"] == "CHIP" and s3["kit_hits"] > 0 and not R["seq2"]["halt"] and not R["seq3"]["halt"],
          f"SEQ again: ELECTRO ({s2['kit_bass_prog']}, {s2['kit_harm_prog']}), CHIP ({s3['kit_bass_prog']}, {s3['kit_harm_prog']}); no halt, the drums hit "
          f"({s3['kit_hits']})")
    check(S["algo0"]["ind_name"] == "LOOP" and S["algo"]["ind_name"] != "LOOP",
          f"the section indicator: LOOP before ({S['algo0']['ind_name']!r}); after ALGO goes from LOOP to VERSE/CHORUS it says {S['algo']['ind_name']!r} "
          f"in the next frame (the structure applied at the next phrase: {S['algo']['bt_structure']}) (m104p3-algo.png)")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 4
def test_m104p4(all_songs, rep):
    lines, st, check = m5.checker("M10.4 part 4: SONG LEARN (M10.3): the keys follow the melody sound; the Start pop-up's section name")
    slugs = [c["slug"] for c in all_songs]
    i = slugs.index("saints")
    saints = next(c for c in all_songs if c["slug"] == "saints")
    prog = saints["layers"]["melody"]["program"]
    key = [f"{O + 1500}:C4", f"{O + 1900}:C4^"]

    def run(name, extra, ms=O + 2500):
        return m5.run(name, ms, m5.open_song(i) + extra)
    jobs = {
        "song": lambda: run("m104p4-song", key),
        "preset": lambda: run("m104p4-preset", [f"{O + 500}:e5:1"] + key),
        "back": lambda: run("m104p4-back", [f"{O + 500}:e5:1", f"{O + 900}:e5:-1"] + key),
        "none": lambda: run("m104p4-none", []),
        "alg1": lambda: run("m104p4-alg1", [f"{O + 500}:e1:16"], O + 1200),
        "alg2": lambda: run("m104p4-alg2", [f"{O + 500}:e1:15"], O + 1200),
        "alg0": lambda: run("m104p4-alg0", [], O + 1200),
    }
    R = run_all(jobs)
    S = {k: r["stats"] for k, r in R.items()}
    check(S["none"]["keys_prog"] == prog and S["none"]["ui_view"] == V_PLAY,
          f"opening a song sets the keys' program on channel 16 to the song's melody program: {S['none']['keys_prog']} (saints' melody: {prog})")
    check(S["song"]["key_route"] == 0 and S["song"]["keys_prog"] == prog and S["song"]["key_note_last"] != 0xFF and S["song"]["mel_sound"] == 0,
          f"with SONG: a key goes to EAS (key_route {S['song']['key_route']}), the keys' program is the melody's ({S['song']['keys_prog']}), "
          f"note {S['song']['key_note_last']}")
    check(S["preset"]["key_route"] == 1 and S["preset"]["mel_sound"] == 1 and S["preset"]["synth_notes"] >= 1 and not R["preset"]["halt"],
          f"with a preset as the melody sound (K4: {S['preset']['vp_value']!r}): a key goes to vsynth (key_route {S['preset']['key_route']}, "
          f"{S['preset']['synth_notes']} synth voices started)")
    check(S["back"]["mel_sound"] == 0 and S["back"]["key_route"] == 0 and S["back"]["keys_prog"] == prog,
          f"K4 back to SONG: the keys are the song's melody again (route {S['back']['key_route']}, program {S['back']['keys_prog']})")
    a1, a2 = S["alg1"], S["alg2"]
    check(a1["vp_title"] == "Start" and a1["vp_sub"] == saints["sections"][1]["name"] and a1["big_sub"] == 1 and a2["big_sub"] == 0 and
          a2["vp_sub"] == "" and S["alg0"]["big_sub"] == 0,
          f"the Start pop-up on a section's first bar: sub line {a1['vp_sub']!r}, big_sub {a1['big_sub']} (value {a1['vp_value']!r}); on another bar: "
          f"{a2['vp_sub']!r}, big_sub {a2['big_sub']} (value {a2['vp_value']!r}) (m104p4-alg1.png, m104p4-alg2.png)")
    rep += lines
    return st["ok"]


# --------------------------------------------------------------------------------------------- part 5
def test_m104p5(all_songs, rep):
    lines, st, check = m5.checker("M10.4 part 5: screenshots (docs/step-M10.4/): the splash, the mode screen, BACKTRACK's help")
    def crashed(r):          # fm1-base's crash screen: a dark red page (the stats still say the view the UI was in)
        px = r["lcd"].convert("RGB").resize((240, 240)).getpixel((120, 200))
        return px[0] > 120 and px[1] < 40 and px[2] < 40
    R = run_all({"splash": lambda: m5.run("m104p5-splash", 2500, [], mode=None),
                 "h-list": lambda: m5.run("m104p5-h-list", 5400, P(4600, "GLO")),
                 "h-play": lambda: m5.run("m104p5-h-play", O + 1500, m5.open_song(0) + P(O + 500, "GLO")),
                 "h-setup": lambda: btrun("m104p5-h-setup", 5800, P(4600, "GLO")),
                 "s-list": lambda: m5.run("m104p5-s-list", 6000, P(4600, "GLO") + P(5000, "GLO")),
                 "mode": lambda: m5.run("m104p5-mode", 4700, [], mode=None),
                 "help": lambda: btrun("m104p5-bt-help", 7000, P(4700, "PLAY") + P(6000, "GLO"))})
    S = {k: r["stats"] for k, r in R.items()}
    SHOTS.mkdir(parents=True, exist_ok=True)
    for src, dst in (("m104p5-splash", "splash"), ("m104p5-mode", "mode-screen"), ("m104p5-bt-help", "bt-help")):
        f = m5.OUT / f"{src}.png"
        if f.exists():
            shutil.copy(f, SHOTS / f"{dst}.png")
    check(S["splash"]["ui_view"] >= 16 and S["mode"]["ui_view"] == V_MODE and S["help"]["ui_view"] == V_HELP and not any(r["halt"] for r in R.values()),
          f"the splash (view {S['splash']['ui_view']}), the mode screen ({S['mode']['ui_view']}) and BACKTRACK's play help ({S['help']['ui_view']}), no halt; "
          f"the pictures are in docs/step-M10.4/ (splash.png, mode-screen.png, bt-help.png; list-footer.png from part 1)")
    check(not any(crashed(R[k]) or R[k]["halt"] for k in R) and S["h-list"]["ui_view"] == V_HELP and S["h-play"]["ui_view"] == V_HELP and
          S["h-setup"]["ui_view"] == V_HELP and S["s-list"]["ui_view"] == V_SETTINGS,
          "the help of the song list, of a song's play screen, of BACKTRACK's SETUP and of its play screen, and the Settings screen: no crash screen "
          "(M10.3's help tables had nine rows where ten were drawn: a NULL string crashed every help but BACKTRACK's play help; found by this run) "
          "(m104p5-h-list.png, m104p5-h-play.png, m104p5-h-setup.png)")
    rep += lines
    return st["ok"]


PARTS = {"m104p1": test_m104p1, "m104p2": test_m104p2, "m104p3": test_m104p3, "m104p4": test_m104p4, "m104p5": test_m104p5}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only")
    ap.add_argument("--pkg")
    ap.add_argument("--out")
    ap.add_argument("--jobs", type=int, default=3)
    a = ap.parse_args()
    m5.OUT = Path(a.out).resolve() if a.out else ROOT / "build/emu/m104"
    if a.pkg:
        m5.PKG = Path(a.pkg).resolve()
    m5.OUT.mkdir(parents=True, exist_ok=True)
    only = set(a.only.split(",")) if a.only else set(PARTS)
    all_songs = m5.songs()
    m5.prepare_flash()
    if only & {"m104p3", "m104p5", "m104p1"}:
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
    text = [f"# M10.4 emulator tests\n\nPackage: {m5.PKG}, sha256 {sha}\n"]
    for k in reports:
        text += reports[k]
    text.append(f"\n**{'ALL PASS' if all(results.values()) else 'FAILURES: ' + ', '.join(k for k, v in results.items() if not v)}**\n")
    (m5.OUT / "report.md").write_text("\n".join(text))
    print("\n".join(text))
    sys.exit(0 if all(results.values()) else 1)


if __name__ == "__main__":
    main()
