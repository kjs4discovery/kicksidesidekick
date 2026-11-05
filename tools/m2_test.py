#!/usr/bin/env python3
# m2_test.py -- Milestone M2: headless emulator tests of the firmware. Never touches the FM-1.
# Licence: TBD (see D017)
#
#   .venv/bin/python -I tools/m2_test.py [--jobs N] [--only SLUG|store|flash|boot|practice|views|levels] [--pkg PKG]
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
OPEN_MS, PLAY_MS = 3400, 4600                  # after the splash (3 s): open the song, then PLAY
STATS = struct.Struct("<14I64I64I64i4I4I4I3I64H64B256I256I" + "iiIIiIIIIiIIIi4I3I")   # PL_STATS_TEMPOS 64
FIELDS = ("magic version blocks render_max budget starts start_frames ends end_frames last_tick loops loop_from "
          "loop_to ntempo").split()
FIELDS2 = ("start_from start_tick pauses pause_frames pause_tick bpm clicks click_peak_vel key_note_last keys_octave "
           "ui_view flash_bars flash_beats transpose").split()
M1_PKG = ROOT / "build/step-M1/fm1acc-0.2.0-m1.fwsc"   # for before / after numbers (its songs are built in)


def songs():
    out = []
    for f in sorted(p for p in (ROOT / "songs").iterdir() if (p / "song.yaml").is_file()):
        c = read_compiled(compile_folder(f)[0])
        out.append(c)
    out.sort(key=lambda c: c["title"].lower())       # the firmware's list order (tools/songs_to_c.py)
    return out


def press(ms, name):
    return [f"{ms}:{name}", f"{ms + 100}:{name}^"]


def run(name, ms, script, flash_id=None, flash="songs", host=None, cut=None, pkg=None):
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
    args = [str(EMU), "--fwsc", str(pkg or PKG)] + (["--flash", str(fl)] if flash else []) + ["--bench", str(ms)] \
        + (["--usb"] if host else [])
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
    while i >= 0 and struct.unpack_from("<I", ram, i + 4)[0] != 3:
        i = ram.find(b"PLST", i + 1)
    if i < 0:
        return None
    v = STATS.unpack_from(ram, i)
    s = dict(zip(FIELDS, v[:14]))
    p = 14
    s["tempo_frames"], s["tempo_pct"], s["tempo_tick"] = v[p:p + 64], v[p + 64:p + 128], v[p + 128:p + 192]
    p += 192
    s["notes_on"], s["notes_skipped"], s["mutes"] = v[p:p + 4], v[p + 4:p + 8], v[p + 8:p + 12]
    p += 12
    s["deferred"], s["song_crc"], s["voices_after_stop"] = v[p:p + 3]
    p += 3
    s["voices"], s["cpu"] = v[p:p + 64], v[p + 64:p + 128]
    p += 128
    s["chord_due"], s["chord_drawn"] = v[p:p + 256], v[p + 256:p + 512]
    p += 512
    s.update(zip(FIELDS2, v[p:p + len(FIELDS2)]))
    p += len(FIELDS2)
    s["note_on_last"], (s["lit_keys"], s["dim_keys"], s["view"]) = v[p:p + 4], v[p + 4:p + 7]
    return s


def rms(x):
    return math.sqrt(sum(v * v for v in x) / len(x)) if x else 0.0


def own_bpm(c):
    return round(60e6 / c["tempo_us"])                 # the firmware's player_song_bpm


def ticks_to_frames(c, ticks, bpm=None):
    return ticks * RATE * 60 / (c["ppq"] * (bpm or own_bpm(c)))


def song_ms(c, count_in=True, bpm=None):
    bpb = int(c["time_signature"][0])
    start = (bpb - c["pickup_beats"]) * c["ppq"] if c["pickup_beats"] else 0
    ticks = c["end_tick"] - (-bpb * c["ppq"] if count_in else start)
    return ticks_to_frames(c, ticks, bpm) / RATE * 1000


def section_region(c, i):
    bt = int(c["time_signature"][0]) * c["ppq"]
    s = c["sections"][i]
    return s["first_bar"] * bt - s["lead_ticks"], (s["first_bar"] + s["bars"]) * bt - s["lead_ticks"]


