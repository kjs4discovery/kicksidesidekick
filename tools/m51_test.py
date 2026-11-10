#!/usr/bin/env python3
# m51_test.py -- M5.1 (the song-send crash): the song store on the TIMED emulator. Laptop and emulator only: never
# touches the FM-1. Licence: TBD (see D017)
#
#   .venv/bin/python -I tools/m51_test.py [--pkg PKG] [--jobs 3]     -> build/emu/m51/report.md
#
# Every run here has FM1_FLASH_TIMED=1 (tools/patches/fm1-emu-flash-timing.patch): erases 8 ms, page programs 2 ms,
# data-area XIP reads cost their cache misses, and a read of the XIP window while a flash command has it off stalls
# the core until the watchdog resets it, as on the chip (the 2026-11-06 incident). Each run reports the watchdog's
# longest unfed stretch; every check below also asks for no watchdog reset and no such read.
#
# Scenarios: the incident's store (M4's 9 songs + 2 private, 68 of 69 sectors), an empty store, a full and
# fragmented one (no erased block left: live songs, stale copies and leftovers in every sector); all 14 songs in one
# command and one by one; power cuts during the sends (making room included) and a boot after each; junk in the
# store; WIPE SONG STORE from the laptop, the settings and the power-on combination.
import argparse
import os
import random
import re
import struct
import sys
import zlib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import m5_test as m5                     # noqa: E402  (its emulator runner, flash parsing, presses)
from fm1song import compile_folder       # noqa: E402

os.environ["FM1_FLASH_TIMED"] = "1"
GAP_LIMIT_MS = 1000                      # the plan: no stretch over ~1 s without feeding the watchdog (it is 8 s)
V_LIST, V_MODE, V_WIPE = 0, 6, 8         # ui.c's views (player_stats.ui_view)
LO, HI = m5.AREA
SEND = m5.SEND
OURS = sorted(p for p in (ROOT / "songs").iterdir() if (p / "song.yaml").is_file())
PRIV = sorted(p for p in (ROOT / "songs/private").glob("*/") if (p / "song.yaml").is_file()) \
    if (ROOT / "songs/private").is_dir() else []
ALL = OURS + PRIV
RUNS = []                                # every run's (name, watchdog resets, xip-off reads, longest late gap ms)


def timed(name, ms, script, **kw):
    r = m5.run(name, ms, script, **kw)
    out = r["out"]
    g = re.search(r"longest gap after the first second (\d+) ms", out)
    w = re.search(r"(\d+) watchdog resets", out)
    x = re.search(r"xip off: (\d+) reads", out)
    r["gap"] = int(g.group(1)) if g else 0
    r["wdt"] = int(w.group(1)) if w else 0
    r["xip"] = int(x.group(1)) if x else 0
    r["stored"] = out.count(": stored (")
    r["busy"] = out.count("is making room")
    RUNS.append((name, r["wdt"], r["xip"], r["gap"]))
    return r


def healthy(r):
    return r["wdt"] == 0 and r["xip"] == 0 and r["gap"] < GAP_LIMIT_MS


def send(paths):
    return f"{SEND} send --emu-pipe " + " ".join(str(p) for p in paths)


def listed(flash, name):
    r = timed(name, 30000, [], flash=str(flash), host=f"{SEND} list --emu-pipe")
    m = re.search(r"(\d+) song\(s\); (\d+) of (\d+) blocks free", r["out"])
    bad = len(re.findall(r"  (DAMAGED|UNREADABLE)", r["out"]))
    return (int(m.group(1)) if m else -1), (int(m.group(2)) if m else -1), bad, r


def boots(flash, name):
    """the FM-1 started on this flash: the mode screen, LEARN, the song list (and nothing reset)"""
    r = timed(name, 6500, [], flash=str(flash))
    s = r["stats"]
    return r, bool(s) and s["ui_view"] == V_LIST and s["mode"] == 1 and r["wdt"] == 0 and not r["halt"]


