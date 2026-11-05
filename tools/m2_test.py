#!/usr/bin/env python3
# m2_test.py -- Milestone M2: headless emulator tests of the firmware. Never touches the FM-1.
# Licence: TBD (see D017)
#
#   .venv/bin/python -I tools/m2_test.py [--jobs N] [--only SLUG|store|flash] [--pkg PACKAGE]
#
# Runs the package in fm1-live (third_party/fm1-emu, patched: tools/apply_patches.sh; deterministic emulated clock)
# and reads the player's numbers from a RAM dump (struct player_stats, "PLST", firmware/app/player.h). The songs are
# not in the firmware (M2): the FM-1 receives them first, over emulated USB from tools/fm1song send (fm1-live
# FM1_USB_HOST), into its flash song store; every run then starts from a copy of that flash. Writes build/emu/m2/:
# <name>.png (the screen), report.md (every number).
#
# The song store (part 1):
#   empty    no songs: the list says how to send them; PLAY does nothing; no crash
#   list     every song sent and stored byte for byte: list shows each, ok
#   junk     the area full of another firmware's leftovers: no songs; a song sent over it is stored
#   damaged  a stored song with a flipped byte: SONG ERROR when opened, the others still play
#   refused  a damaged file sent unchecked (--no-validate): refused by the FM-1, nothing written
#   stall    the laptop stops answering half way: the FM-1 gives up, stores nothing, carries on
#   cut      a power cut during a send (FM1_FLASH_CUT at several points): the other songs intact; sent again: stored
# Per song (playback):
import argparse
import math
import os
import random
import re
import shutil
import struct
import subprocess
import sys
import zlib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from fm1song import compile_folder, read_compiled   # noqa: E402

EMU = ROOT / "third_party/fm1-emu/target/release/fm1-live"
PKG = ROOT / "firmware/app/build/fm1acc.fwsc"
OUT = ROOT / "build/emu/m2"
SEND = f"{ROOT}/tools/fm1song.sh"
AREA = (0x97000, 0xDC000)                      # the song store (firmware/app/store.h)
FLASH = {}                                     # the prepared flash images: "empty", "songs"
LCD = (370, 42, 850, 522)                      # the LCD in fm1-live's panel image (shown 2x)
RATE = 44100
BLOCK = 256
OPEN_MS, PLAY_MS = 1600, 2200                  # after the splash (~1.2 s): open the song, then PLAY
STATS = struct.Struct("<14I32I32I32i4I4I4I3I64H64B256I256I")
FIELDS = ("magic version blocks render_max budget starts start_frames ends end_frames last_tick loops loop_from "
          "loop_to ntempo").split()


def songs():
    out = []
    for f in sorted(p for p in (ROOT / "songs").iterdir() if (p / "song.yaml").is_file()):
        c = read_compiled(compile_folder(f)[0])
        out.append(c)
    out.sort(key=lambda c: c["title"].lower())       # the firmware's list order (tools/songs_to_c.py)
    return out


def press(ms, name):
    return [f"{ms}:{name}", f"{ms + 100}:{name}^"]