def open_song(i):
    return ([f"3150:e0:{i}"] if i else []) + press(OPEN_MS, "PLAY")


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
                                    f"(count-in + {c['bar_count']} bars at {own_bpm(c)} BPM, its own)")
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
    script = open_song(i) + press(OPEN_MS + 300, "ARP") + [f"{OPEN_MS + 400}:e0:-60"] + press(PLAY_MS, "PLAY") \
        + [f"{PLAY_MS + 6000}:e0:60"]
    tr = run(f"{slug}-tempo", PLAY_MS + 14000, script)
    t = tr["stats"]
    n = t["ntempo"]
    check(1 <= n <= 64 and t["tempo_pct"][n - 1] == 250 and t["start_frames"] and t["bpm"] == 250,
          f"tempo 30 BPM from the start, then 250 BPM while playing: {n} step(s), "
          + " ".join(f"{p}" for p in t["tempo_pct"][:n]) + " BPM")
    played = t["blocks"] * BLOCK
    rate = lambda bpm: c["ppq"] * bpm / (RATE * 60)
    marks = [t["start_frames"]] + list(t["tempo_frames"][:n]) + [t["start_frames"] + played]
    pcts = [30] + list(t["tempo_pct"][:n])
    want_tick = start_tick + sum((marks[k + 1] - marks[k]) * rate(pcts[k]) for k in range(len(pcts)))
    check(abs(t["last_tick"] - want_tick) <= 1, f"position after {played} frames at those tempos: "
                                                f"{t['last_tick']}, exact {want_tick:.1f} (no drift)")
    audio_ms = played / RATE * 1000
    check(abs(audio_ms - (tr["ms"] - PLAY_MS)) < 60, f"frames played match emulated time: {audio_ms:.0f} ms of "
                                                     f"audio in the {tr['ms'] - PLAY_MS} ms since PLAY")
    check(t["deferred"] == 0 and t["ends"] == 0, f"30 and 250 BPM: no block deferred events (the 128 a block cap), "
                                               f"not at the end yet; CPU at most {max(t['cpu'][:min(c['bar_count'], 64)])} %")

    # ---- loop
    a, b = section_region(c, 0)
    sec_ms = ticks_to_frames(c, b - a) / RATE * 1000
    script = open_song(i) + press(OPEN_MS + 300, "ARP") + press(PLAY_MS, "PLAY") + press(PLAY_MS + 3000, "SEQ")
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
    lst = run("store-songs", 3500, [])
    check(not lst["halt"], "the song list with every song (store-songs.png)")

    em = run("store-empty", 4600, press(OPEN_MS, "PLAY") + press(PLAY_MS, "PLAY"), flash="empty")
    check(not em["halt"] and em["stats"] and em["stats"]["starts"] == 0 and em["ops"] == 0,
          "empty song area: the list says how to send songs (store-empty.png); PLAY does nothing; no crash; "
          "nothing written")

    junk = OUT / "runs" / "junk-src.flash"
    f = bytearray(FLASH["empty"].read_bytes())
    rng = random.Random(5)
    f[AREA[0]:AREA[1]] = bytes(rng.randrange(256) for _ in range(AREA[1] - AREA[0]))
    f[AREA[0] + 0x5000:AREA[0] + 0x5004] = b"FM1R"
    junk.write_bytes(f)
    j1 = run("store-junk", 3500, [], flash=junk)
    j2 = run("store-junk-send", 60000, [], flash=junk, host=f"{SEND} send --emu-pipe {ROOT}/songs/saints")
    j3 = run("store-junk-after", 3500, [], flash=j2["flash"])
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


def section_from(c, i):
    bt = int(c["time_signature"][0]) * c["ppq"]
    s = c["sections"][i]
    return s["first_bar"] * bt - s["lead_ticks"]