def area(path):
    return Path(path).read_bytes()[LO:HI]


def head(seq, blob, magic=b"FM2R"):
    h = bytearray(b"\xff" * 32)
    h[0:4] = magic
    struct.pack_into("<III", h, 4, seq, len(blob), zlib.crc32(blob))
    struct.pack_into("<I", h, 28, zlib.crc32(bytes(h[4:28])))
    return bytes(h)


def blocks(n):
    b = (32 + n + 511) // 512
    return b if b <= 8 else (b + 7) // 8 * 8


def make_frag(empty, out):
    """a full, fragmented store: no erased block left. Sectors 0..8: one of our songs (an OLDER copy: the send
    replaces it) and leftovers; then the private songs (older copies) on their sectors, their tails leftovers; every
    other sector: a stale copy of one of ours (a lower sequence number than its live copy) and leftovers"""
    img = bytearray(Path(empty).read_bytes())
    blobs = [compile_folder(f)[0] for f in OURS]
    pblobs = [compile_folder(f)[0] for f in PRIV]
    img[LO:HI] = b"\x00" * (HI - LO)               # leftovers everywhere (not erased, no head)
    sec, seq = 0, 1000
    for b in blobs:                                 # the live (older) copies
        o = LO + sec * 4096
        img[o:o + 32 + len(b)] = head(seq, b) + b
        sec, seq = sec + 1, seq + 1
    for b in pblobs:
        o = LO + sec * 4096
        img[o:o + 32 + len(b)] = head(seq, b) + b
        sec, seq = sec + blocks(len(b)) // 8, seq + 1
    k = 0
    while sec < (HI - LO) // 4096:                  # stale copies: they only take room until tidied
        b = blobs[k % len(blobs)]
        o = LO + sec * 4096
        img[o:o + 32 + len(b)] = head(k, b) + b
        sec, k = sec + 1, k + 1
    Path(out).write_bytes(img)
    return out


def make_m4full(empty, out):
    """the incident's store: M4's 9 songs (1.3, FM1R) and 2 of the private ones as M4 had them: 68 of 69 sectors"""
    img = bytearray(Path(empty).read_bytes())
    songs = Path(m5.M4_FLASH).read_bytes()
    priv = (ROOT / "build/step-M4/final/emu/flash/private.bin").read_bytes()
    img[LO:HI] = songs[LO:HI]
    img[0xC4000:0xC4000 + 11 * 4096] = priv[618496:618496 + 11 * 4096]
    img[0xCF000:0xCF000 + 12 * 4096] = priv[704512:704512 + 12 * 4096]
    Path(out).write_bytes(img)
    return out


def checker(title):
    return m5.checker(title)


