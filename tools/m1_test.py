#!/usr/bin/env python3
# m1_test.py -- Milestone M1b: headless emulator tests of the firmware, for every bundled song. Never touches the FM-1.
# Licence: TBD (see D017)
#
#   .venv/bin/python -I tools/m1_test.py [--jobs N] [--only SLUG]
#
# Runs firmware/app/build/fm1acc.fwsc in fm1-live (third_party/fm1-emu; deterministic emulated clock) and reads the
# player's numbers from a RAM dump (struct player_stats, "PLST", firmware/app/player.h). Writes build/emu/m1/:
# <slug>-*.png (the screen), report.md (every number). Per song:
#   full   count-in on; bass level 2 at 5 s; harmony muted 10..18 s; drums muted from 12 s; plays to the end:
#          - the end is reached once, at the frame the song's length predicts (within one 256-frame block);
#          - every chord change was on the screen (DMA done) no later than the block that played it (player.h);
#          - muted note-ons were held back; no EAS voice is left 1 s after the end (no hanging notes);
#          - no block had to defer events
#   tempo  count-in off, tempo 50 % from the start, 120 % at 6 s, stopped by the run's end at 20 s:
#          - the position is exactly what 50 % then 120 % give for the frames played (no drift), and the frames
#            played match the emulated time
#   loop   count-in off, SEQ twice at 3 s (loop the section: the first); run 1.4 sections:
#          - it jumped, from end - lead back to start - lead of section 1 (the pickup kept), never reached the end
# and for one song: flash ids 856014 and b36014 (same screen, sound) and an unknown id (RAM ONLY, sound, no crash).
import argparse
import math
import os
import re
import struct
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from fm1song import compile_folder, read_compiled   # noqa: E402

EMU = ROOT / "third_party/fm1-emu/target/release/fm1-live"
PKG = ROOT / "firmware/app/build/fm1acc.fwsc"
OUT = ROOT / "build/emu/m1"
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


def run(name, ms, script, flash_id=None):
    d = OUT / "runs"
    d.mkdir(parents=True, exist_ok=True)
    env = {**os.environ, "FM1_BENCH_KEYS": ",".join(script), "FM1_BENCH_WAV": str(d / f"{name}.raw"),
           "FM1_BENCH_PANEL": str(d / f"{name}.ppm"), "FM1_DUMPRAM": str(d / f"{name}.ram")}
    env.pop("FM1_FLASH_ID", None)
    if flash_id:
        env["FM1_FLASH_ID"] = flash_id
    r = subprocess.run([str(EMU), "--fwsc", str(PKG), "--bench", str(ms)], env=env, capture_output=True, text=True)
    out = r.stdout + r.stderr
    m = re.search(r"panel at (\d+) ms", out)
    if r.returncode or not m:
        raise SystemExit(f"run {name}: fm1-live failed:\n{out[-2000:]}")
    raw = (d / f"{name}.raw").read_bytes()
    left = struct.unpack(f"<{len(raw) // 2}h", raw[:len(raw) // 2 * 2])[0::2]
    img = Image.open(d / f"{name}.ppm").convert("RGB").crop(LCD)
    img.save(OUT / f"{name}.png")
    return {"name": name, "ms": int(m.group(1)), "left": left, "lcd": img, "stats": stats((d / f"{name}.ram").read_bytes()),
            "halt": "halt:" in out}


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


def test_flash(c, rep):
    lines, ok = ["\n## Flash chip ids (Amazing Grace)\n"], True
    script = press(OPEN_MS, "PLAY") + press(PLAY_MS, "PLAY")
    runs = {fid: run(f"flash-{fid}", PLAY_MS + 4000, script, flash_id=fid) for fid in ("856014", "b36014", "c84017")}
    lst = {fid: run(f"flash-list-{fid}", 1500, [], flash_id=fid) for fid in runs}
    for fid, r in runs.items():
        snd = rms(r["left"][int((PLAY_MS + 1500) / 1000 * RATE):])
        lines.append(f"- {fid}: halt {r['halt']}, sound rms {snd:.0f}, stats {'found' if r['stats'] else 'missing'}")
        ok &= not r["halt"] and snd > 50 and r["stats"] is not None
    same = ImageChops.difference(lst["856014"]["lcd"], lst["b36014"]["lcd"]).getbbox() is None
    diff = ImageChops.difference(lst["856014"]["lcd"], lst["c84017"]["lcd"]).getbbox() is not None
    lines.append(f"- {'PASS' if same else 'FAIL'}: 856014 and b36014 show the same list screen")
    lines.append(f"- {'PASS' if diff else 'FAIL'}: unknown id c84017 shows RAM ONLY (flash-list-c84017.png) and "
                 f"still plays")
    rep += lines
    return ok and same and diff


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
        raise SystemExit("m1_test: build first (make firmware)")
    OUT.mkdir(parents=True, exist_ok=True)
    all_songs = songs()
    reports, results = {}, {}
    with ThreadPoolExecutor(a.jobs) as pool:
        futs = {}
        for i, c in enumerate(all_songs):
            if a.only and c["slug"] != a.only:
                continue
            reports[c["slug"]] = []
            futs[c["slug"]] = pool.submit(test_song, i, c, reports[c["slug"]])
        if not a.only:
            reports["flash"] = []
            futs["flash"] = pool.submit(test_flash, all_songs[0], reports["flash"])
        for k, f in futs.items():
            results[k] = f.result()
    text = [f"# M1 emulator tests\n\nPackage: {PKG.relative_to(ROOT) if PKG.is_relative_to(ROOT) else PKG}, sha256 "
            f"{subprocess.run(['sha256sum', str(PKG)], capture_output=True, text=True).stdout.split()[0]}\n"]
    for k in reports:
        text += reports[k]
    text.append(f"\n**{'ALL PASS' if all(results.values()) else 'FAILURES: ' + ', '.join(k for k, v in results.items() if not v)}**\n")
    (OUT / "report.md").write_text("\n".join(text))
    print("\n".join(text))
    sys.exit(0 if all(results.values()) else 1)


if __name__ == "__main__":
    main()