def test_boot(rep):
    """part 2: the splash for 3 s, then the list; the power-on checks (REC; OCT- + OCT+) still come first"""
    lines, ok = ["\n## Start-up (part 2)\n"], True

    def check(cond, text):
        nonlocal ok
        lines.append(f"- {'PASS' if cond else 'FAIL'}: {text}")
        ok &= bool(cond)

    sp = run("boot-splash", 2800, [])
    li = run("boot-list", 3300, [])
    cream = sp["lcd"].getpixel((20, 20))
    check(sp["stats"]["ui_view"] == 16 and cream[0] > 200 and cream[2] < 230 and li["stats"]["ui_view"] == 0,
          f"the splash (the logo, boot-splash.png) is still up at 2.8 s; the song list at 3.3 s (boot-list.png)")
    pk = run("boot-play-in-splash", 3300, press(2000, "PLAY"))
    check(pk["stats"]["ui_view"] == 0 and pk["stats"]["starts"] == 0,
          "PLAY during the splash does nothing (the list comes, no song opened)")
    oc = run("boot-oct-both", 4000, ["0:OCTDN", "0:OCTUP"])
    check(oc["stats"] is not None and oc["stats"]["ui_view"] == 0xFF and not oc["halt"],
          "OCT- and OCT+ held at power-on: fm1-base's panel set-up takes over, our screens never start "
          "(boot-oct-both.png)")
    rc = run("boot-rec", 4000, ["0:REC"])
    check((rc["stats"] is None or rc["stats"]["ui_view"] == 0xFF) and not rc["halt"],
          "REC held at power-on: the update mode, our screens never start (boot-rec.png)")
    rep += lines
    return ok