def test_sends(empty, rep):
    lines, st, check = checker("M5.1: sending on the timed emulator (no watchdog reset, no XIP read while it is off)")
    d = m5.OUT / "flash"
    m4 = make_m4full(empty, d / "m4full.bin") if (ROOT / "build/step-M4/final/emu/flash/private.bin").exists() \
        else None
    frag = make_frag(empty, d / "frag.bin")
    if m4:
        a = timed("m51-incident-ours", 200000, [], flash=str(m4), host=send(OURS))
        check(a["stored"] == len(OURS) and healthy(a),
              f"the incident's store (M4: 9 songs + 2 private, 68 of 69 sectors), `send songs/*/`: all {len(OURS)} "
              f"stored, longest stretch without the watchdog {a['gap']} ms, {a['busy']} of them made room first")
        b = timed("m51-incident-priv", 300000, [], flash=str(a["flash"]), host=send(PRIV)) if PRIV else a
        n, free, bad, _ = listed(b["flash"], "m51-incident-list")
        check(b["stored"] == (len(PRIV) if PRIV else len(OURS)) and healthy(b) and n == len(ALL) and not bad,
              f"then `send songs/private/*/`: all {len(ALL)} songs on the FM-1, each ok ({free} blocks free); longest "
              f"stretch {b['gap']} ms")
        p = timed("m51-incident-play", m5.PLAY_MS + 4000, m5.press(m5.OPEN_MS, "PLAY") + m5.press(m5.OPEN_MS + 900, "HOME")
                  + m5.press(m5.PLAY_MS, "PLAY"),   # (HOME closes the controls hint: on, as the FM-1 comes)
                  flash=str(b["flash"]))
        check(p["stats"]["starts"] == 1 and m5.rms(p["left"][int((m5.PLAY_MS + 1500) / 1000 * m5.RATE):]) > 50
              and healthy(p), "and the first song in the list plays (after the controls hint)")
    else:
        lines.append("- (no M4 flash images in build/step-M4/: the incident's store is not tested)")
    e = timed("m51-empty-all", 300000, [], flash=str(empty), host=send(ALL))
    n, free, bad, _ = listed(e["flash"], "m51-empty-list")
    check(e["stored"] == len(ALL) and healthy(e) and n == len(ALL) and not bad,
          f"an empty store, all {len(ALL)} songs in one command: stored, listed ok, {free} blocks free; longest "
          f"stretch {e['gap']} ms")
    f = timed("m51-frag-all", 300000, [], flash=str(frag), host=send(ALL))
    n, free, bad, _ = listed(f["flash"], "m51-frag-list")
    check(f["stored"] == len(ALL) and healthy(f) and n == len(ALL) and not bad and f["busy"] > 0,
          f"a full, fragmented store (no erased block; stale copies and leftovers in every sector), all {len(ALL)} in "
          f"one command: {f['busy']} made room first, all stored, listed ok; longest stretch {f['gap']} ms, "
          f"{f['ops']} flash operations")
    cur, ok1, worst = frag, True, 0
    for i, s in enumerate(ALL):                     # one by one: a command each, the flash carried on
        r = timed(f"m51-one-{i:02}", 120000, [], flash=str(cur), host=send([s]))
        ok1 &= r["stored"] == 1 and healthy(r)
        worst = max(worst, r["gap"])
        cur = r["flash"]
    n, free, bad, _ = listed(cur, "m51-one-list")
    r, up = boots(cur, "m51-one-boot")
    check(ok1 and n == len(ALL) and not bad and up,
          f"the same store, the {len(ALL)} songs one command each: all stored, listed ok, it boots to the list; "
          f"longest stretch {worst} ms")
    rep += lines
    return st["ok"]


