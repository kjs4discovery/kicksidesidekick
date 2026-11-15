#!/usr/bin/env python3
# m5_test.py -- Milestone M5: headless emulator tests of the firmware (grown from tools/m4_test.py, m3_test.py,
# m2_test.py). Never touches the FM-1.
# Licence: TBD (see D017)
#
#   .venv/bin/python -I tools/m4_test.py [--jobs N] [--only GROUP[,GROUP..]] [--pkg PKG] [--out DIR]
#   GROUP: a song's slug, store, flash, boot, practice, views, levels, part1 .. part9 (each M3 part's own checks)
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
OUT = ROOT / "build/emu/m5"
SEND = f"{ROOT}/tools/fm1song.sh"
AREA = (0x97000, 0xDC000)                      # the song store (firmware/app/store.h)
FLASH = {}                                     # the prepared flash images: "empty", "songs"
LCD = (370, 42, 850, 522)                      # the LCD in fm1-live's panel image (shown 2x)
RATE = 44100
BLOCK = 256
OPEN_MS, PLAY_MS = 4400, 6200                  # after the splash (M5: 4 s): open the song, then PLAY (M4: room for a hold)
LAYERS_BTN = ("FX", "SCL", "ENV", "LFO")         # the layer buttons: drums, bass, harmony, melody
HOLD = 1000                                      # a hold that makes a layer your part (the firmware: 900 ms)
STATS = struct.Struct("<14I64I64I64i4I4I4I3I64H64B256I256I" + "iiIIiIIIIiIIIi4I3I")   # PL_STATS_TEMPOS 64
STATS4 = struct.Struct("<17I")                  # version 4 (M3): part key_flashes mid_keys loop_step settings saves
                                               # notes_guided folder
FIELDS = ("magic version blocks render_max budget starts start_frames ends end_frames last_tick loops loop_from "
          "loop_to ntempo").split()
FIELDS2 = ("start_from start_tick pauses pause_frames pause_tick bpm clicks click_peak_vel key_note_last keys_octave "
           "ui_view flash_bars flash_beats transpose").split()
M1_PKG = ROOT / "build/step-M1/fm1acc-0.2.0-m1.fwsc"   # for before / after numbers (its songs are built in)
M2_PKG = ROOT / "build/step-M2/final/fm1acc-0.3.0-m2.fwsc"
M2_FLASH = ROOT / "build/step-M2/final/emu/flash/songs.bin"   # M2's app and its songs (fm1-live runs the flash's app)
M3_PKG = ROOT / "build/step-M3/final/fm1acc-0.4.0-m3.fwsc"
M3_FLASH = ROOT / "build/step-M3/final/emu/flash/songs.bin"
M4_PKG = ROOT / "build/step-M4/final/fm1acc-0.5.0-m4.fwsc"
M4_FLASH = ROOT / "build/step-M4/final/emu/flash/songs.bin"


# M10 (decisions-M10.md M10-03, M10-04): songs held back from the suites, and a private song no test may read
HELD_BACK = {}                                    # M10-03: camptown-races is fixed (the planning chat, 2026-11-14); none held back
PRIVATE_SKIP = {"hallelujah"}                     # (the user's copyrighted import: no test reads it)


def song_dirs():
    return sorted(p for p in (ROOT / "songs").iterdir() if (p / "song.yaml").is_file() and p.name not in HELD_BACK)


def private_dirs():
    d = ROOT / "songs/private"
    return sorted(p for p in d.glob("*/") if (p / "song.yaml").is_file() and p.name not in PRIVATE_SKIP) if d.is_dir() else []


def songs():
    out = []
    for f in song_dirs():
        c = read_compiled(compile_folder(f)[0])
        out.append(c)
    out.sort(key=lambda c: c["title"].lower())       # the firmware's list order (tools/songs_to_c.py)
    return out


def press(ms, name):
    return [f"{ms}:{name}", f"{ms + 100}:{name}^"]


def m3_presses(script):
    """M3's tests press a layer button to mute it and make it your part. M4 (D027): hold it (your part), then press it
    twice (FULL -> QUIET -> MUTED). An EDIT during that is moved after it (the views follow the part)"""
    out, busy = [], []
    for ev in sorted(script, key=lambda e: int(e.split(":")[0])):
        t, rest = ev.split(":", 1)
        t = int(t)
        if rest in LAYERS_BTN:
            out += [f"{t}:{rest}", f"{t + HOLD}:{rest}^", f"{t + HOLD + 60}:{rest}", f"{t + HOLD + 120}:{rest}^",
                    f"{t + HOLD + 180}:{rest}", f"{t + HOLD + 240}:{rest}^"]
            busy.append((t, t + HOLD + 300))
            continue
        if rest.rstrip("^") in LAYERS_BTN and rest.endswith("^"):
            continue                                 # (its release is in the expansion)
        if rest.rstrip("^") == "EDIT":
            for a, b in busy:
                if a <= t < b:
                    t = b + (t - a)
        out.append(f"{t}:{rest}")
    return sorted(out, key=lambda e: int(e.split(":")[0]))


def press_raw(ms, name):
    return [f"{ms}:{name}!", f"{ms + 100}:{name}^!"]


def hold(ms, name):
    """M4: hold a layer button (your part)"""
    return [f"{ms}:{name}!", f"{ms + HOLD}:{name}^!"]


def tap(ms, name, n=1):
    """M4: press a layer button n times (its volume: FULL -> QUIET -> MUTED -> FULL)"""
    return [x for k in range(n) for x in (f"{ms + 150 * k}:{name}!", f"{ms + 150 * k + 70}:{name}^!")]


MODE_MS = 4050                                  # M5 part 3: the mode screen after the splash: PLAY here picks LEARN


def run(name, ms, script, flash_id=None, flash="songs", host=None, cut=None, pkg=None, mode="LEARN"):
    """one emulator run. flash: a prepared image's name ("songs", "empty"), a path, or None (a blank chip); host: a
    command playing the laptop over USB (FM1_USB_HOST; the run ends soon after it does); cut: FM1_FLASH_CUT. Layer
    presses written M3's way are translated (m3_presses); events marked "!" (hold, tap) are M4's own"""
    script = m3_presses([e for e in script if not e.endswith("!")]) + [e[:-1] for e in script if e.endswith("!")]
    if mode == "LEARN" and not (pkg and "m4" in Path(str(pkg)).name):   # (M4's packages have no mode screen)
        script += [f"{MODE_MS}:PLAY", f"{MODE_MS + 60}:PLAY^"]
    script = sorted(script, key=lambda e: int(e.split(":")[0]))
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
    folders = " ".join(str(f) for f in song_dirs())
    r = run("send-all", 120000, [], flash="empty", host=f"{SEND} send --emu-pipe {folders}")
    shutil.copy(r["flash"], d / "songs-hint.bin")
    FLASH["songs_hint"] = d / "songs-hint.bin"     # (M5: the controls hint on, as the FM-1 comes)
    FLASH["songs"] = d / "songs-hint.bin"
    # M5: the controls hint off (settings: the last row) for every other test, so a song opens as before
    h = run("hint-off", 7500, press(4600, "GLO") + press(4800, "GLO") + press(6200, "GLO"))   # (M10.2: no longer turns the setting off: S_HINT is Pop-ups, ON)
    shutil.copy(h["flash"], d / "songs.bin")
    FLASH["songs"] = d / "songs.bin"
    f = bytearray((d / "songs.bin").read_bytes())  # M5: an empty song area with the hint off (the settings kept)
    f[AREA[0]:AREA[1]] = b"\xff" * (AREA[1] - AREA[0])
    (d / "empty-quiet.bin").write_bytes(f)
    FLASH["empty_quiet"] = d / "empty-quiet.bin"
    return r


def records(path, dead=False):
    """the song store's live records in a flash image: [(offset, file length)] (store.h's layout; M5: blocks of 512
    bytes, "FM2R" heads with their CRC over all but the magic, deleted ones with the magic zeroed; M4's "FM1R" on
    whole sectors). dead: the deleted ones instead"""
    f = Path(path).read_bytes()
    out, off = [], AREA[0]
    while off < AREA[1]:
        m, n = f[off:off + 4], struct.unpack_from("<I", f, off + 8)[0]
        crc2 = zlib.crc32(f[off + 4:off + 28]) == struct.unpack_from("<I", f, off + 28)[0]
        if m in (b"FM2R", bytes(4)) and crc2 and 32 <= n <= 256 * 1024:
            span = (32 + n + 511) // 512
            span = span if span <= 8 else (span + 7) // 8 * 8
            if (m == b"FM2R") != dead:
                out.append((off, n))
            off += span * 512
        elif m == b"FM1R" and zlib.crc32(f[off:off + 28]) == struct.unpack_from("<I", f, off + 28)[0] and \
                (off - AREA[0]) % 4096 == 0:
            if not dead:
                out.append((off, n))
            off += (32 + n + 4095) // 4096 * 4096
        else:
            off += 512
    return out