def test_practice(all_songs, rep):
    """part 2, on the Twelve-Bar Blues (and Amazing Grace for the count-in): start points, loops, pause, the keys"""
    lines, ok = ["\n## Practice controls (part 2)\n"], True

    def check(cond, text):
        nonlocal ok
        lines.append(f"- {'PASS' if cond else 'FAIL'}: {text}")
        ok &= bool(cond)

    slugs = [c["slug"] for c in all_songs]
    i = slugs.index("twelve-bar-blues")
    c = all_songs[i]
    bt = int(c["time_signature"][0]) * c["ppq"]
    beat_ms = 60000 / own_bpm(c)
    o = open_song(i)
    # start from a section (PRESETS), from a bar (ALGORITHM)
    r1 = run("prac-from-section", PLAY_MS + 2000, o + [f"{OPEN_MS + 300}:e6:1"] + press(PLAY_MS, "PLAY"))
    want = section_from(c, 1)
    check(r1["stats"]["start_from"] == want and r1["stats"]["start_tick"] == want // bt * bt - bt,
          f"PRESETS +1, PLAY: starts at section 2 ('{c['sections'][1]['name']}', tick {r1['stats']['start_from']}, "
          f"expected {want}), count-in from the bar before")
    r2 = run("prac-from-bar", PLAY_MS + 2000, o + [f"{OPEN_MS + 300}:e1:3"] + press(PLAY_MS, "PLAY"))
    want = (c["sections"][0]["first_bar"] + 3) * bt
    check(r2["stats"]["start_from"] == want, f"ALGORITHM +3, PLAY: starts at bar 4 (tick {r2['stats']['start_from']}, "
                                             f"expected {want}) (prac-from-bar.png)")
    # loop bars A-B: SEQ twice (section, bars) from bar 3 (ALGO +2); B moved to A+1 (ALGO -2 in bars mode)
    ab = o + [f"{OPEN_MS + 300}:e1:2"] + press(OPEN_MS + 400, "SEQ") + press(OPEN_MS + 600, "SEQ") \
        + [f"{OPEN_MS + 800}:e1:-2"] + press(OPEN_MS + 1000, "ARP")
    a_bar = c["sections"][0]["first_bar"] + 2
    loop_ms = 2 * 2 * bt * 60000 / (c["ppq"] * own_bpm(c))
    r3 = run("prac-loop-bars", int(PLAY_MS + loop_ms + 1500), ab + press(PLAY_MS, "PLAY"))
    st = r3["stats"]
    check(st["start_from"] == a_bar * bt and st["loops"] >= 2 and st["loop_from"] == a_bar * bt
          and st["loop_to"] == (a_bar + 2) * bt and a_bar * bt <= st["last_tick"] < (a_bar + 2) * bt,
          f"LOOP BARS 3-4 (SEQ twice, ALGORITHM moves B): starts at bar 3, {st['loops']} jumps from tick "
          f"{st['loop_to']} back to {st['loop_from']} (expected {(a_bar + 2) * bt} -> {a_bar * bt}) (prac-loop-bars.png)")
    # a section loop moved while playing: PRESETS +1 -> the next section, jumped to at the next bar line
    r4 = run("prac-loop-move", PLAY_MS + 6000, o + press(OPEN_MS + 300, "ARP") + press(PLAY_MS, "PLAY")
             + press(PLAY_MS + 1500, "SEQ") + [f"{PLAY_MS + 2500}:e6:1"])
    st = r4["stats"]
    s1_from, s1_to = section_from(c, 1), section_from(c, 1) + c["sections"][1]["bars"] * bt
    check(st["loops"] >= 1 and st["loop_from"] == s1_from and s1_from <= st["last_tick"] < s1_to,
          f"a section loop moved to section 2 while playing section 1: jumped there ({st['loops']} jump, now tick "
          f"{st['last_tick']} in {s1_from}..{s1_to})")
    # pause and go on: count-in on (from the paused bar's beat 1) and off (from right there)
    pz = o + press(PLAY_MS, "PLAY") + press(PLAY_MS + 6000, "PLAY")
    r5 = run("prac-paused", PLAY_MS + 10000, pz)
    st = r5["stats"]
    tail = rms(r5["left"][int((PLAY_MS + 6500) / 1000 * RATE):int((PLAY_MS + 7000) / 1000 * RATE)])
    quiet = rms(r5["left"][int((PLAY_MS + 9000) / 1000 * RATE):int((PLAY_MS + 9900) / 1000 * RATE)])
    check(st["pauses"] == 1 and quiet < 5 and st["voices_after_stop"] <= 2,
          f"PLAY while playing: paused at tick {st['pause_tick']}; the notes released at once (0.5 s on: rms {tail:.0f}, "
          f"release tails; {st['voices_after_stop']} voices fading at 1 s), silent 3 s on (rms {quiet:.0f}): no "
          f"hanging note (prac-paused.png)")
    r6 = run("prac-resume", PLAY_MS + 9000, pz + press(PLAY_MS + 8000, "PLAY"))
    st6 = r6["stats"]
    check(st6["starts"] == 2 and st6["start_from"] == st["pause_tick"] // bt * bt and
          st6["start_tick"] == st6["start_from"] - bt, f"PLAY again (count-in on): goes on from the paused bar's beat 1 "
                                                         f"(tick {st6['start_from']}) after a bar of count-in")
    r7 = run("prac-resume-nocount", PLAY_MS + 9000, o + press(OPEN_MS + 300, "ARP") + press(PLAY_MS, "PLAY")
             + press(PLAY_MS + 6000, "PLAY") + press(PLAY_MS + 8000, "PLAY"))
    st7 = r7["stats"]
    check(st7["starts"] == 2 and st7["start_from"] == st7["pause_tick"] == st7["start_tick"],
          f"count-in off: goes on from exactly where it paused (tick {st7['start_from']})")
    r8 = run("prac-home", PLAY_MS + 4000, o + press(PLAY_MS, "PLAY") + press(PLAY_MS + 2000, "HOME"))
    r9 = run("prac-home2", PLAY_MS + 4000, o + press(PLAY_MS, "PLAY") + press(PLAY_MS + 2000, "HOME")
             + press(PLAY_MS + 3000, "HOME"))
    check(r8["stats"]["ui_view"] == 1 and r8["stats"]["voices_after_stop"] == 0 and r9["stats"]["ui_view"] == 0,
          "HOME while playing: stopped, still on the play screen (prac-home.png), no voice left; HOME again: the list")
    # every song opens at its own BPM, whatever the last one was set to
    k = slugs.index("saints")
    r10 = run("prac-own-bpm", PLAY_MS + 3000, o + [f"{OPEN_MS + 200}:e0:-10"] + press(OPEN_MS + 700, "HOME")
              + [f"{OPEN_MS + 900}:e0:{k - i}"] + press(OPEN_MS + 1100, "PLAY"))
    check(r10["stats"]["bpm"] == own_bpm(all_songs[k]), f"tempo changed on one song, then another opened: it plays at "
                                                        f"its own {own_bpm(all_songs[k])} BPM "
                                                        f"(got {r10['stats']['bpm']})")
    # the keys' octave: C4 (key 7) is note 60; OCT+ twice: 84; OCT- 5 times: clamped at -3: 24
    r11 = run("prac-oct-up", 6000, press(3600, "OCTUP") + press(3800, "OCTUP") + ["4200:C4", "4400:C4^"])
    r12 = run("prac-oct-down", 6000, [f"{3600 + 150 * n}:OCTDN" for n in range(5)] +
              [f"{3650 + 150 * n}:OCTDN^" for n in range(5)] + ["4600:C4", "4800:C4^"])
    check(r11["stats"]["key_note_last"] == 84 and r11["stats"]["keys_octave"] == 2 and
          r12["stats"]["key_note_last"] == 24 and r12["stats"]["keys_octave"] == -3,
          f"OCT+ twice, C4: note {r11['stats']['key_note_last']} (84); OCT- five times (stops at -3), C4: note "
          f"{r12['stats']['key_note_last']} (24) (prac-oct-up.png: OCT+ lit)")
    # the beat light: a flash every beat, the bright one on beat 1 (count-in included)
    r13 = run("prac-beat-light", PLAY_MS + 8000, o + press(PLAY_MS, "PLAY"))
    st = r13["stats"]
    beats = 8000 / beat_ms
    check(abs(st["flash_bars"] + st["flash_beats"] - beats) <= 2 and abs(st["flash_bars"] - beats / 4) <= 1,
          f"beat light: {st['flash_bars']} bright + {st['flash_beats']} flashes in 8 s at {own_bpm(c)} BPM "
          f"(~{beats:.0f} beats, a quarter of them beat 1)")
    # the count-in: louder than M1's, beat 1 accented
    g = slugs.index("amazing-grace")
    cin = 5 * 60000 / own_bpm(all_songs[g])
    m2 = run("prac-countin", int(PLAY_MS + cin), open_song(g) + ["1500:pot:4096"] + press(PLAY_MS, "PLAY"))
    m1 = run("prac-countin-m1", int(PLAY_MS + cin), ["1500:pot:4096"] + press(OPEN_MS, "PLAY") + press(PLAY_MS, "PLAY"),
             flash=None, pkg=M1_PKG) if M1_PKG.exists() else None
    seg = lambda r: r["left"][int((PLAY_MS + 50) / 1000 * RATE):int((PLAY_MS + cin - 100) / 1000 * RATE)]
    p2 = max(abs(v) for v in seg(m2))
    p1 = max(abs(v) for v in seg(m1)) if m1 else 0
    check(m2["stats"]["click_peak_vel"] == 127 and (not m1 or p2 > 1.5 * p1),
          f"count-in clicks: beat 1 velocity {m2['stats']['click_peak_vel']}, the others 104 (M1: 112 / 88) at full "
          f"channel volume; peak {p2} vs M1's {p1}" + (f" ({p2 / p1:.1f}x)" if p1 else " (M1 package not found)"))
    rep += lines
    return ok