def test_cuts(empty, rep):
    lines, st, check = checker("M5.1: power cuts while sending (making room included), a boot after each")
    frag = m5.OUT / "flash" / "frag.bin"
    if not frag.exists():
        make_frag(empty, frag)
    whole = timed("m51-cut-none", 300000, [], flash=str(frag), host=send(ALL))
    total = whole["ops"]
    rnd = random.Random(51)
    points = sorted({1, 2, 3, 5, 8, 13, 21, 34, 55, total // 2, total - 1} |
                    {rnd.randrange(1, total) for _ in range(9)})
    points = [p for p in points if 0 < p < total]

    def one(n):
        c = timed(f"m51-cut-{n}", 300000, [], flash=str(frag), host=send(ALL), cut=n)
        b, up = boots(c["flash"], f"m51-cut-{n}-boot")
        a = timed(f"m51-cut-{n}-again", 300000, [], flash=str(c["flash"]), host=send(ALL))
        k, free, bad, _ = listed(a["flash"], f"m51-cut-{n}-list")
        return n, up and b["xip"] == 0, a["stored"] == len(ALL) and healthy(a) and k == len(ALL) and not bad, \
            max(c["gap"], b["gap"], a["gap"])

    with ThreadPoolExecutor(3) as pool:
        res = list(pool.map(one, points))
    bad_boot = [n for n, up, _, _ in res if not up]
    bad_again = [n for n, _, ok, _ in res if not ok]
    check(not bad_boot, f"a power cut at {len(points)} points of the {total} flash operations of sending all "
          f"{len(ALL)} to the fragmented store ({', '.join(map(str, points))}): it starts every time, to the song list"
          + (f" (NOT after {bad_boot})" if bad_boot else ""))
    check(not bad_again, "and sending all again afterwards stores every song, each listed ok, no watchdog reset; "
          f"longest stretch {max(g for *_, g in res)} ms" + (f" (NOT after {bad_again})" if bad_again else ""))
    rep += lines
    return st["ok"]


def test_junk_and_wipe(empty, rep):
    lines, st, check = checker("M5.1: any store content tolerated; WIPE SONG STORE")
    d = m5.OUT / "flash"
    img = bytearray(Path(empty).read_bytes())
    rnd = random.Random(7)
    img[LO:HI] = bytes(rnd.randrange(256) for _ in range(HI - LO))
    for o in range(LO, HI, 4096 * 5):               # head-like starts in the junk
        img[o:o + 4] = b"FM2R" if (o // 4096) % 2 else b"FM1R"
    junk = d / "junk.bin"
    junk.write_bytes(img)
    b, up = boots(junk, "m51-junk-boot")
    check(up and healthy(b), f"junk in the whole store: it starts, to the song list; longest stretch {b['gap']} ms")
    w = timed("m51-junk-wipe", 60000, [], flash=str(junk), host=f"{SEND} wipe --emu-pipe --yes")
    check("song store wiped" in w["out"] and area(w["flash"]) == b"\xff" * (HI - LO) and healthy(w) and
          Path(w["flash"]).read_bytes()[:LO] == bytes(img[:LO]) and Path(w["flash"]).read_bytes()[HI:] == bytes(img[HI:]),
          f"`fm1song wipe --yes`: the whole song store erased (0x97000..0xDBFFF all 0xFF), nothing outside it "
          f"changed; longest stretch {w['gap']} ms")
    s = timed("m51-junk-send", 60000, [], flash=str(w["flash"]), host=send(OURS[:1]))
    check(s["stored"] == 1 and healthy(s), "then a song stores")
    n = timed("m51-wipe-no", 30000, [], flash=str(w["flash"]), host=f"{SEND} wipe --emu-pipe")
    check("--yes" in n["out"] and area(n["flash"]) == area(w["flash"]), "`fm1song wipe` without --yes: nothing sent")
    songs = m5.FLASH["songs"]
    nav = m5.press(4600, "GLO") + m5.press(4800, "GLO") + ["5000:e0:10", "5300:e1:1"]
    h = timed("m51-wipe-settings-short", 9000, nav + ["5800:PLAY", "6800:PLAY^"], flash=songs)
    check(area(h["flash"]) == area(songs) and h["stats"]["ui_view"] == V_WIPE,
          "Settings > Wipe song store: the question first; PLAY held 1 s: nothing wiped")
    k = timed("m51-wipe-settings-home", 9000, nav + m5.press(5800, "HOME"), flash=songs)
    check(area(k["flash"]) == area(songs) and k["stats"]["ui_view"] == 3, "HOME: back to the settings, nothing wiped")
    g = timed("m51-wipe-settings", 10000, nav + ["5800:PLAY", "8100:PLAY^"], flash=songs)
    m5.shutil.copy(m5.OUT / "m51-wipe-settings.png", m5.OUT / "m51-wipe-done.png")
    check(area(g["flash"]) == b"\xff" * (HI - LO) and healthy(g) and g["stats"]["ui_view"] == V_WIPE,
          f"PLAY held 2 s: the song store erased, \"SONG STORE WIPED\" (m51-wipe-done.png); longest stretch "
          f"{g['gap']} ms")
    q = timed("m51-wipe-ask", 9000, nav, flash=songs)   # (its picture: the question)
    p = timed("m51-wipe-power-on", 3000, ["0:GLO", "0:HOME", "1500:GLO^", "1500:HOME^"], flash=songs, mode=None)
    check(p["stats"]["ui_view"] == V_WIPE and area(p["flash"]) == area(songs),
          "GLO + HOME held at power-on: the WIPE question (m51-wipe-power-on.png), nothing wiped yet")
    c = timed("m51-wipe-power-on-keep", 5000, ["0:GLO", "0:HOME", "1500:GLO^", "1500:HOME^"] + m5.press(2500, "HOME"),
              flash=songs, mode=None)
    check(c["stats"]["ui_view"] == V_MODE and area(c["flash"]) == area(songs),
          "then HOME: the mode screen, the songs kept (the held GLO + HOME alone never cancel nor wipe)")
    del q
    rep += lines
    return st["ok"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pkg", help="the package (default: the dev build)")
    ap.add_argument("--out", help="the work folder (default build/emu/m51)")
    ap.add_argument("--only")
    a = ap.parse_args()
    m5.PKG = Path(a.pkg).resolve() if a.pkg else m5.PKG
    m5.OUT = Path(a.out).resolve() if a.out else ROOT / "build/emu/m51"
    m5.OUT.mkdir(parents=True, exist_ok=True)
    (m5.OUT / "flash").mkdir(exist_ok=True)
    empty = m5.OUT / "flash" / "empty.bin"
    empty.unlink(missing_ok=True)
    m5.subprocess.run([str(m5.EMU), "--fwsc", str(m5.PKG), "--flash", str(empty), "--bench", "1500"],
                      env={**os.environ, "FM1_BENCH_NOKEYS": "1"}, capture_output=True, check=True)
    m5.FLASH["empty"] = empty
    s = timed("m51-songs", 120000, [], flash=str(empty), host=send(OURS))
    m5.FLASH["songs"] = s["flash"]
    tests = {"sends": test_sends, "cuts": test_cuts, "wipe": test_junk_and_wipe}
    only = set(a.only.split(",")) if a.only else set(tests)
    rep, results = {k: [] for k in tests if k in only}, {}
    with ThreadPoolExecutor(3) as pool:
        futs = {k: pool.submit(fn, empty, rep[k]) for k, fn in tests.items() if k in only}
        results = {k: f.result() for k, f in futs.items()}
    worst = max(RUNS, key=lambda r: r[3])
    resets = [r[0] for r in RUNS if r[1]]
    xips = [r[0] for r in RUNS if r[2]]
    text = [f"# M5.1 emulator tests (timed flash)\n\nPackage: {m5.PKG.relative_to(ROOT) if m5.PKG.is_relative_to(ROOT) else m5.PKG}"
            f", sha256 {m5.subprocess.run(['sha256sum', str(m5.PKG)], capture_output=True, text=True).stdout.split()[0]}"
            "\n\nfm1-live with FM1_FLASH_TIMED=1 (tools/patches/fm1-emu-flash-timing.patch): 4 KiB erase 8 ms, page "
            "program 2 ms (Puya P25Q-H typical), data-area XIP reads ~300 cycles a cache-line miss, a read of the XIP "
            "window while a flash command has it off stalls the core until the watchdog (8 s) resets it.\n"]
    for k in rep:
        text += rep[k]
    text += [f"\n## Over all {len(RUNS)} runs\n",
             f"- longest stretch without feeding the watchdog after the first second: **{worst[3]} ms** ({worst[0]}); "
             f"the watchdog: 8000 ms; the goal: under {GAP_LIMIT_MS} ms",
             f"- watchdog resets: {len(resets)}" + (f" ({', '.join(resets)})" if resets else ""),
             f"- XIP reads while a flash command had it off: {len(xips)}" + (f" ({', '.join(xips)})" if xips else "")]
    ok = all(results.values()) and not resets and not xips and worst[3] < GAP_LIMIT_MS
    text.append(f"\n**{'ALL PASS' if ok else 'FAILURES: ' + ', '.join(k for k, v in results.items() if not v)}**\n")
    (m5.OUT / "report.md").write_text("\n".join(text))
    print("\n".join(text))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