def run(name, ms, script, flash_id=None, flash="songs", host=None, cut=None):
    """one emulator run. flash: a prepared image's name ("songs", "empty"), a path, or None (a blank chip); host: a
    command playing the laptop over USB (FM1_USB_HOST; the run ends soon after it does); cut: FM1_FLASH_CUT"""
    d = OUT / "runs"
    d.mkdir(parents=True, exist_ok=True)
    env = {**os.environ, "FM1_BENCH_KEYS": ",".join(script), "FM1_BENCH_WAV": str(d / f"{name}.raw"),
           "FM1_BENCH_PANEL": str(d / f"{name}.ppm"), "FM1_DUMPRAM": str(d / f"{name}.ram")}
    for k in ("FM1_FLASH_ID", "FM1_USB_HOST", "FM1_FLASH_CUT"):
        env.pop(k, None)
    if not script:
        env["FM1_BENCH_NOKEYS"] = "1"
    if flash_id:
        env["FM1_FLASH_ID"] = flash_id
    if host:
        env["FM1_USB_HOST"] = host
    if cut:
        env["FM1_FLASH_CUT"] = str(cut)
    fl = d / f"{name}.flash"
    fl.unlink(missing_ok=True)
    if flash:
        shutil.copy(FLASH.get(flash, flash), fl)
    args = [str(EMU), "--fwsc", str(PKG), "--flash", str(fl), "--bench", str(ms)] + (["--usb"] if host else [])
    r = subprocess.run(args, env=env, capture_output=True, text=True)
    out = r.stdout + r.stderr
    m = re.search(r"panel at (\d+) ms", out)
    if r.returncode or not m:
        raise SystemExit(f"run {name}: fm1-live failed:\n{out[-2000:]}")
    raw = (d / f"{name}.raw").read_bytes()
    left = struct.unpack(f"<{len(raw) // 2}h", raw[:len(raw) // 2 * 2])[0::2]
    img = Image.open(d / f"{name}.ppm").convert("RGB").crop(LCD)
    img.save(OUT / f"{name}.png")
    ops = re.search(r"flash: (\d+) erases / programs", out)
    return {"name": name, "ms": int(m.group(1)), "left": left, "lcd": img, "stats": stats((d / f"{name}.ram").read_bytes()),
            "halt": "halt:" in out, "out": out, "flash": fl, "ops": int(ops.group(1)) if ops else 0}


def prepare_flash():
    """flash images of this build: "empty" (a blank song area) and "songs" (every song sent over USB)"""
    d = OUT / "flash"
    d.mkdir(parents=True, exist_ok=True)
    empty = d / "empty.bin"
    empty.unlink(missing_ok=True)
    subprocess.run([str(EMU), "--fwsc", str(PKG), "--flash", str(empty), "--bench", "1500"],
                   env={**os.environ, "FM1_BENCH_NOKEYS": "1"}, capture_output=True, check=True)
    FLASH["empty"] = empty
    folders = " ".join(str(f) for f in sorted((ROOT / "songs").iterdir()) if (f / "song.yaml").is_file())
    r = run("send-all", 120000, [], flash="empty", host=f"{SEND} send --emu-pipe {folders}")
    shutil.copy(r["flash"], d / "songs.bin")
    FLASH["songs"] = d / "songs.bin"
    return r


def records(path):
    """the song store's records in a flash image: [(offset, file length)] (store.h's layout)"""
    f = Path(path).read_bytes()
    out, off = [], AREA[0]
    while off < AREA[1]:
        if f[off:off + 4] == b"FM1R" and zlib.crc32(f[off:off + 28]) == struct.unpack_from("<I", f, off + 28)[0]:
            n = struct.unpack_from("<I", f, off + 8)[0]
            out.append((off, n))
            off += (32 + n + 4095) // 4096 * 4096
        else:
            off += 4096
    return out


def stats(ram):
    i = ram.find(b"PLST")
    while i >= 0 and struct.unpack_from("<I", ram, i + 4)[0] != 1:
        i = ram.find(b"PLST", i + 1)
    if i < 0:
        return None
    v = STATS.unpack_from(ram, i)
    s = dict(zip(FIELDS, v[:14]))
    p = 14
    s["tempo_frames"], s["tempo_pct"], s["tempo_tick"] = v[p:p + 32], v[p + 32:p + 64], v[p + 64:p + 96]
    p += 96
    s["notes_on"], s["notes_skipped"], s["mutes"] = v[p:p + 4], v[p + 4:p + 8], v[p + 8:p + 12]
    p += 12
    s["deferred"], s["song_crc"], s["voices_after_stop"] = v[p:p + 3]
    p += 3
    s["voices"], s["cpu"] = v[p:p + 64], v[p + 64:p + 128]
    p += 128
    s["chord_due"], s["chord_drawn"] = v[p:p + 256], v[p + 256:p + 512]
    return s


def rms(x):
    return math.sqrt(sum(v * v for v in x) / len(x)) if x else 0.0


def ticks_to_frames(c, ticks, pct=100):
    return ticks * c["tempo_us"] * 441 / (c["ppq"] * pct * 100)


def song_ms(c, count_in=True, pct=100):
    bpb = int(c["time_signature"][0])
    start = (bpb - c["pickup_beats"]) * c["ppq"] if c["pickup_beats"] else 0
    ticks = c["end_tick"] - (-bpb * c["ppq"] if count_in else start)
    return ticks_to_frames(c, ticks, pct) / RATE * 1000


def section_region(c, i):
    bt = int(c["time_signature"][0]) * c["ppq"]
    s = c["sections"][i]
    return s["first_bar"] * bt - s["lead_ticks"], (s["first_bar"] + s["bars"]) * bt - s["lead_ticks"]


def open_song(i):
    return ([f"1450:e0:{i}"] if i else []) + press(OPEN_MS, "PLAY")


def test_song(i, c, rep):
    slug, ok = c["slug"], True
    bpb = int(c["time_signature"][0])
    start_tick = (bpb - c["pickup_beats"]) * c["ppq"] if c["pickup_beats"] else 0
    lines = [f"\n## {c['title']} ({slug})\n"]

    def check(cond, text):
        nonlocal ok
        lines.append(f"- {'PASS' if cond else 'FAIL'}: {text}")
        ok &= bool(cond)

    # ---- full
    total = song_ms(c)
    script = open_song(i) + press(PLAY_MS, "PLAY") + [f"{PLAY_MS + 5000}:e3:1"] + press(PLAY_MS + 10000, "ENV") \
        + press(PLAY_MS + 12000, "FX") + press(PLAY_MS + 18000, "ENV")
    shot_ms = PLAY_MS + 13000
    full = run(f"{slug}-full", int(PLAY_MS + total + 2500), script)
    mid = run(f"{slug}-screen", shot_ms, script)
    s = full["stats"]
    check(s is not None and not full["halt"], "the firmware ran to the end of the run (no halt), stats found")
    if s is None:
        rep += lines
        return False
    want = ticks_to_frames(c, c["end_tick"] + bpb * c["ppq"])
    got = s["end_frames"] - s["start_frames"]
    check(s["starts"] == 1 and s["ends"] == 1, f"played once to the end (starts {s['starts']}, ends {s['ends']})")
    check(abs(got - want) <= BLOCK, f"length: {got} frames from PLAY to the end, the song predicts {want:.0f} "
                                    f"(count-in + {c['bar_count']} bars at {c['tempo_bpm']:g} bpm)")
    due, drawn = s["chord_due"], s["chord_drawn"]
    margins, late, missing = [], [], []
    for k, ev in enumerate(c["chord_events"][:256]):
        if ev["tick"] < start_tick or k == 0 or not due[k]:
            continue
        if not drawn[k]:
            missing.append(k)
        elif drawn[k] > due[k]:
            late.append((k, (drawn[k] - due[k]) / RATE * 1000))
        else:
            margins.append((due[k] - drawn[k]) / RATE * 1000)
    n_changes = len([e for e in c["chord_events"][1:256] if e["tick"] >= start_tick])
    check(not late and not missing and len(margins) == n_changes,
          f"chord on screen before its sound: {len(margins)} of {n_changes} changes, early by "
          f"{min(margins, default=0):.0f}..{max(margins, default=0):.0f} ms"
          + (f"; LATE {late[:4]}" if late else "") + (f"; never drawn {missing[:6]}" if missing else ""))
    check(s["notes_skipped"][2] > 0 and s["notes_skipped"][0] > 0,
          f"muted layers held back: harmony {s['notes_skipped'][2]}, drums {s['notes_skipped'][0]} note-ons "
          f"(sent: drums {s['notes_on'][0]}, bass {s['notes_on'][1]}, harmony {s['notes_on'][2]}, "
          f"melody {s['notes_on'][3]}); mutes that released held notes {list(s['mutes'])}")
    check(s["voices_after_stop"] == 0, f"no EAS voice 1 s after the end (got {s['voices_after_stop']})")
    check(s["deferred"] == 0, f"no block deferred events (got {s['deferred']})")
    nbars = min(c["bar_count"], 64)
    busiest = max(range(nbars), key=lambda b: s["voices"][b])
    check(max(s["voices"][:nbars]) <= 24, f"voices at most {max(s['voices'][:nbars])} of 24 (bar index {busiest}); "
                                          f"CPU load at most {max(s['cpu'][:nbars])} %; longest EAS render "
                                          f"{s['render_max'] / s['budget'] * 100:.1f} % of a block")
    play_from = int((PLAY_MS + 500) / 1000 * RATE)
    check(rms(full["left"][play_from:play_from + RATE * 4]) > 50, "sound while playing")

    # ---- tempo
    script = open_song(i) + press(OPEN_MS + 300, "ARP") + [f"{OPEN_MS + 400}:e0:-10"] + press(PLAY_MS, "PLAY") \
        + [f"{PLAY_MS + 6000}:e0:14"]
    tr = run(f"{slug}-tempo", PLAY_MS + 20000, script)
    t = tr["stats"]
    n = t["ntempo"]
    check(1 <= n <= 32 and t["tempo_pct"][n - 1] == 120,
          f"tempo changed while playing: {n} step(s), " + " ".join(f"{p}%" for p in t["tempo_pct"][:n]))
    played = t["blocks"] * BLOCK
    rate = lambda pct: c["ppq"] * pct * 100 / (c["tempo_us"] * 441)
    marks = [t["start_frames"]] + list(t["tempo_frames"][:n]) + [t["start_frames"] + played]
    pcts = [50] + list(t["tempo_pct"][:n])
    want_tick = start_tick + sum((marks[k + 1] - marks[k]) * rate(pcts[k]) for k in range(len(pcts)))
    check(abs(t["last_tick"] - want_tick) <= 1, f"position after {played} frames at those tempos: "
                                                f"{t['last_tick']}, exact {want_tick:.1f} (no drift)")
    audio_ms = played / RATE * 1000
    check(abs(audio_ms - (tr["ms"] - PLAY_MS)) < 60, f"frames played match emulated time: {audio_ms:.0f} ms of "
                                                     f"audio in the {tr['ms'] - PLAY_MS} ms since PLAY")
    check(t["ends"] == 0, "not at the end yet")

    # ---- loop
    a, b = section_region(c, 0)
    sec_ms = ticks_to_frames(c, b - a) / RATE * 1000
    script = open_song(i) + press(OPEN_MS + 300, "ARP") + press(PLAY_MS, "PLAY") + press(PLAY_MS + 3000, "SEQ") \
        + press(PLAY_MS + 3300, "SEQ")
    lr = run(f"{slug}-loop", int(PLAY_MS + 3000 + 1.4 * sec_ms), script)
    lo = lr["stats"]
    check(lo["loops"] >= 1 and lo["loop_from"] == a and lo["loop_to"] == b and lo["ends"] == 0,
          f"section loop: {lo['loops']} jump(s) from tick {lo['loop_to']} to {lo['loop_from']} (expected {b} -> {a}: "
          f"section '{c['sections'][0]['name']}' minus its {c['sections'][0]['lead_ticks']}-tick lead-in)")
    check(a <= lo["last_tick"] < b, f"after the jump, inside the loop (tick {lo['last_tick']})")
    check(lo["voices_after_stop"] in (0, 0xFFFFFFFF), "no voices left by a stop")
    lines.append(f"- screens: {slug}-screen.png (at {shot_ms - PLAY_MS} ms: bass L2, harmony and drums muted), "
                 f"{slug}-loop.png, {slug}-tempo.png")
    rep += lines
    return ok


def test_store(all_songs, sent, rep):
    lines, ok = ["\n## The song store (part 1)\n"], True

    def check(cond, text):
        nonlocal ok
        lines.append(f"- {'PASS' if cond else 'FAIL'}: {text}")
        ok &= bool(cond)

    slugs = [c["slug"] for c in all_songs]
    stored = {}
    for off, n in records(FLASH["songs"]):
        blob = FLASH["songs"].read_bytes()[off + 32:off + 32 + n]
        stored[read_compiled(blob)["slug"]] = blob
    want = {c["slug"]: compile_folder(ROOT / "songs" / c["slug"])[0] for c in all_songs}
    check(sent["out"].count(": stored (") == len(slugs) and stored == want,
          f"sent all {len(slugs)} songs over USB ({sent['ms']} ms emulated, {sent['ops']} flash erases / programs); "
          f"each stored byte for byte ({sum(len(b) for b in want.values())} bytes, format 1.1)")
    li = run("store-list", 30000, [], host=f"{SEND} list --emu-pipe")
    check(all(re.search(rf"{s} +\d+ bytes +\d+ sectors +ok", li["out"]) for s in slugs) and li["ops"] == 0,
          f"fm1song list: every song, ok; nothing written by listing")
    lst = run("store-songs", 1800, [])
    check(not lst["halt"], "the song list with every song (store-songs.png)")

    em = run("store-empty", 3500, press(OPEN_MS, "PLAY") + press(PLAY_MS, "PLAY"), flash="empty")
    check(not em["halt"] and em["stats"] and em["stats"]["starts"] == 0 and em["ops"] == 0,
          "empty song area: the list says how to send songs (store-empty.png); PLAY does nothing; no crash; "
          "nothing written")

    junk = OUT / "runs" / "junk-src.flash"
    f = bytearray(FLASH["empty"].read_bytes())
    rng = random.Random(5)
    f[AREA[0]:AREA[1]] = bytes(rng.randrange(256) for _ in range(AREA[1] - AREA[0]))
    f[AREA[0] + 0x5000:AREA[0] + 0x5004] = b"FM1R"
    junk.write_bytes(f)
    j1 = run("store-junk", 2500, [], flash=junk)
    j2 = run("store-junk-send", 60000, [], flash=junk, host=f"{SEND} send --emu-pipe {ROOT}/songs/saints")
    j3 = run("store-junk-after", 1800, [], flash=j2["flash"])
    check(not j1["halt"] and j1["ops"] == 0 and "saints: stored" in j2["out"] and len(records(j2["flash"])) == 1,
          f"junk in the song area (another firmware's leftovers): no songs listed, nothing written; a song sent over "
          f"it is stored ({j2['ops']} erases / programs) and listed (store-junk.png, store-junk-after.png)")

    # a stored song damaged: a byte flipped in its events
    recs = records(FLASH["songs"])
    dmg = OUT / "runs" / "damaged-src.flash"
    f = bytearray(FLASH["songs"].read_bytes())
    off, n = recs[0]
    victim = read_compiled(bytes(f[off + 32:off + 32 + n]))["slug"]
    f[off + 32 + n - 40] ^= 0x10
    dmg.write_bytes(f)
    vi = slugs.index(victim)
    other = (vi + 1) % len(slugs)
    d1 = run("store-damaged", PLAY_MS + 4500, open_song(vi) + press(PLAY_MS, "PLAY"), flash=dmg)
    d2 = run("store-damaged-other", PLAY_MS + 3000, open_song(other) + press(PLAY_MS, "PLAY"), flash=dmg)
    check(d1["stats"] and d1["stats"]["starts"] == 0 and not d1["halt"],
          f"a damaged stored song ({victim}): SONG ERROR when opened, not played (store-damaged.png)")
    check(d2["stats"] and d2["stats"]["starts"] == 1 and rms(d2["left"][int((PLAY_MS + 1500) / 1000 * RATE):]) > 50,
          f"the other songs still play ({slugs[other]})")

    # a damaged file sent unchecked: past the laptop's check, refused by the FM-1's
    good = want["saints"]
    bad_crc = OUT / "runs" / "bad-crc.fm1s"
    b = bytearray(good)
    b[300] ^= 0x01
    bad_crc.write_bytes(b)
    bad_struct = OUT / "runs" / "bad-struct.fm1s"
    b = bytearray(good)
    b[33] = 0xFF                                # the directory's first entry's id: a required chunk missing
    b[12:16] = bytes(4)
    b[12:16] = struct.pack("<I", zlib.crc32(bytes(b)))
    bad_struct.write_bytes(b)
    rf = run("store-refused", 60000, [], host=f"{SEND} send --emu-pipe --no-validate {bad_crc} {bad_struct}")
    check(rf["out"].count("NOT stored") == 2 and "CRC" in rf["out"] and "refused the song" in rf["out"]
          and rf["ops"] == 0, "damaged files sent unchecked (--no-validate): both refused by the FM-1 (bad CRC; a "
                              "missing chunk), nothing written")

    # the laptop stops answering after 3 of the reads
    good_path = OUT / "runs" / "good.fm1s"
    good_path.write_bytes(good)
    st = run("store-stall", 60000, [], host=f"{sys.executable} -I {ROOT}/tools/m2_host.py stall {good_path} 3")
    check("result 3" in st["out"] and st["ops"] == 0, "the laptop stops answering half way: the FM-1 gives up "
                                                      "(transfer stopped), nothing written")
    st2 = run("store-stall-after", 30000, [], flash=st["flash"], host=f"{SEND} list --emu-pipe")
    check(all(re.search(rf"{s} +\d+ bytes +\d+ sectors +ok", st2["out"]) for s in slugs),
          "after it: every song still there, ok")

    # power cuts during a send: four songs stored, the fifth sent and cut at several points
    four = OUT / "runs" / "four-src.flash"
    f = bytearray(FLASH["songs"].read_bytes())
    last_off, last_n = recs[-1]
    last_slug = read_compiled(bytes(f[last_off + 32:last_off + 32 + last_n]))["slug"]
    f[last_off:last_off + 4096] = b"\xff" * 4096          # its head sector erased: gone
    four.write_bytes(f)
    probe = run("store-cut-probe", 60000, [], flash=four, host=f"{SEND} send --emu-pipe {ROOT}/songs/{last_slug}")
    total = probe["ops"]
    cuts = sorted({1, 2, 3, total // 2, total - 1, total})
    bad = []
    for k in cuts:
        c1 = run(f"store-cut-{k}", 60000, [], flash=four, host=f"{SEND} send --emu-pipe {ROOT}/songs/{last_slug}",
                 cut=k)
        c2 = run(f"store-cut-{k}-after", 30000, [], flash=c1["flash"], host=f"{SEND} list --emu-pipe")
        others = [s for s in slugs if s != last_slug]
        intact = all(re.search(rf"{s} +\d+ bytes +\d+ sectors +ok", c2["out"]) for s in others)
        lastok = re.search(rf"{last_slug} +\d+ bytes +\d+ sectors +(\S+)", c2["out"])
        if not intact or (lastok and lastok.group(1) != "ok") or c2["halt"]:
            bad.append(k)
        if k == cuts[len(cuts) // 2]:
            c3 = run(f"store-cut-{k}-again", 60000, [], flash=c1["flash"],
                     host=f"{SEND} send --emu-pipe {ROOT}/songs/{last_slug}")
            again = f"{last_slug}: stored" in c3["out"]
    check(not bad and again, f"power cut during a send ({last_slug}: {total} erases / programs; cut at {cuts}): the "
                             f"other {len(slugs) - 1} songs intact every time, the cut song absent or whole, never "
                             f"half-listed; sent again after a cut: stored" + (f"; WRONG at {bad}" if bad else ""))
    rep += lines
    return ok


def test_flash(c, rep):
    lines, ok = ["\n## Flash chip ids (Amazing Grace)\n"], True
    script = press(OPEN_MS, "PLAY") + press(PLAY_MS, "PLAY")
    runs = {fid: run(f"flash-{fid}", PLAY_MS + 4000, script, flash_id=fid) for fid in ("856014", "b36014")}
    lst = {fid: run(f"flash-list-{fid}", 1500, [], flash_id=fid) for fid in ("856014", "b36014", "c84017")}
    for fid, r in runs.items():
        snd = rms(r["left"][int((PLAY_MS + 1500) / 1000 * RATE):])
        lines.append(f"- {fid}: halt {r['halt']}, sound rms {snd:.0f}, stats {'found' if r['stats'] else 'missing'}")
        ok &= not r["halt"] and snd > 50 and r["stats"] is not None
    same = ImageChops.difference(lst["856014"]["lcd"], lst["b36014"]["lcd"]).getbbox() is None
    diff = ImageChops.difference(lst["856014"]["lcd"], lst["c84017"]["lcd"]).getbbox() is not None
    un = run("flash-c84017-keys", 3000, ["1500:C4"], flash_id="c84017", flash="songs")
    keys = rms(un["left"][int(1.6 * RATE):int(2.6 * RATE)]) > 50
    us = run("flash-c84017-send", 30000, [], flash_id="c84017", host=f"{SEND} send --emu-pipe {ROOT}/songs/saints")
    refused = "NOT stored" in us["out"] and "RAM ONLY" in us["out"] and us["ops"] == 0
    lines.append(f"- {'PASS' if same else 'FAIL'}: 856014 and b36014 show the same list screen, and play")
    lines.append(f"- {'PASS' if diff and not lst['c84017']['halt'] else 'FAIL'}: unknown id c84017: NO SONG STORAGE "
                 f"(flash-list-c84017.png), no crash")
    lines.append(f"- {'PASS' if keys else 'FAIL'}: unknown id c84017: the keys still sound (C4 held)")
    lines.append(f"- {'PASS' if refused else 'FAIL'}: unknown id c84017: a song sent is refused (RAM ONLY), nothing "
                 f"written")
    rep += lines
    return ok and same and diff and keys and refused


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jobs", type=int, default=3)
    ap.add_argument("--only")
    ap.add_argument("--pkg", help="the package to test (default: the dev build, firmware/app/build/fm1acc.fwsc)")
    a = ap.parse_args()
    global PKG
    if a.pkg:
        PKG = Path(a.pkg).resolve()
    if not PKG.exists():
        raise SystemExit("m2_test: build first (make firmware)")
    OUT.mkdir(parents=True, exist_ok=True)
    all_songs = songs()
    reports, results = {}, {}
    sent = prepare_flash()
    with ThreadPoolExecutor(a.jobs) as pool:
        futs = {}
        for i, c in enumerate(all_songs):
            if a.only and c["slug"] != a.only:
                continue
            reports[c["slug"]] = []
            futs[c["slug"]] = pool.submit(test_song, i, c, reports[c["slug"]])
        if not a.only or a.only == "store":
            reports["store"] = []
            futs["store"] = pool.submit(test_store, all_songs, sent, reports["store"])
        if not a.only or a.only == "flash":
            reports["flash"] = []
            futs["flash"] = pool.submit(test_flash, all_songs[0], reports["flash"])
        for k, f in futs.items():
            results[k] = f.result()
    text = [f"# M2 emulator tests\n\nPackage: {PKG.relative_to(ROOT) if PKG.is_relative_to(ROOT) else PKG}, sha256 "
            f"{subprocess.run(['sha256sum', str(PKG)], capture_output=True, text=True).stdout.split()[0]}\n"]
    for k in reports:
        text += reports[k]
    text.append(f"\n**{'ALL PASS' if all(results.values()) else 'FAILURES: ' + ', '.join(k for k, v in results.items() if not v)}**\n")
    (OUT / "report.md").write_text("\n".join(text))
    print("\n".join(text))
    sys.exit(0 if all(results.values()) else 1)


if __name__ == "__main__":
    main()