def key_bits(notes, oct=0):
    """MIDI notes -> the FM-1's key bits (F3 = key 0, 27 keys), folded by octaves, as ui.c key_bit"""
    bits = 0
    for n in notes:
        k = n - 53 - 12 * oct
        while k < 0:
            k += 12
        while k >= 27:
            k -= 12
        bits |= 1 << k
    return bits


def chord_at(c, tick):
    k = 0
    for i, e in enumerate(c["chord_events"]):
        if e["tick"] <= tick:
            k = i
    return k


def test_views(all_songs, rep):
    """part 3: the key (transposition), the views, the key lights, the bass names"""
    lines, ok = ["\n## Key, views, key lights (part 3)\n"], True

    def check(cond, text):
        nonlocal ok
        lines.append(f"- {'PASS' if cond else 'FAIL'}: {text}")
        ok &= bool(cond)

    slugs = [c["slug"] for c in all_songs]
    i = slugs.index("twelve-bar-blues")
    c = all_songs[i]
    o = open_song(i)
    ms = PLAY_MS + 6000
    plain = run("view-key0", ms, o + press(PLAY_MS, "PLAY"))
    up2 = run("view-key2", ms, o + press(OPEN_MS + 200, "SAVE") + [f"{OPEN_MS + 400}:e0:2"] + press(PLAY_MS, "PLAY"))
    a, b = plain["stats"], up2["stats"]
    moved = [(b["note_on_last"][l] - a["note_on_last"][l]) % 12 for l in (1, 2, 3)]
    check(b["transpose"] == 2 and moved == [2, 2, 2] and a["note_on_last"][0] == b["note_on_last"][0],
          f"SAVE, SELECT +2: the key D (+2): bass, harmony, melody sent 2 semitones up (last notes {a['note_on_last'][1:]} "
          f"-> {b['note_on_last'][1:]}), drums unchanged ({b['note_on_last'][0]}) (view-key2.png: names in D)")
    nx = run("view-key-next-song", PLAY_MS + 2000, o + press(OPEN_MS + 200, "SAVE") + [f"{OPEN_MS + 400}:e0:2"]
             + press(OPEN_MS + 900, "SAVE") + press(OPEN_MS + 1000, "HOME") + [f"{OPEN_MS + 1100}:e0:-1"]
             + press(OPEN_MS + 1200, "PLAY") + press(PLAY_MS, "PLAY"))
    check(nx["stats"]["transpose"] == 0, "another song opened: its own key again (transpose 0)")
    live = run("view-key-live", PLAY_MS + 10000, o + press(PLAY_MS, "PLAY") + press(PLAY_MS + 3000, "SAVE")
               + [f"{PLAY_MS + 3200}:e0:-3"] + press(PLAY_MS + 6000, "HOME"))
    quiet = rms(live["left"][int((PLAY_MS + 9000) / 1000 * RATE):int((PLAY_MS + 9900) / 1000 * RATE)])
    check(live["stats"]["transpose"] == -3 and quiet < 5,
          f"the key changed while playing (-3): the sounding notes ended at once; silent 3 s after HOME (rms "
          f"{quiet:.0f}): no hanging note")
    # the views (EDIT) and the chord's timing in the keyboard view
    v1 = run("view-keys", PLAY_MS + 9000, o + press(OPEN_MS + 200, "ENV") + press(OPEN_MS + 400, "EDIT")
             + press(PLAY_MS, "PLAY"))
    v2 = run("view-chart", PLAY_MS + 9000, o + press(OPEN_MS + 200, "SCL") + press(OPEN_MS + 400, "EDIT")
             + press(OPEN_MS + 700, "EDIT") + press(PLAY_MS, "PLAY"))
    v3 = run("view-chord-cue", PLAY_MS + 9000, o + press(OPEN_MS + 200, "SCL") + press(OPEN_MS + 300, "ENV")
             + press(PLAY_MS, "PLAY"))
    st = v1["stats"]
    due, drawn = st["chord_due"], st["chord_drawn"]
    late = [k for k in range(1, 256) if due[k] and (not drawn[k] or drawn[k] > due[k])]
    check(v1["stats"]["view"] == 1 and v2["stats"]["view"] == 2 and not late and sum(1 for d in due if d) > 2,
          f"EDIT: the keyboard view (view-keys.png), EDIT again: the chart (view-chart.png); in the keyboard view every "
          f"chord was on the screen before its sound ({sum(1 for d in due if d)} changes)")
    # the key lights: harmony muted -> the chord's voicing; bass muted -> the bass note; the octave shift moves them
    def voicing_bits(r, oct=0):
        t = r["stats"]["last_tick"]
        lvl = 0
        want = set()
        for lead in (0, 200, 400):                 # the chord the screen shows (drawn ahead of the sound)
            k = chord_at(c, t + lead)
            want.add(key_bits(c["chord_events"][k]["voicing"][lvl], oct))
        return want
    check(st["lit_keys"] in voicing_bits(v1) and st["lit_keys"],
          f"harmony muted: the keys lit = the chord's level-1 voicing (bits {st['lit_keys']:#09x})")
    oc = run("view-keys-oct", PLAY_MS + 9000, o + press(OPEN_MS + 100, "OCTUP") + press(OPEN_MS + 200, "ENV")
             + press(PLAY_MS, "PLAY"))
    check(oc["stats"]["lit_keys"] in voicing_bits(oc, 1) and oc["stats"]["lit_keys"] != 0,
          "with OCT+ the lit keys move down an octave, where those notes now are under the hands")
    bs = run("view-bass-lights", PLAY_MS + 9000, o + press(OPEN_MS + 200, "SCL") + press(PLAY_MS, "PLAY"))
    bits = bin(bs["stats"]["lit_keys"]).count("1")
    check(bits == 1 and bs["stats"]["note_on_last"][1] == 0,
          f"bass muted (harmony playing): one key lit, the bass note ({bs['stats']['lit_keys']:#09x}); the bass "
          f"layer sent nothing (view-chord-cue.png: BASS name and the rhythm cue)")
    rep += lines
    return ok