def stats(ram):
    i = ram.find(b"PLST")
    while i >= 0 and struct.unpack_from("<I", ram, i + 4)[0] not in (3, 4):
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
    s["part"], s["key_flashes"], s["mid_keys"], s["loop_step"], s["settings"], s["settings_saves"] = 0xFF, 0, 0, 0, 0, 0
    s["notes_guided"], s["folder"], s["hints"], s["splash_notes"] = 0, 0, 0, 0
    if s["version"] >= 4:
        (s["part"], s["key_flashes"], s["mid_keys"], s["loop_step"], s["settings"], s["settings_saves"],
         s["notes_guided"], s["folder"], s["hints"], s["splash_notes"], s["mode"], s["synth_notes"],
         s["synth_voices_max"], s["synth_preset"], s["arp_steps"], s["synth_peak"],
         s["synth_render_max"]) = STATS4.unpack_from(ram, i + STATS.size)
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
    return ([f"4150:e0:{i}"] if i else []) + press(OPEN_MS, "PLAY")


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
    count_in_ms = (bpb + c["pickup_beats"]) * 60000 / own_bpm(c) if c["pickup_beats"] else bpb * 60000 / own_bpm(c)
    play_from = int((PLAY_MS + count_in_ms + 300) / 1000 * RATE)
    check(rms(full["left"][play_from:play_from + RATE * 4]) > 50, "sound while playing (after the count-in)")

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

    # ---- loop (M3: bars A-B, set while playing: SEQ takes the bar playing as A, SEQ, PRESETS +1 moves B to its
    # section's last bar, SEQ: on)
    bt = bpb * c["ppq"]
    t_set = PLAY_MS + 3000
    tick = start_tick + (3000 + 30) / 1000 * c["ppq"] * own_bpm(c) / 60   # (ARP: the count-in off)
    a_bar = max(int(tick // bt), c["sections"][0]["first_bar"])
    sec_of = lambda bar: next(x for x in c["sections"] if x["first_bar"] <= bar < x["first_bar"] + x["bars"])
    sb = sec_of(min(a_bar + 3, c["bar_count"] - 1))
    b_bar = sb["first_bar"] + sb["bars"] - 1
    a, b = a_bar * bt, (b_bar + 1) * bt
    run_ms = t_set + ticks_to_frames(c, b - tick + 0.4 * (b - a)) / RATE * 1000
    script = open_song(i) + press(OPEN_MS + 300, "ARP") + press(PLAY_MS, "PLAY") + press(t_set - 200, "SEQ") \
        + press(t_set, "SEQ") \
        + press(t_set + 150, "SEQ") + [f"{t_set + 300}:e6:1"] + press(t_set + 700, "SEQ")
    lr = run(f"{slug}-loop", int(run_ms), script)
    lo = lr["stats"]
    check(lo["loops"] >= 1 and lo["loop_from"] == a and lo["loop_to"] == b and lo["ends"] == 0,
          f"loop A-B set while playing (A = the bar playing, B = its section's last bar): {lo['loops']} jump(s) from "
          f"tick {lo['loop_to']} to {lo['loop_from']} (expected {b} -> {a}: bars {a_bar}..{b_bar})")
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
          f"each stored byte for byte ({sum(len(b) for b in want.values())} bytes, format "
          f"1.{'/1.'.join(str(v) for v in sorted({b[5] for b in want.values()}))})")
    li = run("store-list", 30000, [], host=f"{SEND} list --emu-pipe")
    check(all(re.search(rf"{s} +\d+ bytes +\d+ (?:sectors|blocks) +ok", li["out"]) for s in slugs) and li["ops"] == 0,
          f"fm1song list: every song, ok; nothing written by listing")
    lst = run("store-songs", 4500, [])
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
    j1 = run("store-junk", 4500, [], flash=junk)
    j2 = run("store-junk-send", 60000, [], flash=junk, host=f"{SEND} send --emu-pipe {ROOT}/songs/saints")
    j3 = run("store-junk-after", 4500, [], flash=j2["flash"])
    check(not j1["halt"] and j1["ops"] == 0 and "saints: stored" in j2["out"] and len(records(j2["flash"])) == 1,
          f"junk in the song area (another firmware's leftovers): no songs listed, nothing written; a song sent over "
          f"it is stored ({j2['ops']} erases / programs) and listed (store-junk.png, store-junk-after.png)")

    # a stored song damaged: a byte flipped in its events
    recs = records(FLASH["songs"])
    dmg = OUT / "runs" / "damaged-src.flash"
    f = bytearray(FLASH["songs"].read_bytes())
    off, n = recs[0]
    victim = read_compiled(bytes(f[off + 32:off + 32 + n]))["slug"]
    f[off + 32 + n // 2] ^= 0x10       # (M10: the file ends with the INFO chunk: a flip there fails the list's peek, not the open)
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
          and records(rf["flash"]) == records(FLASH["songs"]),
          "damaged files sent unchecked (--no-validate): both refused by the FM-1 (bad CRC; a missing chunk), not "
          "stored, every song as it was (M5: received into the flash, checked there; no head written)")

    # the laptop stops answering after the first read (M5: a 2.0 song is about 2-3 reads of 512 bytes)
    good_path = OUT / "runs" / "good.fm1s"
    good_path.write_bytes(good)
    st = run("store-stall", 60000, [], host=f"{sys.executable} -I {ROOT}/tools/m2_host.py stall {good_path} 1")
    check("result 3" in st["out"] and records(st["flash"]) == records(FLASH["songs"]),
          "the laptop stops answering half way: the FM-1 gives up (transfer stopped), nothing stored")
    st2 = run("store-stall-after", 30000, [], flash=st["flash"], host=f"{SEND} list --emu-pipe")
    check(all(re.search(rf"{s} +\d+ bytes +\d+ (?:sectors|blocks) +ok", st2["out"]) for s in slugs),
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
        intact = all(re.search(rf"{s} +\d+ bytes +\d+ (?:sectors|blocks) +ok", c2["out"]) for s in others)
        lastok = re.search(rf"{last_slug} +\d+ bytes +\d+ (?:sectors|blocks) +(\S+)", c2["out"])
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
    li = run("boot-list", 4300, [])
    cream = sp["lcd"].getpixel((20, 20))
    check(sp["stats"]["ui_view"] >= 16 and cream[0] > 200 and cream[2] < 230 and li["stats"]["ui_view"] == 0,
          f"the splash (the logo, boot-splash.png) is still up at 2.8 s; the song list at 3.3 s (boot-list.png)")
    pk = run("boot-play-in-splash", 4300, press(2000, "PLAY"))
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
    # (M3: the loops are part 3's: tools/m3_test.py --only part3; the per-song run sets one while playing)
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
    r11 = run("prac-oct-up", 7000, press(4600, "OCTUP") + press(4800, "OCTUP") + ["5200:C4", "5400:C4^"])
    r12 = run("prac-oct-down", 7000, [f"{4600 + 150 * n}:OCTDN" for n in range(5)] +
              [f"{4650 + 150 * n}:OCTDN^" for n in range(5)] + ["5600:C4", "5800:C4^"])
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
    keys1 = st["lit_keys"] | st["mid_keys"]                # (M3: bright at an attack, the middle level between)
    check(keys1 in voicing_bits(v1) and keys1,
          f"harmony muted: the keys lit = the chord's level-1 voicing (bits {st['lit_keys']:#09x})")
    oc = run("view-keys-oct", PLAY_MS + 9000, o + press(OPEN_MS + 100, "OCTUP") + press(OPEN_MS + 200, "ENV")
             + press(PLAY_MS, "PLAY"))
    keys2 = oc["stats"]["lit_keys"] | oc["stats"]["mid_keys"]
    check(keys2 in voicing_bits(oc, 1) and keys2 != 0,
          "with OCT+ the lit keys move down an octave, where those notes now are under the hands")
    bs = run("view-bass-lights", PLAY_MS + 9000, o + press(OPEN_MS + 200, "SCL") + press(PLAY_MS, "PLAY"))
    bits = bin(bs["stats"]["lit_keys"] | bs["stats"]["mid_keys"]).count("1")
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
    lst = {fid: run(f"flash-list-{fid}", 4500, [], flash_id=fid) for fid in ("856014", "b36014", "c84017")}
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


# ------------------------------------------------------------------------------------------------ M3, per part
def checker(title):
    lines, state = [f"\n## {title}\n"], {"ok": True}

    def check(cond, text):
        lines.append(f"- {'PASS' if cond else 'FAIL'}: {text}")
        state["ok"] &= bool(cond)
    return lines, state, check


def drums_only(i, lvl, name, pkg=None):
    """drums alone at level lvl (bass, harmony, melody muted), MASTER 100 %, count-in off: (rms, drum note-ons)"""
    s = open_song(i) + ["1500:pot:4096"] + press(OPEN_MS + 200, "SCL") + press(OPEN_MS + 300, "ENV") \
        + press(OPEN_MS + 400, "LFO") + press(OPEN_MS + 500, "ARP") + ([f"{OPEN_MS + 600}:e2:1"] if lvl == 2 else [])
    r = run(name, PLAY_MS + 8000, s + press(PLAY_MS, "PLAY"), pkg=pkg)
    return rms(r["left"][int((PLAY_MS + 1500) / 1000 * RATE):]), r["stats"]["notes_on"][0]


def test_part1(all_songs, rep):
    """M3 part 1: the bugs (on Greensleeves, Oh! Susanna, the Blues)"""
    lines, st, check = checker("Part 1: bugs")
    slugs = [c["slug"] for c in all_songs]
    for slug in ("greensleeves", "oh-susanna"):
        i = slugs.index(slug)
        a, na = drums_only(i, 1, f"p1-drums1-{slug}")
        b, nb = drums_only(i, 2, f"p1-drums2-{slug}")
        g, ng = drums_only(slugs.index("amazing-grace"), 1, f"p1-drums1-grace-{slug}")
        check(a > 0.8 * g and na > 0 and nb > na, f"{slug}: drums alone at level 1: rms {a:.0f} ({na} hits in 8 s; "
              f"Amazing Grace's level 1: {g:.0f}), level 2: rms {b:.0f} ({nb} hits): a backbeat at L1, busier at L2")
    i = slugs.index("twelve-bar-blues")
    c = all_songs[i]
    bt = int(c["time_signature"][0]) * c["ppq"]
    beat_ms = 60000 / own_bpm(c)
    o = open_song(i)
    r = run("p1-beat-light", PLAY_MS + 8000, o + press(PLAY_MS, "PLAY"))
    s = r["stats"]
    beats = 8000 / beat_ms
    check(abs(s["flash_bars"] + s["flash_beats"] - beats) <= 2 and abs(s["flash_bars"] - beats / 4) <= 1,
          f"beat light: a full flash on every beat ({s['flash_bars']} on beat 1, longer, + {s['flash_beats']} on the "
          f"others in 8 s at {own_bpm(c)} BPM, ~{beats:.0f} beats)")
    # loop bars, a 2-bar loop played past its end: one A, one B on the strip (p1-loop-marks.png); then the loop off:
    # the start point is the one set before (bar 3), not the loop's A
    # (M4's SEQ cycle: off -> song -> set A -> set B -> A-B on -> off; the start point set to bar 3 first)
    ab = o + [f"{OPEN_MS + 100}:e1:2"] + press(OPEN_MS + 400, "SEQ") + press(OPEN_MS + 550, "SEQ") \
        + [f"{OPEN_MS + 700}:e1:2"] + press(OPEN_MS + 1000, "SEQ") + [f"{OPEN_MS + 1150}:e1:-2"] \
        + press(OPEN_MS + 1400, "SEQ") + press(OPEN_MS + 1550, "ARP")
    loop_ms = 2 * bt * 60000 / (c["ppq"] * own_bpm(c))
    run("p1-loop-marks", int(PLAY_MS + loop_ms * 1.6), ab + press(PLAY_MS, "PLAY"))
    off = ab + press(OPEN_MS + 1700, "SEQ") + press(OPEN_MS + 2400, "PLAY")
    r = run("p1-loop-off", OPEN_MS + 3900, off)
    want = (c["sections"][0]["first_bar"] + 2) * bt
    check(r["stats"]["start_from"] == want, f"LOOP BARS with A moved to bar 5, then the loop off: PLAY starts at the "
          f"start point set before, bar 3 (tick {r['stats']['start_from']}, expected {want}); the strip shows one A and "
          f"one B (p1-loop-marks.png), no START BAR left over (p1-loop-off.png)")
    # the keyboard view after a pause, count-in on: the count is shown (p1-keys-countin.png)
    r = run("p1-keys-countin", PLAY_MS + 6000 + int(beat_ms * 1.5), o + press(OPEN_MS + 200, "EDIT")
            + press(PLAY_MS, "PLAY") + press(PLAY_MS + 4000, "PLAY") + press(PLAY_MS + 6000, "PLAY"))
    px = r["lcd"].crop((0, 64, 480, 200)).getcolors(1 << 16) or []
    amber = sum(n for n, (rr, gg, bb) in px if rr > 200 and 120 < gg < 200 and bb < 60)
    check(r["stats"]["view"] == 1 and amber > 500, f"keyboard view, paused, PLAY with count-in: the count over the view "
          f"({amber} amber pixels: the big number; p1-keys-countin.png)")
    rep += lines
    return st["ok"]


def attacks(c, layer, var, t0, t1):
    """the layer's attack ticks (note-on ticks, once each) in t0 <= tick < t1"""
    return sorted({e[0] for e in c["layers"][layer]["variants"][var] if e[1] == 144 and t0 <= e[0] < t1})


def last_attack_notes(c, layer, var, tick):
    """the notes of the layer's last attack at or before tick"""
    ev = [e for e in c["layers"][layer]["variants"][var] if e[1] == 144 and e[0] <= tick]
    if not ev:
        return []
    a = max(e[0] for e in ev)
    return [e[2] for e in ev if e[0] == a]


def test_part2(all_songs, rep):
    """M3 part 2: follow your part (all four layers), blinking key lights, the cue always shown"""
    lines, st, check = checker("Part 2: follow your part")
    slugs = [c["slug"] for c in all_songs]
    g, d, sa = slugs.index("amazing-grace"), slugs.index("danny-boy"), slugs.index("saints")
    cg, cd = all_songs[g], all_songs[d]
    lead = lambda c, ms: int(ms / 1000 * c["ppq"] * own_bpm(c) / 60)
    # the harmony muted: the keys blink, bright at each of its attacks (Amazing Grace, L1: held chords; L2: waltz)
    for lvl in (1, 2):
        sc = open_song(g) + press(OPEN_MS + 200, "ENV") + ([f"{OPEN_MS + 300}:e4:1"] if lvl == 2 else []) \
            + press(PLAY_MS, "PLAY")
        r = run(f"p2-harm-blink{lvl}", PLAY_MS + 9000, sc)
        s = r["stats"]
        want = attacks(cg, "harmony", lvl, s["start_from"], s["last_tick"] + lead(cg, 30))
        check(s["part"] == 2 and abs(s["key_flashes"] - len(want)) <= 1 and (s["lit_keys"] or s["mid_keys"]),
              f"harmony muted, L{lvl}: your part = harmony; the key lights flashed {s['key_flashes']} times for "
              f"{len(want)} attacks of the harmony at L{lvl} (bright at each, darker between: lit {s['lit_keys']:#x}, "
              f"between {s['mid_keys']:#x})")
    # the bass muted (Danny Boy): the screen, the views and the lights follow the bass
    base = open_song(d) + press(OPEN_MS + 200, "SCL") + press(OPEN_MS + 300, "ARP")
    r = run("p2-bass", PLAY_MS + 7000, base + press(PLAY_MS, "PLAY"))
    s = r["stats"]
    note = min(last_attack_notes(cd, "bass", 1, s["last_tick"] + lead(cd, 30)) or [0])
    keys = s["lit_keys"] | s["mid_keys"]
    check(s["part"] == 1 and keys == key_bits([note]) and s["dim_keys"] == 0,
          f"bass muted: your part = bass; one key lit, its note now ({note}: bits {keys:#x}); the chord view shows the "
          f"bass note, its next, the bars' bass notes (p2-bass.png)")
    run("p2-bass-keys", PLAY_MS + 7000, base + press(OPEN_MS + 500, "EDIT") + press(PLAY_MS, "PLAY"))
    run("p2-bass-chart", PLAY_MS + 7000, base + press(OPEN_MS + 500, "EDIT") + press(OPEN_MS + 700, "EDIT")
        + press(PLAY_MS, "PLAY"))
    # the melody muted (Amazing Grace)
    base = open_song(g) + press(OPEN_MS + 200, "LFO") + press(OPEN_MS + 300, "ARP")
    r = run("p2-melody", PLAY_MS + 7000, base + press(PLAY_MS, "PLAY"))
    s = r["stats"]
    note = max(last_attack_notes(cg, "melody", 1, s["last_tick"] + lead(cg, 30)) or [0])
    keys = s["lit_keys"] | s["mid_keys"]
    want = attacks(cg, "melody", 1, s["start_from"], s["last_tick"] + lead(cg, 30))
    check(s["part"] == 3 and keys == key_bits([note]) and abs(s["key_flashes"] - len(want)) <= 1,
          f"melody muted: your part = melody; its note's key lit ({note}), {s['key_flashes']} flashes for "
          f"{len(want)} melody notes (p2-melody.png)")
    run("p2-melody-keys", PLAY_MS + 7000, base + press(OPEN_MS + 500, "EDIT") + press(PLAY_MS, "PLAY"))
    # the drums muted (Saints): no key lights; the drum names and the bars' hits
    base = open_song(sa) + press(OPEN_MS + 200, "FX") + press(OPEN_MS + 300, "ARP")
    r = run("p2-drums", PLAY_MS + 5000, base + press(PLAY_MS, "PLAY"))
    s = r["stats"]
    r2 = run("p2-drums-chart", PLAY_MS + 5000, base + press(OPEN_MS + 500, "EDIT") + press(PLAY_MS, "PLAY"))
    check(s["part"] == 0 and not (s["lit_keys"] | s["mid_keys"] | s["dim_keys"]) and r2["stats"]["view"] == 2,
          "drums muted: your part = drums; no key lights; EDIT skips the keyboard view (to the chart: the bars' hits) "
          "(p2-drums.png, p2-drums-chart.png)")
    # several muted: the last one muted; unmuted again: the one before
    r = run("p2-two", PLAY_MS + 3000, open_song(g) + press(OPEN_MS + 200, "ENV") + press(OPEN_MS + 400, "SCL")
            + press(PLAY_MS, "PLAY"))
    r2 = run("p2-two-back", PLAY_MS + 3000, open_song(g) + press(OPEN_MS + 200, "ENV") + press(OPEN_MS + 400, "SCL")
             + press(OPEN_MS + 600, "SCL") + press(PLAY_MS, "PLAY"))
    check(r["stats"]["part"] == 1 and r2["stats"]["part"] == 2,
          "harmony muted, then bass: your part = bass (the last muted; p2-two.png: YOURS on BASS, MUTE on HARM); the "
          "bass on again: the harmony")
    # none muted: the cue is shown (harmony's), the keys stay dark
    r = run("p2-none", PLAY_MS + 5000, open_song(g) + press(OPEN_MS + 300, "ARP") + press(PLAY_MS, "PLAY"))
    s = r["stats"]
    check(s["part"] == 0xFF and not (s["lit_keys"] | s["mid_keys"]), "nothing muted: no part of yours, the keys dark; "
          "the rhythm cue still shown, right of the chord (p2-none.png)")
    rep += lines
    return st["ok"]


def test_part3(all_songs, rep):
    """M3 part 3: loops redesigned: bars A-B only, set and confirmed step by step (SEQ), highlighted (the Blues, Saints)"""
    lines, st, check = checker("Part 3: loops")
    slugs = [c["slug"] for c in all_songs]
    i = slugs.index("twelve-bar-blues")
    c = all_songs[i]
    bt = int(c["time_signature"][0]) * c["ppq"]
    bar_ms = bt * 60000 / (c["ppq"] * own_bpm(c))
    o = open_song(i) + press(OPEN_MS + 100, "ARP")
    O = OPEN_MS
    # stopped: SEQ (set A), ALGORITHM +2 (bar 3), SEQ (A set; B = A + 3 = bar 6), ALGORITHM -1 (bar 5), SEQ (on)
    seta = o + press(O + 150, "SEQ") + press(O + 300, "SEQ") + [f"{O + 500}:e1:2"]   # (M4: SEQ, SEQ: set A)
    run("p3-set-a", O + 1100, seta)
    setb = seta + press(O + 1100, "SEQ") + [f"{O + 1300}:e1:-1"]
    rb = run("p3-set-b", O + 1900, setb)
    on = setb + press(O + 1900, "SEQ")
    r = run("p3-loop-on", int(O + 2400 + 3.3 * bar_ms), on + press(O + 2400, "PLAY"))
    s = r["stats"]
    check(rb["stats"]["loop_step"] == 2 and s["loop_step"] == 0 and s["start_from"] == 2 * bt and s["loops"] >= 1
          and s["loop_from"] == 2 * bt and s["loop_to"] == 5 * bt and 2 * bt <= s["last_tick"] < 5 * bt,
          f"stopped: SEQ, ALGORITHM to bar 3, SEQ (A set: p3-set-a.png), ALGORITHM to bar 5, SEQ (B set: on; "
          f"p3-set-b.png): PLAY starts at A (tick {s['start_from']}), jumps from the end of bar 5 back to bar 3 "
          f"({s['loops']} jump(s), {s['loop_to']} -> {s['loop_from']}); the loop's bars filled (p3-loop-on.png)")
    # not active while being set: PLAY during step 2 starts at the start point, no jump
    r = run("p3-not-yet", int(O + 2400 + 5.5 * bar_ms), setb + press(O + 2400, "PLAY"))
    check(r["stats"]["loops"] == 0 and r["stats"]["start_from"] == 0, "while A and B are being set the loop is not on "
          f"yet: PLAY starts at the start point, no jump ({r['stats']['loops']} jumps)")
    # HOME steps back: at B -> A, at A -> cancelled (no loop)
    r = run("p3-home", O + 2600, setb + press(O + 1900, "HOME") + press(O + 2100, "HOME") + press(O + 2400, "PLAY"))
    check(r["stats"]["loop_step"] == 0 and r["stats"]["start_from"] == 0 and r["stats"]["ui_view"] == 1,
          "HOME while setting: B back to A, then cancelled (no loop; still on the play screen)")
    # SEQ with the loop on: off; PLAY starts at the start point again
    r = run("p3-off", O + 3000, on + press(O + 2300, "SEQ") + press(O + 2600, "PLAY"))
    check(r["stats"]["start_from"] == 0 and r["stats"]["loops"] == 0, "SEQ with the loop on: the loop off; PLAY "
          "starts at the start point (bar 1)")
    # PRESETS while setting: A to a section's first bar, B to a section's last (Saints: 16-bar sections)
    k = slugs.index("saints")
    cs = all_songs[k]
    bts = int(cs["time_signature"][0]) * cs["ppq"]
    sec = cs["sections"]
    o2 = open_song(k) + press(O + 100, "ARP") + press(O + 150, "SEQ") + press(O + 300, "SEQ") + [f"{O + 500}:e6:1"] + press(O + 900, "SEQ") \
        + [f"{O + 1100}:e6:1"] + press(O + 1500, "SEQ")
    r = run("p3-sections", O + 2600, o2 + press(O + 2000, "PLAY"))
    a_want = sec[1]["first_bar"]
    b_want = sec[1]["first_bar"] + sec[1]["bars"] - 1
    check(r["stats"]["start_from"] == a_want * bts, f"PRESETS while setting: A to section 2's first bar ({a_want}), "
          f"B to its last ({b_want}); PLAY starts at A (tick {r['stats']['start_from']}, expected {a_want * bts})")
    # while playing: SEQ takes the bar playing as A; SEQ, SEQ: A..A+3, jumped into at the next bar line if outside
    r = run("p3-playing", int(PLAY_MS + 9 * bar_ms), o + press(PLAY_MS, "PLAY") + press(int(PLAY_MS + 1.4 * bar_ms), "SEQ")
            + press(int(PLAY_MS + 1.5 * bar_ms), "SEQ")
            + press(int(PLAY_MS + 1.6 * bar_ms), "SEQ") + press(int(PLAY_MS + 1.7 * bar_ms), "SEQ"))
    s = r["stats"]
    check(s["loops"] >= 1 and s["loop_to"] - s["loop_from"] == 4 * bt and s["loop_from"] in (bt, 2 * bt),
          f"while playing: SEQ three times in bar 2: the loop is bars 2-5 ({s['loop_from']}..{s['loop_to']}), "
          f"{s['loops']} jump(s) back to A")
    rep += lines
    return st["ok"]


def settings_script(t0, rows):
    """GLO, GLO (the settings), then for each (row, clicks): SELECT to the row, ALGORITHM clicks; GLO (saved, back)"""
    sc = press(t0, "GLO") + press(t0 + 200, "GLO")
    t, row = t0 + 400, 0
    for r, n in rows:
        if r != row:
            sc.append(f"{t}:e0:{r - row}")
            t += 300
            row = r
        sc.append(f"{t}:e1:{n}")
        t += 300
    return sc + press(t, "GLO"), t + 200


def test_part4(all_songs, rep):
    """M3 part 4: legend, layout, zoomed keyboard, count-in everywhere, the beep, inversions, settings"""
    lines, st, check = checker("Part 4: screen polish, count-in, inversions, settings")
    slugs = [c["slug"] for c in all_songs]
    g, d = slugs.index("amazing-grace"), slugs.index("danny-boy")
    cg = all_songs[g]
    bt = int(cg["time_signature"][0]) * cg["ppq"]
    beat_ms = 60000 / own_bpm(cg)
    # the count-in: a beep, much louder than M2's clicks (MASTER 100 %, Amazing Grace, 1 bar + its pickup)
    cin = (3 + 1) * beat_ms
    seg = lambda r: r["left"][int((PLAY_MS + 50) / 1000 * RATE):int((PLAY_MS + cin - 100) / 1000 * RATE)]
    m3 = run("p4-countin", int(PLAY_MS + cin), open_song(g) + ["1500:pot:4096"] + press(PLAY_MS, "PLAY"))
    m2 = run("p4-countin-m2", int(PLAY_MS + cin), open_song(g) + ["1500:pot:4096"] + press(PLAY_MS, "PLAY"),
             pkg=M2_PKG, flash=str(M2_FLASH)) if M2_FLASH.exists() else None
    p3, r3 = max(abs(v) for v in seg(m3)), rms(seg(m3))
    p2, r2 = (max(abs(v) for v in seg(m2)), rms(seg(m2))) if m2 else (0, 0)
    check(m3["stats"]["click_peak_vel"] == 127 and (not m2 or r3 > 2 * r2),
          f"count-in beep (square lead, C6 / G5, 70 ms): peak {p3}, rms {r3:.0f}; M2's wood-block clicks: peak {p2}, "
          f"rms {r2:.0f}" + (f" ({r3 / r2:.1f}x the energy level)" if r2 else ""))
    # settings: blink off, count-in 2 bars, volume LOW; saved; kept over a restart (the same flash)
    sc, t = settings_script(OPEN_MS + 200, [(0, 1), (1, 1), (2, -1)])   # (M4: from MEDIUM, the new default)
    r1 = run("p4-settings", t + 600, open_song(g) + sc)
    run("p4-settings-screen", t - 300, open_song(g) + sc[:-2])
    want = 0 | 1 << 4 | 0 << 8 | 1 << 12 | 1 << 20 | 1 << 24 | 1 << 28   # (M4: quiet 50 %, key lights on; M5: the
                                                                          # test flash's hint off)
    r2b = run("p4-settings-restart", PLAY_MS + 3000, open_song(g) + ["1500:pot:4096"] + press(PLAY_MS, "PLAY"),
              flash=r1["flash"])
    s2 = r2b["stats"]
    check(r1["stats"]["settings"] == want and r1["stats"]["settings_saves"] == 1 and s2["settings"] == want,
          f"settings (GLO, GLO; p4-settings-screen.png): key lights blink OFF, count-in 2 BARS, volume LOW: in use "
          f"({r1['stats']['settings']:#x}), saved once, and after a restart from that flash still {s2['settings']:#x}")
    check(s2["start_from"] - s2["start_tick"] == bt + 2 * bt - (bt - cg["pickup_beats"] * cg["ppq"]) or
          s2["start_tick"] == (s2["start_from"] // bt) * bt - 2 * bt,
          f"count-in 2 bars: PLAY from tick {s2['start_from']} counts in from {s2['start_tick']} (two bars before "
          f"its bar)")
    lo = rms(r2b["left"][int((PLAY_MS + 50) / 1000 * RATE):int((PLAY_MS + 1500) / 1000 * RATE)])
    check(lo < r3 * 0.85, f"count-in volume LOW: rms {lo:.0f} (MEDIUM, the default: {r3:.0f})")
    # blink off: the keys steady (no middle level)
    r = run("p4-blink-off", PLAY_MS + 7000, open_song(g) + sc + press(t + 300, "ENV") + press(t + 500, "ARP")
            + press(PLAY_MS + 1500, "PLAY"))
    s4 = r["stats"]
    check(s4["part"] == 2 and s4["mid_keys"] == 0 and s4["lit_keys"] and s4["key_flashes"] <= 2,
          f"blink OFF: the harmony's keys lit steadily (lit {s4['lit_keys']:#x}, middle {s4['mid_keys']:#x}, "
          f"{s4['key_flashes']} flash(es))")
    # the levels make inversions visible: level 1 all root position, level 2 has other inversions (the data); shown
    # on the inversion line (p4-inv-l1.png, p4-inv-l2.png) and in the zoomed keyboard view (p4-keys.png)
    def inv(c, lvl):
        out = []
        for e in c["chord_events"]:
            v = [n for n in e["voicing"][lvl - 1] if n]
            ch = c["chords"][e["chord"]]
            if v and ch["root"] is not None:
                out.append((min(v) - ch["root"]) % 12)
        return out
    l1, l2 = inv(cg, 1), inv(cg, 2)
    check(all(x == 0 for x in l1) and any(x for x in l2), f"Amazing Grace: level 1 all root position; level 2 "
          f"{sum(1 for x in l2 if x)} of {len(l2)} chords in an inversion (voice leading)")
    o = open_song(d) + press(OPEN_MS + 200, "ENV")
    run("p4-inv-l1", PLAY_MS + 9000, o + press(PLAY_MS, "PLAY"))
    run("p4-inv-l2", PLAY_MS + 9000, o + [f"{OPEN_MS + 400}:e4:1"] + press(PLAY_MS, "PLAY"))
    run("p4-keys", PLAY_MS + 9000, o + [f"{OPEN_MS + 400}:e4:1"] + press(OPEN_MS + 700, "EDIT") + press(PLAY_MS, "PLAY"))
    run("p4-keys-bass", PLAY_MS + 9000, open_song(d) + press(OPEN_MS + 200, "SCL") + press(OPEN_MS + 700, "EDIT")
        + press(PLAY_MS, "PLAY"))
    # the count-in in every view: chord, keyboard, chart (p4-count-*.png)
    for v, n in (("chord", 0), ("keys", 1), ("chart", 2)):
        r = run(f"p4-count-{v}", int(PLAY_MS + beat_ms * 1.5), open_song(g) + press(OPEN_MS + 200, "ENV")
                + [x for k in range(n) for x in press(OPEN_MS + 500 + 300 * k, "EDIT")] + press(PLAY_MS, "PLAY"))
        px = r["lcd"].crop((0, 64, 480, 352)).getcolors(1 << 16) or []
        amber = sum(c for c, (rr, gg, bb) in px if rr > 200 and 120 < gg < 200 and bb < 60)
        check(r["stats"]["view"] == n and amber > 500, f"count-in shown in the {v} view (p4-count-{v}.png)")
    rep += lines
    return st["ok"]


def test_part5(all_songs, rep):
    """M3 part 5: Happy Birthday (its full run is the per-song test); here the list and the pickup. M10: 15 songs = the 9 older
    + the 6 new ones; it was a stale count (9) that failed in M9's untidy run (10)"""
    lines, st, check = checker("Part 5: Happy Birthday")
    slugs = [c["slug"] for c in all_songs]
    i = slugs.index("happy-birthday")
    c = all_songs[i]
    bt = 3 * c["ppq"]
    mel = sorted(e[0] for e in c["layers"]["melody"]["variants"][1] if e[1] == 144)
    check(len(all_songs) == 15 and c["pickup_beats"] == 1 and mel[:2] == [2 * c["ppq"], 2 * c["ppq"] + c["ppq"] // 2]
          and c["bar_count"] == 17, f"15 songs; Happy Birthday: 3/4, a one-beat pickup of two eighths (melody at ticks "
          f"{mel[:2]}), 16 bars + the pickup bar, {own_bpm(c)} BPM")
    r = run("p5-list", 5300, [f"4600:e0:{i}"])
    r2 = run("p5-open", PLAY_MS + 7000, open_song(i) + press(OPEN_MS + 200, "LFO") + press(PLAY_MS, "PLAY"))
    s = r2["stats"]
    check(s["part"] == 3 and s["start_from"] == 2 * c["ppq"],
          f"opened from the list (p5-list.png), melody muted, played from the pickup (tick {s['start_from']}; "
          f"p5-open.png)")
    rep += lines
    return st["ok"]


def test_part6(all_songs, rep):
    """M3 part 6: the guitar view (harmony) and the bass tab (a scrolling view)"""
    lines, st, check = checker("Part 6: guitar chord boxes, bass tab")
    r = subprocess.run([sys.executable, "-I", str(ROOT / "tools/guitar_check.py")], capture_output=True, text=True)
    check(r.returncode == 0, "tools/guitar_check.py: " + " / ".join(r.stdout.strip().splitlines()))
    slugs = [c["slug"] for c in all_songs]
    d, b = slugs.index("danny-boy"), slugs.index("twelve-bar-blues")
    cd = all_songs[d]
    harm = open_song(d) + press(OPEN_MS + 200, "ENV") + [x for k in range(3) for x in press(OPEN_MS + 400 + 250 * k,
                                                                                         "EDIT")]
    r1 = run("p6-guitar", PLAY_MS + 9000, harm + press(PLAY_MS, "PLAY"))
    r2 = run("p6-guitar-key", PLAY_MS + 9000, harm + press(OPEN_MS + 1300, "SAVE") + [f"{OPEN_MS + 1500}:e0:-3"]
             + press(OPEN_MS + 1900, "SAVE") + press(PLAY_MS, "PLAY"))
    check(r1["stats"]["view"] == 3 and r2["stats"]["view"] == 3 and r2["stats"]["transpose"] == -3,
          "harmony muted, EDIT x3: the guitar view (p6-guitar.png: C/E in Danny Boy, open shapes); in A (-3), barre "
          "shapes follow the key (p6-guitar-key.png)")
    bass = open_song(d) + press(OPEN_MS + 200, "SCL") + [x for k in range(3) for x in press(OPEN_MS + 400 + 250 * k,
                                                                                        "EDIT")]
    r3 = run("p6-tab", PLAY_MS + 9000, bass + press(PLAY_MS, "PLAY"))
    run("p6-tab-stopped", OPEN_MS + 2000, bass)
    s3 = r3["stats"]
    check(s3["view"] == 4 and s3["deferred"] == 0 and max(s3["cpu"][:8]) <= 30,
          f"bass muted, EDIT x3: the bass tab, scrolling (p6-tab.png); the audio is not disturbed by redrawing it "
          f"each frame (no deferred block, CPU at most {max(s3['cpu'][:8])} %)")
    # a whole song in the tab view at 250 BPM (the busiest redraw): played to the end, on time
    cb = all_songs[b]
    bass_b = open_song(b) + press(OPEN_MS + 100, "ARP") + [f"{OPEN_MS + 200}:e0:200"] + press(OPEN_MS + 500, "SCL") \
        + [x for k in range(3) for x in press(OPEN_MS + 700 + 250 * k, "EDIT")]
    ms = song_ms(cb, count_in=False, bpm=250)
    r4 = run("p6-tab-250", int(PLAY_MS + ms + 2000), bass_b + press(PLAY_MS, "PLAY"))
    s4 = r4["stats"]
    want = ticks_to_frames(cb, cb["end_tick"], 250)
    got = s4["end_frames"] - s4["start_frames"]
    check(s4["ends"] == 1 and abs(got - want) <= BLOCK and s4["deferred"] == 0 and s4["view"] == 4,
          f"the Blues at 250 BPM in the tab view: to the end on time ({got} frames, predicted {want:.0f}), no "
          f"deferred block, CPU at most {max(s4['cpu'][:cb['bar_count']])} %")
    rep += lines
    return st["ok"]


def test_part7(all_songs, rep):
    """M3 part 7: the scrolling views: melody roll, notation (treble, bass), drum grid; the zoom setting"""
    lines, st, check = checker("Part 7: scrolling views")
    slugs = [c["slug"] for c in all_songs]
    g, d, sa, b = (slugs.index(x) for x in ("amazing-grace", "danny-boy", "saints", "twelve-bar-blues"))
    edits = lambda n, t0=OPEN_MS + 400: [x for k in range(n) for x in press(t0 + 250 * k, "EDIT")]
    runs = {
        "p7-roll": (open_song(g) + press(OPEN_MS + 200, "LFO") + edits(3), 5),
        "p7-notation": (open_song(g) + press(OPEN_MS + 200, "LFO") + edits(4), 6),
        "p7-notation-bass": (open_song(d) + press(OPEN_MS + 200, "SCL") + edits(4), 6),
        "p7-grid": (open_song(sa) + press(OPEN_MS + 200, "FX") + edits(2), 7),
    }
    for name, (sc, view) in runs.items():
        r = run(name, PLAY_MS + 9000, sc + press(PLAY_MS, "PLAY"))
        s = r["stats"]
        check(s["view"] == view and s["deferred"] == 0 and not r["halt"],
              f"{name}: view {s['view']} (expected {view}), scrolling; no deferred audio block, CPU at most "
              f"{max(s['cpu'][:6])} % ({name}.png)")
    # zoom: 3 bars (the setting), the notation in a key with flats (Greensleeves -> key change)
    sc, t = settings_script(OPEN_MS + 200, [(4, 1)])
    r = run("p7-notation-zoom", PLAY_MS + 9000, open_song(g) + sc + press(t + 200, "LFO") + edits(4, t + 400)
            + press(PLAY_MS + 1000, "PLAY"))
    check(r["stats"]["view"] == 6 and (r["stats"]["settings"] >> 16) & 15 == 1,
          "the setting 'Scrolling views show 3 BARS': the notation at half the scale (p7-notation-zoom.png)")
    gr = slugs.index("greensleeves")
    r = run("p7-notation-flats", PLAY_MS + 9000, open_song(gr) + press(OPEN_MS + 200, "LFO") + edits(4)
            + press(OPEN_MS + 1500, "SAVE") + [f"{OPEN_MS + 1700}:e0:5"] + press(OPEN_MS + 2100, "SAVE")
            + press(PLAY_MS, "PLAY"))
    check(r["stats"]["view"] == 6 and r["stats"]["transpose"] == 5, "Greensleeves moved to D minor (+5): the key "
          "signature and accidentals follow (p7-notation-flats.png)")
    # the whole Blues at 250 BPM in the drum grid and the notation: on time, no deferred block
    cb = all_songs[b]
    ms = song_ms(cb, count_in=False, bpm=250)
    for name, mute, n in (("p7-grid-250", "FX", 2), ("p7-notation-250", "LFO", 4)):
        r = run(name, int(PLAY_MS + ms + 2000), open_song(b) + press(OPEN_MS + 100, "ARP") +
                [f"{OPEN_MS + 200}:e0:200"] + press(OPEN_MS + 500, mute) + edits(n, OPEN_MS + 700) + press(PLAY_MS, "PLAY"))
        s = r["stats"]
        want = ticks_to_frames(cb, cb["end_tick"], 250)
        got = s["end_frames"] - s["start_frames"]
        check(s["ends"] == 1 and abs(got - want) <= BLOCK and s["deferred"] == 0,
              f"{name}: the Blues at 250 BPM to the end on time ({got} frames, predicted {want:.0f}), no deferred "
              f"block, CPU at most {max(s['cpu'][:cb['bar_count']])} %")
    rep += lines
    return st["ok"]


def test_part8(all_songs, rep):
    """M3 part 8: song folders (categories, format 1.2), older formats, an M2 FM-1 gets 1.1; the guide volume"""
    lines, st, check = checker("Part 8: song folders, format 1.2, guide volume")
    cats = sorted({c["category"] for c in all_songs})
    folk = sorted((c for c in all_songs if c["category"] == "Folk"), key=lambda c: c["title"].lower())
    k = cats.index("Folk") + 1                         # the folders: All songs, then the categories, sorted
    r0 = run("p8-all", 4600, [])                      # (M5: the splash is 4 s)
    r1 = run("p8-folders", 5300, press(4600, "HOME"))
    r2 = run("p8-folk", 6000, press(4600, "HOME") + [f"4800:e0:{k}"] + press(5200, "PLAY"))
    r3 = run("p8-folk-open", 7000, press(4600, "HOME") + [f"4800:e0:{k}"] + press(5200, "PLAY") + press(5800, "PLAY"))
    check(r0["stats"]["folder"] == 1 and r1["stats"]["folder"] == 0 and r2["stats"]["folder"] == k + 1 and
          r3["stats"]["ui_view"] == 1 and r3["stats"]["song_crc"] == int(folk[0]["crc32"], 16),
          f"the list opens in All songs (p8-all.png); HOME: the folders {['All songs'] + cats} (p8-folders.png); "
          f"Folk: its {len(folk)} songs (p8-folk.png); PLAY: {folk[0]['title']} opens")
    # older files: a 1.1 and a 1.0 file are still read (no category: the folder "Other")
    d = OUT / "p8"
    d.mkdir(exist_ok=True)
    f11, f10 = d / "saints-11.fm1s", d / "grace-10.fm1s"
    subprocess.run([SEND, "compile", str(ROOT / "songs/saints"), "--format", "1.1", "-o", str(f11)], check=True,
                   capture_output=True)
    subprocess.run([SEND, "compile", str(ROOT / "songs/amazing-grace"), "--format", "1.0", "-o", str(f10)],
                   check=True, capture_output=True)
    r = run("p8-old-send", 60000, [], host=f"{SEND} send --emu-pipe {f11} {f10}")
    r4 = run("p8-old-folders", 5300, press(4600, "HOME"), flash=r["flash"])
    i = [c["slug"] for c in all_songs].index("saints")
    r5 = run("p8-old-play", PLAY_MS + 3000, open_song(i) + press(PLAY_MS, "PLAY"), flash=r["flash"])
    check(r["out"].count("stored") == 2 and r5["stats"]["starts"] == 1 and r5["stats"]["song_crc"] != 0,
          "a 1.1 and a 1.0 file sent: both stored and played; with no category they are in the folder Other "
          "(p8-old-folders.png)")
    # an M2 FM-1 (reads up to 1.1): fm1song send compiles 1.1 for it, without the category
    if M2_FLASH.exists():
        r = run("p8-send-to-m2", 60000, [], flash=str(M2_FLASH), pkg=M2_PKG,
                host=f"{SEND} send --emu-pipe {ROOT}/songs/happy-birthday")
        check("sending 1.1" in r["out"] and "stored" in r["out"] and "NOT stored" not in r["out"],
              "fm1song send to an M2 FM-1 (it reads up to 1.1): sent as 1.1 without the category, stored")
    # (M3's guide volume became M4's QUIET: tools/m4_test.py m4p2)
    rep += lines
    return st["ok"]


PART_TESTS = {"part1": test_part1, "part2": test_part2, "part3": test_part3, "part4": test_part4, "part5": test_part5,
              "part6": test_part6, "part7": test_part7, "part8": test_part8}


# ------------------------------------------------------------------------------------------------ M4, per part
def test_m4p1(all_songs, rep):
    """M4 part 1: count-in default MEDIUM; the whole-song loop back; the list's help mentions HOME = folders"""
    lines, st, check = checker("M4 part 1: quick fixes")
    slugs = [c["slug"] for c in all_songs]
    i = slugs.index("happy-birthday")
    c = all_songs[i]
    r = run("m4p1-defaults", 4000, [])
    check((r["stats"]["settings"] >> 8) & 15 == 1, "no settings saved: the count-in volume is MEDIUM")
    ms = song_ms(c, count_in=False)
    r = run("m4p1-song-loop", int(PLAY_MS + ms + 4000), open_song(i) + press(OPEN_MS + 200, "ARP")
            + press(OPEN_MS + 400, "SEQ") + press(PLAY_MS, "PLAY"))
    s = r["stats"]
    check(s["loops"] >= 1 and s["ends"] == 0 and s["loop_from"] == c["sections"][0]["first_bar"] * 0 + (
        (int(c["time_signature"][0]) - c["pickup_beats"]) * c["ppq"]) and s["loop_to"] == c["end_tick"],
          f"SEQ once: LOOP SONG; at the end it goes round again from the pickup ({s['loops']} jump, "
          f"{s['loop_to']} -> {s['loop_from']}), never ends (m4p1-song-loop.png)")
    r = run("m4p1-song-then-ab", OPEN_MS + 1500, open_song(i) + press(OPEN_MS + 200, "SEQ")
            + press(OPEN_MS + 500, "SEQ"))
    check(r["stats"]["loop_step"] == 1, "SEQ again: the song loop off, setting A (m4p1-song-then-ab.png)")
    run("m4p1-help-list", 5300, press(4600, "GLO"))
    rep += lines
    return st["ok"]


def test_m4p2(all_songs, rep):
    """M4 part 2 (D027): your part chosen by holding its button, followed muted or not; a press cycles FULL / QUIET /
    MUTED; settings: the quiet level, key lights on / off"""
    lines, st, check = checker("M4 part 2: your part chosen on purpose, FULL / QUIET / MUTED")
    slugs = [c["slug"] for c in all_songs]
    g, d = slugs.index("amazing-grace"), slugs.index("danny-boy")
    cd = all_songs[d]
    O = OPEN_MS
    lead = lambda c, ms: int(ms / 1000 * c["ppq"] * own_bpm(c) / 60)
    r = run("m4p2-default", PLAY_MS + 6000, open_song(g) + press_raw(O + 200, "ARP") + press_raw(PLAY_MS, "PLAY"))
    s = r["stats"]
    check(s["part"] == 2 and (s["lit_keys"] | s["mid_keys"]) and s["notes_skipped"][2] == 0 and s["key_flashes"] > 0,
          f"a song opens with the harmony as your part: its keys light (and blink, {s['key_flashes']} flashes) while "
          f"it plays at FULL (nothing muted) (m4p2-default.png)")
    r = run("m4p2-hold-bass", PLAY_MS + 6000, open_song(d) + hold(O + 100, "SCL") + press_raw(O + 1300, "ARP")
            + press_raw(PLAY_MS, "PLAY"))
    s = r["stats"]
    note = min(last_attack_notes(cd, "bass", 1, s["last_tick"] + lead(cd, 30)) or [0])
    check(s["part"] == 1 and (s["lit_keys"] | s["mid_keys"]) == key_bits([note]) and s["notes_skipped"][1] == 0 and
          s["notes_on"][1] > 0, f"hold SEL (bass) 1 s: your part = bass, not muted: it plays and its note's key lights "
          f"({note}) (m4p2-hold-bass.png)")
    # harmony alone (drums, bass, melody muted: three presses... two each) at FULL, QUIET, MUTED, and FULL again
    base = open_song(g) + ["1500:pot:4096"] + press_raw(O + 100, "ARP")
    for b, t0 in (("FX", O + 200), ("SCL", O + 500), ("LFO", O + 800)):
        base += tap(t0, b, 2)
    seg = lambda r: rms(r["left"][int((PLAY_MS + 1500) / 1000 * RATE):])
    runs = {n: run(f"m4p2-{n}", PLAY_MS + 7000, base + (tap(O + 1100, "ENV", k) if k else []) + press_raw(PLAY_MS, "PLAY"))
            for n, k in (("full", 0), ("quiet", 1), ("muted", 2), ("full-again", 3))}
    lv = {n: seg(x) for n, x in runs.items()}
    sq, sm, sf = runs["quiet"]["stats"], runs["muted"]["stats"], runs["full-again"]["stats"]
    check(lv["muted"] < lv["quiet"] < lv["full"] and abs(lv["full-again"] - lv["full"]) < 0.1 * lv["full"] and
          sq["notes_guided"] > 0 and sq["notes_skipped"][2] == 0 and sm["notes_skipped"][2] > 0 and
          sf["notes_guided"] == 0 and sm["part"] == 2,
          f"press ENV: QUIET (rms {lv['quiet']:.0f}), again: MUTED ({lv['muted']:.0f}), again: FULL "
          f"({lv['full-again']:.0f}; at first {lv['full']:.0f}); the harmony stays your part (m4p2-quiet.png)")
    sc, t = settings_script(O + 100, [(5, -1)])     # (the settings first: a layer button closes the help)
    b25 = open_song(g) + ["1500:pot:4096"] + sc + press_raw(t + 150, "ARP")
    for k, bt in enumerate(("FX", "SCL", "LFO")):
        b25 += tap(t + 300 + 300 * k, bt, 2)
    r = run("m4p2-quiet25", PLAY_MS + 8000, b25 + tap(t + 1300, "ENV", 1) + press_raw(PLAY_MS + 1000, "PLAY"))
    q25 = rms(r["left"][int((PLAY_MS + 2500) / 1000 * RATE):])
    check(q25 < lv["quiet"] and (r["stats"]["settings"] >> 20) & 15 == 0,
          f"setting Quiet level 25 %: rms {q25:.0f} (50 %, the default: {lv['quiet']:.0f})")
    sc, t = settings_script(O + 100, [(6, 1)])
    r = run("m4p2-lights-off", PLAY_MS + 6000, open_song(g) + sc + press_raw(PLAY_MS + 800, "PLAY"))
    check(not (r["stats"]["lit_keys"] | r["stats"]["mid_keys"]), "setting Key lights OFF: no key lights")
    r = run("m4p2-drums", PLAY_MS + 5000, open_song(g) + hold(O + 100, "FX") + press_raw(PLAY_MS, "PLAY"))
    check(r["stats"]["part"] == 0 and not (r["stats"]["lit_keys"] | r["stats"]["mid_keys"]),
          "hold FX: your part = drums; no key lights (the beat light is enough)")
    rep += lines
    return st["ok"]


def test_m4p3(all_songs, rep):
    """M4 part 3: harmony notation (grand staff), drums: grid + drum notation only, hiding views (settings)"""
    lines, st, check = checker("M4 part 3: harmony notation, drum views, hidden views")
    slugs = [c["slug"] for c in all_songs]
    g, sa, b = slugs.index("amazing-grace"), slugs.index("saints"), slugs.index("twelve-bar-blues")
    O = OPEN_MS
    ed = lambda n, t0: [x for k in range(n) for x in press_raw(t0 + 250 * k, "EDIT")]
    for lvl in (1, 2):
        r = run(f"m4p3-harm-notation{lvl}", PLAY_MS + 7000, open_song(g) + ([f"{O + 100}:e4:1"] if lvl == 2 else [])
                + ed(4, O + 300) + press_raw(PLAY_MS, "PLAY"))
        check(r["stats"]["view"] == 6 and r["stats"]["part"] == 2 and r["stats"]["deferred"] == 0,
              f"harmony (your part by default), EDIT x4: the notation, a grand staff with the chords stacked, level "
              f"{lvl} (m4p3-harm-notation{lvl}.png)")
    r1 = run("m4p3-drum-grid", PLAY_MS + 5000, open_song(sa) + hold(O + 100, "FX") + press_raw(PLAY_MS, "PLAY"))
    r2 = run("m4p3-drum-notation", PLAY_MS + 5000, open_song(sa) + hold(O + 100, "FX") + ed(1, O + 1300)
             + press_raw(PLAY_MS, "PLAY"))
    r3 = run("m4p3-drum-back", PLAY_MS + 3000, open_song(sa) + hold(O + 100, "FX") + ed(2, O + 1300)
             + press_raw(PLAY_MS, "PLAY"))
    check(r1["stats"]["view"] == 7 and r2["stats"]["view"] == 8 and r3["stats"]["view"] == 7,
          "drums your part: the drum grid first, EDIT: the drum notation, EDIT: the grid again (only these two; "
          "m4p3-drum-grid.png, m4p3-drum-notation.png)")
    # hide the harmony's keyboard and chart: EDIT goes chord -> guitar
    vs = press_raw(O + 100, "GLO") + press_raw(O + 300, "GLO") + [f"{O + 500}:e0:7"] + press_raw(O + 800, "PLAY") \
        + [f"{O + 1000}:e0:8"] + [f"{O + 1300}:e1:1"] + [f"{O + 1600}:e0:1"] + [f"{O + 1900}:e1:1"]
    run("m4p3-viewset", O + 2200, open_song(g) + vs)
    vs += press_raw(O + 2200, "GLO") + press_raw(O + 2500, "GLO")
    r = run("m4p3-hidden", PLAY_MS + 3500, open_song(g) + vs + ed(1, O + 2900) + press_raw(PLAY_MS + 1500, "PLAY"))
    r2 = run("m4p3-hidden-kept", 5000, [], flash=r["flash"])
    r3 = run("m4p3-hidden-restart", PLAY_MS + 2000, open_song(g) + ed(1, O + 300) + press_raw(PLAY_MS, "PLAY"),
             flash=r["flash"])
    check(r["stats"]["view"] == 3 and r["stats"]["settings_saves"] == 1 and r3["stats"]["view"] == 3,
          "settings, Views: the harmony's KEYBOARD and CHART hidden (m4p3-viewset.png): EDIT goes from the chord "
          "straight to the guitar; saved, and still so after a restart")
    # the last view of a layer cannot be hidden: drums' grid hidden, then its notation refused
    vd = press_raw(O + 100, "GLO") + press_raw(O + 300, "GLO") + [f"{O + 500}:e0:7"] + press_raw(O + 800, "PLAY") \
        + [f"{O + 1000}:e1:1"] + [f"{O + 1300}:e0:1"] + [f"{O + 1600}:e1:1"] + press_raw(O + 1900, "GLO") \
        + press_raw(O + 2100, "GLO")
    r = run("m4p3-last-view", PLAY_MS + 3000, open_song(sa) + vd + hold(O + 2400, "FX") + press_raw(PLAY_MS + 900, "PLAY"))
    check(r["stats"]["view"] == 8, "drums: the grid hidden, then hiding the notation too is refused (ONE VIEW MUST "
          "STAY): your part drums shows the drum notation")
    cb = all_songs[b]
    ms = song_ms(cb, count_in=False, bpm=250)
    r = run("m4p3-harm-notation-250", int(PLAY_MS + ms + 2000), open_song(b) + press_raw(O + 100, "ARP")
            + [f"{O + 200}:e0:200"] + [f"{O + 500}:e4:1"] + ed(4, O + 700) + press_raw(PLAY_MS, "PLAY"))
    s = r["stats"]
    want = ticks_to_frames(cb, cb["end_tick"], 250)
    got = s["end_frames"] - s["start_frames"]
    check(s["ends"] == 1 and abs(got - want) <= BLOCK and s["deferred"] == 0 and s["view"] == 6,
          f"the Blues at 250 BPM, harmony L2 in the notation: on time ({got} / {want:.0f} frames), no deferred block, "
          f"CPU at most {max(s['cpu'][:cb['bar_count']])} %")
    rep += lines
    return st["ok"]


def ladder_facts(c):
    """the generated levels, from the compiled song: per harmony level the attacks, the notes per attack (most), the
    span of notes within one chord (most); bass and drums: attacks per level"""
    out = {}
    ev = c["chord_events"]
    for layer in ("harmony", "bass", "drums"):
        for lv, evs in c["layers"][layer]["variants"].items():
            ons = [e for e in evs if e[1] == 144]
            ticks = {}
            last = ev[-1]["tick"]                    # (the last chord is held at every level: left out)
            for e in ons:
                if e[0] < last:
                    ticks.setdefault(e[0], []).append(e[2])
            span = 0
            for k in range(len(ev)):
                t0, t1 = ev[k]["tick"], ev[k + 1]["tick"] if k + 1 < len(ev) else 1 << 30
                ps = [e[2] for e in ons if t0 <= e[0] < t1]
                span = max(span, max(ps) - min(ps) if ps else 0)
            out[(layer, lv)] = (len(ticks), max((len(v) for v in ticks.values()), default=0), span)
    return out


def test_m4p4(all_songs, rep):
    """M4 part 4: five harmony levels, bass 4, drums 3 (format 1.3); the device follows each level"""
    lines, st, check = checker("M4 part 4: five harmony levels (format 1.3)")
    bad = []
    for c in all_songs:
        n = {l: len(c["layers"][l]["variants"]) for l in ("harmony", "bass", "drums")}
        f = ladder_facts(c)
        h = [f[("harmony", k)] for k in range(1, 6)]
        okc = (c["version"] == "2.0" and n == {"harmony": 5, "bass": 4, "drums": 3}   # (M5: 2.0, the levels generated)
               and h[0][0] < h[1][0] and h[1][0] == h[2][0]           # L2, L3: the same rhythm, more than L1
               and h[3][1] == 1 and h[4][1] == 1                      # L4, L5: one note at a time
               and h[3][0] > h[1][0] * 0.5 and h[4][2] >= 12          # L5 spans an octave or more in a chord
               and f[("bass", 1)][0] < f[("bass", 2)][0] <= f[("bass", 4)][0]
               and f[("drums", 1)][0] < f[("drums", 2)][0] <= f[("drums", 3)][0] + 4)
        if not okc:
            bad.append((c["slug"], n, h))
        lines.append(f"  - {c['slug']}: harmony attacks/notes/span by level {[x for x in h]}; bass attacks "
                     f"{[f[('bass', k)][0] for k in range(1, 5)]}; drums {[f[('drums', k)][0] for k in range(1, 4)]}")
    check(not bad, f"all {len(all_songs)} songs: format 1.3, harmony 5 levels (L1 held < L2 = L3 rhythm; L4, L5 a note "
                   f"at a time, L5 over an octave), bass 4, drums 3 (busier upwards)" + (f"; BAD {bad[:2]}" if bad else ""))
    slugs = [c["slug"] for c in all_songs]
    g, su = slugs.index("amazing-grace"), slugs.index("oh-susanna")
    cg = all_songs[g]
    lead = lambda c, ms: int(ms / 1000 * c["ppq"] * own_bpm(c) / 60)
    r = run("m4p4-l5", PLAY_MS + 7000, open_song(g) + press_raw(OPEN_MS + 100, "ARP") + [f"{OPEN_MS + 200}:e4:9"]
            + press_raw(PLAY_MS, "PLAY"))
    s = r["stats"]
    want = attacks(cg, "harmony", 5, s["start_from"], s["last_tick"] + lead(cg, 30))
    check(bin(s["lit_keys"]).count("1") <= 1 and s["mid_keys"] and abs(s["key_flashes"] - len(want)) <= 2,
          f"harmony L5 (K3 turned up past 5: stops at 5): the key lights flash each note as it is played ({s['key_flashes']} "
          f"flashes for {len(want)} notes), the chord around it at the middle level (m4p4-l5.png)")
    run("m4p4-l4-keys", PLAY_MS + 7000, open_song(g) + [f"{OPEN_MS + 200}:e4:3"]
        + [x for k in range(1) for x in press_raw(OPEN_MS + 500, "EDIT")] + press_raw(PLAY_MS, "PLAY"))
    run("m4p4-l3", PLAY_MS + 7000, open_song(g) + [f"{OPEN_MS + 200}:e4:2"] + press_raw(PLAY_MS, "PLAY"))
    # older formats: a 1.2 copy (3 levels) is read; fm1song send to an M3 FM-1 sends 1.2 without levels above 3
    d = OUT / "m4p4"
    d.mkdir(exist_ok=True)
    f12 = d / "saints-12.fm1s"
    subprocess.run([SEND, "compile", str(ROOT / "songs/saints"), "--format", "1.2", "-o", str(f12)], check=True,
                   capture_output=True)
    c12 = read_compiled(f12.read_bytes())
    r = run("m4p4-old-send", 60000, [], host=f"{SEND} send --emu-pipe {f12}")
    i = slugs.index("saints")
    r2 = run("m4p4-old-play", PLAY_MS + 3000, open_song(i) + [f"{OPEN_MS + 200}:e4:9"] + press_raw(PLAY_MS, "PLAY"),
             flash=r["flash"])
    check(c12["version"] == "1.2" and len(c12["layers"]["harmony"]["variants"]) == 3 and "stored" in r["out"] and
          r2["stats"]["starts"] == 1, "a 1.2 copy (the first 3 levels) is stored and plays; K3 stops at its 3")
    if M3_FLASH.exists():
        r = run("m4p4-send-to-m3", 60000, [], flash=str(M3_FLASH), pkg=M3_PKG,
                host=f"{SEND} send --emu-pipe {ROOT}/songs/oh-susanna")
        check("sending 1.2" in r["out"] and "stored" in r["out"] and "NOT stored" not in r["out"],
              "fm1song send to an M3 FM-1 (it reads up to 1.2): sent as 1.2 with the first 3 levels, stored")
    # the busiest: Oh! Susanna at 250 BPM, harmony L5, bass L4, drums L3
    cs = all_songs[su]
    ms = song_ms(cs, count_in=False, bpm=250)
    r = run("m4p4-busy-250", int(PLAY_MS + ms + 2000), open_song(su) + press_raw(OPEN_MS + 100, "ARP")
            + [f"{OPEN_MS + 200}:e0:200", f"{OPEN_MS + 300}:e4:9", f"{OPEN_MS + 400}:e3:9", f"{OPEN_MS + 500}:e2:9"]
            + press_raw(PLAY_MS, "PLAY"))
    s = r["stats"]
    start = (int(cs["time_signature"][0]) - cs["pickup_beats"]) * cs["ppq"] if cs["pickup_beats"] else 0
    want = ticks_to_frames(cs, cs["end_tick"] - start, 250)   # (from the pickup's first note, no count-in)
    got = s["end_frames"] - s["start_frames"]
    nb = min(cs["bar_count"], 64)
    check(s["ends"] == 1 and abs(got - want) <= BLOCK and s["deferred"] == 0 and max(s["voices"][:nb]) <= 24,
          f"Oh! Susanna at 250 BPM with harmony L5, bass L4, drums L3: on time ({got} / {want:.0f}), no deferred "
          f"block, at most {max(s['voices'][:nb])} voices, CPU at most {max(s['cpu'][:nb])} %")
    rep += lines
    return st["ok"]


def test_m4p5(all_songs, rep):
    """M4 part 5: the import tool's songs (the user's private test songs, songs/private/; never committed): sent on a
    flash of their own (the store holds them, not with all 9 of ours), each played to the end at its top levels"""
    lines, st, check = checker("M4 part 5: imported songs (private)")
    priv = private_dirs()
    if not priv:
        lines.append("- (no songs/private/: the user's private songs are not on this machine; nothing to test)")
        rep += lines
        return True
    cs = []
    for f in priv:
        c = read_compiled(compile_folder(f)[0])
        n = {l: len(c["layers"][l]["variants"]) for l in c["layers"]}
        cs.append(c)
        check(c["version"] == ("2.1" if f.name == "zombie" else "2.0") and n.get("harmony") == 5 and
              n.get("bass") == 4 and n.get("drums") == 3 and c["category"] == "Private test",
              f"{f.name}: format {c['version']} (M5; M7: Zombie 2.1, its bass's start level), every level ({n}), category Private test, "
              f"{c['size']} bytes, {c['bar_count']} bars")
    r = run("m4p5-send", 180000, [], flash="empty_quiet", host=f"{SEND} send --emu-pipe " + " ".join(str(f) for f in priv))
    check(r["out"].count(": stored (") == len(priv), f"all {len(priv)} sent over emulated USB and stored")
    fl = r["flash"]
    shutil.copy(fl, OUT / "flash" / "private.bin")
    cs.sort(key=lambda c: c["title"].lower())
    for i, c in enumerate(cs):
        ms = song_ms(c, count_in=False)
        bpb = int(c["time_signature"][0])
        start = (bpb - c["pickup_beats"]) * c["ppq"] if c["pickup_beats"] else 0
        sc = ([f"4150:e0:{i}"] if i else []) + press_raw(OPEN_MS, "PLAY") + press_raw(OPEN_MS + 100, "ARP") \
            + [f"{OPEN_MS + 200}:e4:9", f"{OPEN_MS + 300}:e3:9", f"{OPEN_MS + 400}:e2:9"] + press_raw(PLAY_MS, "PLAY")
        r = run(f"m4p5-{c['slug']}", int(PLAY_MS + song_ms(c) + 2500), sc, flash=str(OUT / "flash" / "private.bin"))
        s = r["stats"]
        want = ticks_to_frames(c, c["end_tick"] - s["start_tick"])      # (from the count-in's first beat)
        got = s["end_frames"] - s["start_frames"]
        nb = min(c["bar_count"], 64)
        check(s["ends"] == 1 and abs(got - want) <= BLOCK and s["deferred"] == 0 and max(s["voices"][:nb]) <= 24
              and s["song_crc"] == int(c["crc32"], 16),
              f"{c['title']}: played to the end at harmony L5, bass L4, drums L3, on time ({got} / {want:.0f}), no "
              f"deferred block, at most {max(s['voices'][:nb])} voices, CPU at most {max(s['cpu'][:nb])} %")
    rep += lines
    return st["ok"]


PART_TESTS.update({"m4p1": test_m4p1, "m4p2": test_m4p2, "m4p3": test_m4p3, "m4p4": test_m4p4, "m4p5": test_m4p5})
del PART_TESTS["part2"]                          # (M3's "the last muted is yours": replaced by m4p2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jobs", type=int, default=3)
    ap.add_argument("--only")
    ap.add_argument("--pkg", help="the package to test (default: the dev build, firmware/app/build/fm1acc.fwsc)")
    ap.add_argument("--out", help="the work folder (default build/emu/m5); two runs at once need two folders")
    a = ap.parse_args()
    only = set(a.only.split(",")) if a.only else None
    want = lambda g: not only or g in only
    global PKG, OUT
    if a.pkg:
        PKG = Path(a.pkg).resolve()
    if a.out:
        OUT = Path(a.out).resolve()
    if not PKG.exists():
        raise SystemExit("m5_test: build first (make firmware)")
    OUT.mkdir(parents=True, exist_ok=True)
    all_songs = songs()
    reports, results = {}, {}
    sent = prepare_flash()
    with ThreadPoolExecutor(a.jobs) as pool:
        futs = {}
        for i, c in enumerate(all_songs):
            if only and c["slug"] not in only:
                continue
            reports[c["slug"]] = []
            futs[c["slug"]] = pool.submit(test_song, i, c, reports[c["slug"]])
        if want("store"):
            reports["store"] = []
            futs["store"] = pool.submit(test_store, all_songs, sent, reports["store"])
        if want("boot"):
            reports["boot"] = []
            futs["boot"] = pool.submit(test_boot, reports["boot"])
        if want("practice"):
            reports["practice"] = []
            futs["practice"] = pool.submit(test_practice, all_songs, reports["practice"])
        if want("views"):
            reports["views"] = []
            futs["views"] = pool.submit(test_views, all_songs, reports["views"])
        if want("levels"):
            reports["levels"] = []
            futs["levels"] = pool.submit(test_levels, all_songs, reports["levels"])
        if want("flash"):
            reports["flash"] = []
            futs["flash"] = pool.submit(test_flash, all_songs[0], reports["flash"])
        for name, fn in PART_TESTS.items():
            if want(name):                  # (the full suite: every part's checks too)
                reports[name] = []
                futs[name] = pool.submit(fn, all_songs, reports[name])
        for k, f in futs.items():
            results[k] = f.result()
    text = [f"# M5 emulator tests\n\nPackage: {PKG.relative_to(ROOT) if PKG.is_relative_to(ROOT) else PKG}, sha256 "
            f"{subprocess.run(['sha256sum', str(PKG)], capture_output=True, text=True).stdout.split()[0]}\n"]
    for k in reports:
        text += reports[k]
    text.append(f"\n**{'ALL PASS' if all(results.values()) else 'FAILURES: ' + ', '.join(k for k, v in results.items() if not v)}**\n")
    (OUT / "report.md").write_text("\n".join(text))
    print("\n".join(text))
    sys.exit(0 if all(results.values()) else 1)



# ------------------------------------------------------------------------------------------------ M5, per part
LAYER_ORDER = ("drums", "bass", "harmony", "melody")


def level_loudness(all_songs, flash="songs", pkg=None, prefix="lv", songs_only=None):
    """{slug: {layer: [rms per level]}}: each layer alone (the others pressed to MUTED), at each level, MASTER 100 %,
    12 s from the first bar"""
    from concurrent.futures import ThreadPoolExecutor
    jobs = []
    for i, c in enumerate(all_songs):
        if songs_only and c["slug"] not in songs_only:
            continue
        for li, layer in enumerate(LAYER_ORDER[:3]):
            n = len(c["layers"].get(layer, {}).get("variants", {}))
            for lv in range(1, n + 1):
                sc = ([f"4150:e0:{i}"] if i else []) + press_raw(OPEN_MS, "PLAY") + ["1500:pot:4096"] \
                    + press_raw(OPEN_MS + 100, "ARP")
                for k, b in enumerate(LAYERS_BTN):
                    if k != li:
                        sc += tap(OPEN_MS + 200 + 330 * k, b, 2)
                if lv > 1:
                    sc.append(f"{OPEN_MS + 1500}:e{2 + li}:{lv - 1}")
                jobs.append((c["slug"], layer, lv, sc))

    def one(j):
        slug, layer, lv, sc = j
        r = run(f"{prefix}-{slug}-{layer}{lv}", PLAY_MS + 13000, sc + press_raw(PLAY_MS, "PLAY"), flash=flash, pkg=pkg)
        return slug, layer, lv, rms(r["left"][int((PLAY_MS + 1000) / 1000 * RATE):])
    out = {}
    with ThreadPoolExecutor(4) as ex:
        for slug, layer, lv, v in ex.map(one, jobs):
            out.setdefault(slug, {}).setdefault(layer, {})[lv] = v
    return {s: {l: [d[k] for k in sorted(d)] for l, d in v.items()} for s, v in out.items()}


def test_m5p1(all_songs, rep):
    """M5 part 1: level loudness, fresh start, splash and its melody, the controls hint, the bass octave, the
    arpeggio's lights"""
    lines, st, check = checker("M5 part 1: fixes")
    # every level of a layer about as loud as the others (each layer alone, 12 s from the first bar)
    res = level_loudness(all_songs, prefix="m5p1-lv")
    worst = []
    for slug, v in res.items():
        for layer, xs in v.items():
            ys = [x for x in xs if x > 30]       # (a level silent in the first 12 s: not measured)
            ratio = max(ys) / min(ys) if len(ys) > 1 else 1
            worst.append((ratio, slug, layer, [round(x) for x in xs]))
            lines.append(f"  - {slug} {layer}: rms by level {[round(x) for x in xs]}, loudest / softest {ratio:.2f}")
    worst.sort(reverse=True)
    check(worst[0][0] <= 1.6, f"every layer's levels within 1.6x of each other in all {len(res)} songs (worst: "
                              f"{worst[0][1]} {worst[0][2]} {worst[0][0]:.2f})")
    slugs = [c["slug"] for c in all_songs]
    g = slugs.index("amazing-grace")
    cg = all_songs[g]
    O = OPEN_MS
    # the splash: 4 s, the start-up melody's 5 notes
    r1, r2 = run("m5p1-splash", 3900, []), run("m5p1-list", 4500, [])
    check(r1["stats"]["ui_view"] >= 16 and r2["stats"]["ui_view"] < 16 and r2["stats"]["splash_notes"] == 5,
          f"the splash for 4 s (still up at 3.9 s), the list at 4.3 s; the start-up melody: "
          f"{r2['stats']['splash_notes']} notes")
    # (M10.2: the controls hint screen "THE LAYER BUTTONS" is gone: its check m5p1-hint is removed)
    # a fresh start for every song: view, key, tempo
    r = run("m5p1-fresh", PLAY_MS + 1500, open_song(g) + press_raw(O + 300, "EDIT") + press_raw(O + 500, "SAVE")
            + [f"{O + 600}:e0:2"] + press_raw(O + 900, "SAVE") + [f"{O + 1100}:e0:-10"] + press_raw(O + 1400, "HOME")
            + press_raw(O + 1600, "PLAY") + press_raw(O + 1800, "PLAY") + press_raw(PLAY_MS + 600, "PLAY"))
    s = r["stats"]
    check(s["view"] == 0 and s["transpose"] == 0 and s["bpm"] == own_bpm(cg),
          f"view, key and tempo changed, the song closed and opened again: its own view, key and {own_bpm(cg)} BPM")
    # the bass in its real octave; it sounds +1 octave (the default setting) or real
    lo = min(e[2] for c in all_songs for v in c["layers"]["bass"]["variants"].values() for e in v if e[1] == 144)
    hi = max(e[2] for c in all_songs for v in c["layers"]["bass"]["variants"].values() for e in v if e[1] == 144)
    d = slugs.index("danny-boy")
    base = open_song(d) + press_raw(O + 100, "PLAY") + press_raw(O + 300, "ARP")
    up = run("m5p1-bass-up", PLAY_MS + 4000, base + press_raw(PLAY_MS, "PLAY"))
    sc, t = settings_script(O + 400, [(8, -1)])
    real = run("m5p1-bass-real", PLAY_MS + 4000, base + sc + press_raw(PLAY_MS + 1500, "PLAY"))
    a, b = up["stats"]["note_on_last"][1], real["stats"]["note_on_last"][1]
    check(28 <= lo and hi <= 52 and a - b in (12, 0) and a >= 40 and (real["stats"]["settings"] >> 28) & 15 == 0,
          f"the bass written in its real octave (all songs: {lo}..{hi}); sounding +1 octave by default (last note "
          f"{a}), REAL in the settings: {b}")
    run("m5p1-tab", PLAY_MS + 6000, base + hold(O + 500, "SCL") + [x for k in range(3) for x in
                                                                     press_raw(O + 1700 + 250 * k, "EDIT")]
        + press_raw(PLAY_MS + 300, "PLAY"))
    run("m5p1-bass-notation", PLAY_MS + 6000, base + hold(O + 500, "SCL") + [x for k in range(4) for x in
                                                                              press_raw(O + 1700 + 250 * k, "EDIT")]
        + press_raw(PLAY_MS + 300, "PLAY"))
    # harmony L5: every note the arpeggio plays in the chord lights (now bright, the rest at the middle level)
    r = run("m5p1-l5", PLAY_MS + 7000, open_song(g) + press_raw(O + 100, "PLAY") + press_raw(O + 200, "ARP")
            + [f"{O + 300}:e4:9"] + press_raw(PLAY_MS, "PLAY"))
    s = r["stats"]
    keys = bin(s["lit_keys"] | s["mid_keys"]).count("1")
    check(keys >= 4, f"harmony L5: {keys} keys lit (the note played and every other note of the chord's arpeggio)")
    run("m5p1-layout", PLAY_MS + 3000, open_song(g) + press_raw(O + 100, "PLAY") + [f"{O + 300}:e1:5"]
        + press_raw(O + 600, "SEQ") + press_raw(PLAY_MS, "PLAY"))
    rep += lines
    return st["ok"]


def long_song(bars_x, fmt, out):
    """a long test song (Four Chords' form repeated bars_x times, melody once) compiled in fmt -> its folder, bytes"""
    import yaml
    d = OUT / f"long-{bars_x}"
    if d.exists():
        shutil.rmtree(d)
    shutil.copytree(ROOT / "songs/four-chords", d)
    y = yaml.safe_load((d / "song.yaml").read_text())
    form = []
    for k in range(bars_x):
        for sec in y["form"]:
            a, b = (int(x) for x in sec["bars"].split("-"))
            form.append({**sec, "section": f"{sec['section']} {k + 1}"[:15], "bars": f"{a + 32 * k}-{b + 32 * k}"})
    y["form"] = form
    y["slug"], y["title"] = f"long-{bars_x}", f"Long Test {bars_x}"
    (d / "song.yaml").write_text(yaml.safe_dump(y, sort_keys=False))
    blob = compile_folder(d, fmt)[0]
    out.write_bytes(blob)
    return d, blob


def test_m5p2(all_songs, rep):
    """M5 part 2 (D029): format 2.0 (the FM-1 generates the levels; one level of each layer at a time), songs straight
    into the flash, the store in blocks (several songs a sector)"""
    lines, st, check = checker("M5 part 2: storage (format 2.0, the block store)")
    slugs = [c["slug"] for c in all_songs]
    O = OPEN_MS
    sizes = {c["slug"]: c["size"] for c in all_songs}
    check(all(c["version"] == "2.0" for c in all_songs),
          f"all {len(all_songs)} songs in format 2.0: {sum(sizes.values())} bytes together (M4's 1.3: 168,712)")
    recs = records(FLASH["songs"])
    blocks = sum(min((32 + n + 511) // 512, 8) for _, n in recs)
    secs = len({(o - AREA[0]) // 4096 for o, _ in recs})
    check(len(recs) == len(all_songs) and secs < len(recs),
          f"stored in {blocks} blocks of 512 bytes in {secs} sectors (several songs a sector; M4: 45 sectors)")
    # a level changed while playing: built on the FM-1 (the other layers play on, untouched)
    g = slugs.index("saints")
    c = all_songs[g]
    base = open_song(g) + press(PLAY_MS, "PLAY")
    a = run("m5p2-steady", PLAY_MS + 9000, base)
    b = run("m5p2-switch", PLAY_MS + 9000, base + [f"{PLAY_MS + 3000}:e4:4", f"{PLAY_MS + 6000}:e3:3"])
    sa, sb = a["stats"], b["stats"]
    check(sb["notes_on"][0] == sa["notes_on"][0] and sb["notes_on"][3] == sa["notes_on"][3] and
          sb["notes_on"][2] > sa["notes_on"][2] and sb["deferred"] == 0 and not b["halt"],
          f"harmony L1 -> L5 and bass L1 -> L4 while playing ({c['title']}): drums and melody note for note as "
          f"without ({sb['notes_on'][0]}, {sb['notes_on'][3]} notes), harmony {sa['notes_on'][2]} -> "
          f"{sb['notes_on'][2]} notes, no deferred block")
    k = run("m5p2-levels-end", int(PLAY_MS + song_ms(c) + 2500),
            open_song(g) + [f"{O + 200}:e4:9", f"{O + 300}:e3:9", f"{O + 400}:e2:9"] + press(PLAY_MS, "PLAY"))
    ks = k["stats"]
    start = (int(c["time_signature"][0]) - c["pickup_beats"]) * c["ppq"] if c["pickup_beats"] else 0
    want = ticks_to_frames(c, c["end_tick"] - ks["start_tick"])     # (from the count-in's first beat)
    got = ks["end_frames"] - ks["start_frames"]
    check(ks["ends"] == 1 and abs(got - want) <= BLOCK and ks["deferred"] == 0,
          f"at harmony L5, bass L4, drums L3 (generated on the FM-1): played to the end on time ({got} / {want:.0f})")
    # every level's notes as the laptop's generators make them: the FM-1's note count per layer at each level, 6 s
    bad = []
    for lv, knob in ((5, "e4"), (4, "e3"), (3, "e2")):
        r = run(f"m5p2-count-{lv}", PLAY_MS + 6000, open_song(g) + [f"{O + 200}:{knob}:{lv - 1}"] + press(PLAY_MS, "PLAY"))
        layer = {"e4": 2, "e3": 1, "e2": 0}[knob]
        name = LAYER_ORDER[layer]
        t1 = r["stats"]["last_tick"]
        evs = c["layers"][name]["variants"][lv]
        n = sum(1 for e in evs if e[1] == 144 and start <= e[0] < t1)
        if abs(r["stats"]["notes_on"][layer] - n) > 6:
            bad.append((name, lv, r["stats"]["notes_on"][layer], n))
    check(not bad, f"harmony L5, bass L4, drums L3: the FM-1 plays as many notes as the laptop's generators make "
                   f"(to within a block) {bad or ''}")
    # a file over M4's 64 KiB: straight into the flash
    f1 = OUT / "runs" / "long-4-v10.fm1s"
    _, big = long_song(4, (1, 0), f1)
    r = run("m5p2-big-send", 240000, [], flash="empty_quiet", host=f"{SEND} send --emu-pipe --no-validate {f1}")
    rb = records(r["flash"])
    check("stored (" in r["out"] and len(big) > 64 * 1024 and len(rb) == 1 and rb[0][1] == len(big),
          f"a {len(big)}-byte song (M4: at most 65,536, in RAM) received straight into the flash and stored")
    rp = run("m5p2-big-play", PLAY_MS + 4000, press(O, "PLAY") + press(PLAY_MS, "PLAY"), flash=r["flash"])
    check(rp["stats"]["starts"] == 1 and rms(rp["left"][int((PLAY_MS + 1500) / 1000 * RATE):]) > 50,
          "and it plays")
    # a refused file leaves nothing: a damaged copy (its CRC right, a chunk wrong)
    bad_f = OUT / "runs" / "bad.fm1s"
    bb = bytearray(compile_folder(ROOT / "songs/saints")[0])
    bb[4] = 3                                    # version 3.0: refused by the FM-1 after it is received
    struct.pack_into("<I", bb, 12, 0)
    struct.pack_into("<I", bb, 12, zlib.crc32(bytes(bb)))
    bad_f.write_bytes(bb)
    rr = run("m5p2-refused", 60000, [], host=f"{SEND} send --emu-pipe --no-validate {bad_f}")
    rl = run("m5p2-refused-list", 30000, [], flash=rr["flash"], host=f"{SEND} list --emu-pipe")
    check("NOT stored" in rr["out"] and f"{len(all_songs)} song(s)" in rl["out"] and
          len(records(rr["flash"])) == len(all_songs),
          "a file refused after it was received (format 3.0): not stored, nothing else changed")
    # M4's store: its records (FM1R, whole sectors, format 1.3) still read and played
    m4 = bytearray(FLASH["empty_quiet"].read_bytes())
    off = AREA[0]
    for sl in ("saints", "amazing-grace"):
        blob = compile_folder(ROOT / "songs" / sl, (1, 3))[0]
        h = bytearray(b"\xff" * 32)
        h[0:4] = b"FM1R"
        struct.pack_into("<III", h, 4, 1, len(blob), zlib.crc32(blob))
        struct.pack_into("<I", h, 28, zlib.crc32(bytes(h[:28])))
        m4[off:off + 32] = h
        m4[off + 32:off + 32 + len(blob)] = blob
        off += (32 + len(blob) + 4095) // 4096 * 4096
    fm4 = OUT / "runs" / "m4-store.flash"
    fm4.write_bytes(m4)
    l4 = run("m5p2-m4-list", 30000, [], flash=fm4, host=f"{SEND} list --emu-pipe")
    p4 = run("m5p2-m4-play", PLAY_MS + 4000, press(O, "PLAY") + press(PLAY_MS, "PLAY"), flash=fm4)
    s4 = run("m5p2-m4-send", 90000, [], flash=fm4, host=f"{SEND} send --emu-pipe {ROOT}/songs/saints")
    check("2 song(s)" in l4["out"] and p4["stats"]["starts"] == 1 and
          rms(p4["left"][int((PLAY_MS + 1500) / 1000 * RATE):]) > 50 and "stored (" in s4["out"] and
          len(records(s4["flash"])) == 2 and any(f[0:4] == b"FM2R" for f in [s4["flash"].read_bytes()[o:o + 4]
                                                                            for o, _ in records(s4["flash"])]),
          "an M4 unit's songs (FM1R records, format 1.3): listed and played; one sent again replaces its M4 copy")
    # the fallback: an M4 FM-1 (its own package) gets 1.3 from fm1song send, and plays it
    m4pkg = ROOT / "build/step-M4/final/fm1acc-0.5.0-m4.fwsc"
    if m4pkg.exists():
        m4flash = OUT / "flash" / "m4-empty.bin"          # (the emulator runs the app in the flash: M4's own)
        m4flash.unlink(missing_ok=True)
        subprocess.run([str(EMU), "--fwsc", str(m4pkg), "--flash", str(m4flash), "--bench", "1500"],
                       env={**os.environ, "FM1_BENCH_NOKEYS": "1"}, capture_output=True, check=True)
        fb = run("m5p2-m4-fallback", 90000, [], flash=str(m4flash), pkg=m4pkg,
                 host=f"{SEND} send --emu-pipe {ROOT}/songs/saints")
        fp = run("m5p2-m4-fallback-play", PLAY_MS + 4000, press(O, "PLAY") + press(PLAY_MS, "PLAY"),
                 flash=fb["flash"], pkg=m4pkg)
        rb = records(fb["flash"])
        v = fb["flash"].read_bytes()[rb[0][0] + 32 + 4:rb[0][0] + 32 + 6] if rb else b""
        check("sending 1.3" in fb["out"] and "stored (" in fb["out"] and v == bytes([1, 3]) and
              fp["stats"]["starts"] == 1 and rms(fp["left"][int((PLAY_MS + 1500) / 1000 * RATE):]) > 50,
              "the fallback: fm1song send to an M4 FM-1 (its own package) sends 1.3 (every level as events); it "
              "stores and plays it")
    # every song together: ours and (when on this machine) the private ones, whole
    priv = private_dirs()
    if priv:
        folders = " ".join(str(f) for f in song_dirs())
        ra = run("m5p2-all", 300000, [], flash="empty_quiet",
                 host=f"{SEND} send --emu-pipe {folders} " + " ".join(str(f) for f in priv))
        la = run("m5p2-all-list", 30000, [], flash=ra["flash"], host=f"{SEND} list --emu-pipe")
        n = len(all_songs) + len(priv)
        m = re.search(r"(\d+) of (\d+) blocks free", la["out"])
        free = int(m.group(1)) if m else 0
        check(ra["out"].count(": stored (") == n and f"{n} song(s)" in la["out"],
              f"all {n} songs on one FM-1 (ours and the {len(priv)} private ones, whole): {free} of 552 blocks still "
              f"free, room for about {free * 512 // 1300} more songs like ours")
        whole = [f.name for f in priv if "only the first" not in (f / "IMPORT-REPORT.txt").read_text()]
        check(len(whole) == len(priv), f"the private songs imported whole (no bars cut): {', '.join(whole)}")
    lines.append(f"  - sizes (2.0): " + ", ".join(f"{k} {v}" for k, v in sizes.items()))
    rep += lines
    return st["ok"]


def test_m5p3(all_songs, rep):
    """M5 part 3 (D030, trimmed: HANDOFF 2026-11-06 17:15): the mode screen, SYNTH mode on Felucca-Engines' ANALOG"""
    lines, st, check = checker("M5 part 3: the mode screen, SYNTH mode")
    m = run("m5p3-mode", 4700, [], mode=None)
    check(m["stats"]["ui_view"] == 6 and m["stats"]["mode"] == 0,
          "after the splash: the mode screen (the mascot, LEARN / SYNTH, the cursor on LEARN; m5p3-mode.png)")
    l = run("m5p3-learn", 4700, [])
    check(l["stats"]["ui_view"] == 0 and l["stats"]["mode"] == 1, "PLAY on LEARN: the song list, as before")
    pick = ["4050:e0:2", f"4200:PLAY", f"4260:PLAY^"]       # SELECT to SYNTH (M8: the third), PLAY
    m2 = run("m5p3-mode-synth-cursor", 4150, ["4050:e0:2"], mode=None)
    chord = ["5000:C4", "5000:E4", "5000:G4", "6500:C4^", "6500:E4^", "6500:G4^"]
    s1 = run("m5p3-synth", 7000, pick + chord, mode=None)
    ss = s1["stats"]
    snd = rms(s1["left"][int(5.2 * RATE):int(6.4 * RATE)])
    check(ss["mode"] == 2 and ss["ui_view"] == 7 and ss["synth_notes"] == 3 and snd > 300 and
          ss["synth_peak"] < 8388607, f"SELECT + PLAY: SYNTH (m5p3-synth.png); a chord of 3 keys: 3 voices, sound "
          f"(rms {snd:.0f}), no clipping (peak {ss['synth_peak']} of 8388607)")
    check(m2["stats"]["ui_view"] == 6, "the mode screen's cursor moves to SYNTH (m5p3-mode-synth-cursor.png)")
    # presets: M7: the PRESET knob (M5, M6: SELECT); every one sounds (5 keys held 1 s each)
    bad, peaks = [], []
    n = 30
    for i in range(0, n, 4):
        sc = pick + ([f"4600:e6:{i}"] if i else []) + [f"5000:{k}" for k in ("C4", "E4", "G4", "B4")] + \
            [f"6000:{k}^" for k in ("C4", "E4", "G4", "B4")]
        r1 = run(f"m5p3-preset-{i}", 6200, sc, mode=None)
        v = rms(r1["left"][int(5.1 * RATE):int(6.0 * RATE)])
        peaks.append((i, round(v)))
        if r1["stats"]["synth_preset"] != i or v < 50 or r1["stats"]["synth_peak"] >= 8388607:   # (RISER: a 6 s attack)
            bad.append((i, r1["stats"]["synth_preset"], round(v)))
    check(not bad, f"presets by PRESET (M7): each sounds, none clips (rms of 0, 4, .. 28: {peaks}) {bad or ''}")
    # the arpeggiator: a preset with one (OCT BOUNCE, 3rd; M8: 6th), and ARP on preset 1 (a press). M7: pattern and rate on the
    # ARP page (M5, M6: SEQ and GLO; SEQ is now the drum pattern menu, GLO the help)
    a1 = run("m5p3-arp-preset", 7000, pick + ["4600:e6:5", "5000:C4", "5000:G4", "6800:C4^", "6800:G4^"], mode=None)
    a2 = run("m5p3-arp-button", 7500, pick + ["4600:ARP", "4700:ARP^", "5000:C4", "5000:E4", "5000:G4",
                                              "7200:C4^", "7200:E4^", "7200:G4^"], mode=None)
    a3 = run("m5p3-arp-seq", 8400, pick + ["4600:ARP", "4700:ARP^", "4800:e1:-5", "5000:e2:2", "5400:e3:2",
                                           "6200:C4", "6200:G4", "8200:C4^", "8200:G4^"], mode=None)
    check(a1["stats"]["arp_steps"] >= 12 and a2["stats"]["arp_steps"] >= 3 and   # (1/16 at 120 BPM: 8 a second)
          a3["stats"]["arp_steps"] > a2["stats"]["arp_steps"],
          f"the arpeggiator: OCT BOUNCE's own up-down 1/16 ({a1['stats']['arp_steps']} steps in 1.8 s); ARP on "
          f"preset 1, up 1/8 at 120 BPM ({a2['stats']['arp_steps']} in 2.2 s); on the ARP page DOWN, 1/8T "
          f"({a3['stats']['arp_steps']} in 2 s; m5p3-arp-seq.png)")
    # the knobs and the octave (the screen: m5p3-knobs.png)
    run("m5p3-knobs", 6000, pick + ["4600:e2:-10", "4700:e3:10", "4800:e4:5", "4900:e5:-5", "5000:e1:4", "5100:OCTUP",
                                    "5200:OCTUP^"], mode=None)
    oc = run("m5p3-oct", 6000, pick + ["4600:OCTUP", "4700:OCTUP^", "5000:C4", "5500:C4^"], mode=None)
    check(oc["stats"]["key_note_last"] == 72, f"OCT+: the keys an octave up (C4 key -> {oc['stats']['key_note_last']})")
    # CPU: 8 voices of the busiest sound (SYNC SCREAM, 5th), chorus and delay on
    keys8 = ("C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5")
    cp = run("m5p3-cpu", 7000, pick + ["4600:e6:7"] + [f"5000:{k}" for k in keys8] + [f"6800:{k}^" for k in keys8],
             mode=None)
    c = cp["stats"]
    load = c["synth_render_max"] / c["budget"] if c["budget"] else 9
    check(c["synth_voices_max"] == 8 and load < 0.8 and not cp["halt"],
          f"8 voices of SYNC SCREAM with chorus and delay: the longest render {load * 100:.0f} % of its block time "
          f"(emulated CPU)")
    # learning and SYNTH never at once: a song sent in SYNTH mode is refused, nothing written
    f = OUT / "runs" / "saints-synth.fm1s"
    f.write_bytes(compile_folder(ROOT / "songs/saints")[0])
    sn = run("m5p3-send", 30000, pick, mode=None, host=f"{sys.executable} -I {ROOT}/tools/m2_host.py synth-send {f}")
    check("result 9" in sn["out"] and "SYNTH or BACKTRACK mode" in sn["out"] and   # (M8: the message names both)
          records(sn["flash"]) == records(FLASH["songs"]),
          "a song sent in SYNTH mode: refused (switch off, LEARN, send), the store unchanged")
    # an unknown flash chip: SYNTH still plays (it needs no flash)
    uf = run("m5p3-flash-unknown", 7000, pick + chord, flash_id="c84017", mode=None)
    check(uf["stats"]["mode"] == 2 and rms(uf["left"][int(5.2 * RATE):int(6.4 * RATE)]) > 300 and not uf["halt"],
          "unknown flash id c84017: SYNTH mode plays, no crash")
    rep += lines
    return st["ok"]


PART_TESTS.update({"m5p1": test_m5p1, "m5p2": test_m5p2, "m5p3": test_m5p3})


if __name__ == "__main__":
    main()