def test_levels(all_songs, rep):
    """part 2 item 7: every song at level 2 on every layer, MASTER 100 %: the peak, and samples at full scale"""
    lines, ok = ["\n## Levels: every song at L2, MASTER 100 % (part 2)\n"], True
    for i, c in enumerate(all_songs):
        ms = int(PLAY_MS + min(song_ms(c, count_in=False), 40000) + 300)
        script = open_song(i) + ["1500:pot:4096"] + press(OPEN_MS + 300, "ARP") + \
            [f"{OPEN_MS + 400}:e2:1", f"{OPEN_MS + 420}:e3:1", f"{OPEN_MS + 440}:e4:1"] + press(PLAY_MS, "PLAY")
        r = run(f"levels-{c['slug']}", ms, script)
        raw = (OUT / "runs" / f"levels-{c['slug']}.raw").read_bytes()
        x = struct.unpack(f"<{len(raw) // 2}h", raw)
        peak = max(abs(v) for v in x)
        full = sum(1 for v in x if abs(v) >= 32767)
        lines.append(f"- {'PASS' if not full else 'FAIL'}: {c['slug']}: peak {peak} ({peak / 327.68:.0f} % of full "
                     f"scale), {full} samples at full scale")
        ok &= not full
    rep += lines
    return ok


def test_flash(c, rep):
    lines, ok = ["\n## Flash chip ids (Amazing Grace)\n"], True
    script = press(OPEN_MS, "PLAY") + press(PLAY_MS, "PLAY")
    runs = {fid: run(f"flash-{fid}", PLAY_MS + 4000, script, flash_id=fid) for fid in ("856014", "b36014")}
    lst = {fid: run(f"flash-list-{fid}", 3500, [], flash_id=fid) for fid in ("856014", "b36014", "c84017")}
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
    ap.add_argument("--out", help="the work folder (default build/emu/m2); two runs at once need two folders")
    a = ap.parse_args()
    global PKG, OUT
    if a.pkg:
        PKG = Path(a.pkg).resolve()
    if a.out:
        OUT = Path(a.out).resolve()
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
        if not a.only or a.only == "boot":
            reports["boot"] = []
            futs["boot"] = pool.submit(test_boot, reports["boot"])
        if not a.only or a.only == "practice":
            reports["practice"] = []
            futs["practice"] = pool.submit(test_practice, all_songs, reports["practice"])
        if not a.only or a.only == "views":
            reports["views"] = []
            futs["views"] = pool.submit(test_views, all_songs, reports["views"])
        if not a.only or a.only == "levels":
            reports["levels"] = []
            futs["levels"] = pool.submit(test_levels, all_songs, reports["levels"])
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
