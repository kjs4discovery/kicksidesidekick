#!/usr/bin/env python3
# proto_test.py -- PROTOTYPE (Step 0.7): headless emulator tests of the song prototype (A4, A5). Never touches the FM-1.
# Licence: TBD (see D017)
#
#   .venv/bin/python -I tools/proto_test.py [--jobs N]
#
# Runs firmware/app/build/fm1acc.fwsc in fm1-live (third_party/fm1-emu, deterministic emulated clock) and writes
# build/emu/proto/: harmony-muted.wav (the whole song, harmony muted), bar-*.png (the screen at a few bars),
# flash-*.png (the three flash ids), report.md (every number below). Checks:
#   1. harmony muted: the harmony's voices are gone (fewer voices per bar than with every layer on) and the audio
#      differs from the all-layers run only after the song starts;
#   2. the chord on the screen changes no later than the audio: for a few chord changes, runs that end just before
#      and just after the new chord's first sample is output (the emulated I2S) must show the new chord whenever that
#      sample is already in their audio;
#   3. flash id 856014 and b36014: the same screen, sound; an unknown id: RAM ONLY on the screen, sound, no crash;
#   4. the load: per bar, voices, the platform's audio load and the longest EAS render, from a RAM dump.
import argparse
import math
import os
import re
import struct
import subprocess
import sys
import wave
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parent.parent
EMU = ROOT / "third_party/fm1-emu/target/release/fm1-live"
PKG = ROOT / "firmware/app/build/fm1acc.fwsc"
OUT = ROOT / "build/emu/proto"
SONG_H = ROOT / "firmware/app/build/gen/proto_song.h"
LCD = (370, 42, 850, 522)                      # the LCD in fm1-live's panel image (shown 2x)
CHORD_BAND = (370, 42 + 80, 850, 42 + 248)     # LCD rows 40..123: the current chord, large
RATE = 44100
PLAY_MS, ENV_MS, SEL_MS = 2000, 1600, 1700     # after the splash (~1.2 s): harmony (ENV), bass (SEL) muted, PLAY
SONG_MS = PLAY_MS + 72000 + 1500               # the whole song and its tail
DS = [-20, -10, 0, 10, 20, 30, 40, 60, 80, 120]  # chord test: runs ending d ms before the chord's first frame is output


def song_consts():
    t = SONG_H.read_text()
    c = {k: int(v) for k, v in re.findall(r"#define SONG_(\w+) (\d+)u", t)}
    names = re.search(r"SONG_CHORD_NAME\[\d+\] = \{(.*)\};", t).group(1)
    names = [n.strip().strip('"') for n in names.split(",")]
    chords = [(int(a), names[int(b)]) for a, b in re.findall(r"\{(\d+)u, (\d+)\},", t)]
    return c, chords


def keys(*events):
    return ",".join(f"{ms}:{k},{ms + 100}:{k}^" for ms, k in sorted(events))


def run(name, ms, events, flash_id=None, ram=False):
    """one headless run of ms emulated ms: its audio (stereo frames, left channel), panel image, RAM, end time"""
    d = OUT / "runs"
    d.mkdir(parents=True, exist_ok=True)
    env = {**os.environ, "FM1_BENCH_KEYS": keys(*events), "FM1_BENCH_WAV": str(d / f"{name}.raw"),
           "FM1_BENCH_PANEL": str(d / f"{name}.ppm")}   # FM1_BENCH_KEYS replaces the default keys
    env.pop("FM1_FLASH_ID", None)
    if flash_id:
        env["FM1_FLASH_ID"] = flash_id
    if ram:
        env["FM1_DUMPRAM"] = str(d / f"{name}.ram")
    r = subprocess.run([str(EMU), "--fwsc", str(PKG), "--bench", str(ms)], env=env, capture_output=True, text=True)
    out = r.stdout + r.stderr
    m = re.search(r"panel at (\d+) ms", out)
    if r.returncode or not m:
        raise SystemExit(f"run {name}: fm1-live failed:\n{out[-2000:]}")
    raw = (d / f"{name}.raw").read_bytes()
    left = struct.unpack(f"<{len(raw) // 2}h", raw[:len(raw) // 2 * 2])[0::2]
    return {"name": name, "ms": int(m.group(1)), "left": left, "raw": raw,
            "panel": Image.open(d / f"{name}.ppm").convert("RGB"),
            "ram": (d / f"{name}.ram").read_bytes() if ram else None,
            "halt": "halt" in out.lower() and "no halt" not in out.lower(), "log": out}


def rms(x):
    return math.sqrt(sum(v * v for v in x) / len(x)) if x else 0.0


def stats(ram):
    i = ram.find(b"PPST")
    if i < 0:
        return None
    magic, bars, blocks, rmax, budget = struct.unpack_from("<5I", ram, i)
    voices = struct.unpack_from(f"<{bars}H", ram, i + 20)
    cpu = struct.unpack_from(f"<{bars}B", ram, i + 20 + 2 * bars)
    render = struct.unpack_from(f"<{bars}I", ram, i + 20 + 3 * bars + (-(20 + 3 * bars)) % 4)
    return {"blocks": blocks, "render_max": rmax, "budget": budget, "voices": voices, "cpu": cpu, "render": render}


def save_wav(path, raw):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(raw)


def lcd(run_):
    return run_["panel"].crop(LCD)


def same(a, b):
    return ImageChops.difference(a, b).getbbox() is None


def onset(x, around, span):
    """the first frame of a note starting near frame `around` in signal x (one instrument): the block of 128 frames
    whose level jumps most against the one before, then its first frame above a tenth of that block's peak"""
    best, bi = 0.0, None
    for b in range(around - span, around + span, 128):
        if b < 256:
            continue
        cur, prev = rms(x[b:b + 128]), rms(x[b - 256:b - 128]) + 1.0
        if cur / prev > best:
            best, bi = cur / prev, b
    seg = x[bi - 128:bi + 128]
    peak = max(abs(v) for v in seg)
    for k, v in enumerate(seg):
        if abs(v) > peak / 10:
            return bi - 128 + k, best
    return bi, best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jobs", type=int, default=3)
    a = ap.parse_args()
    if not PKG.exists() or not SONG_H.exists():
        raise SystemExit("proto_test: build first (make firmware)")
    c, chords = song_consts()
    beat_ms = c["TEMPO_US"] / 1000
    tick_ms = beat_ms / c["PPQ"]
    bar_ticks = c["PPQ"] * c["BEATS_PER_BAR"]
    rep, ok = [], True
    pool = ThreadPoolExecutor(a.jobs)

    def check(cond, text):
        nonlocal ok
        rep.append(f"- {'PASS' if cond else 'FAIL'}: {text}")
        ok &= bool(cond)

    # 1. the whole song: every layer; harmony muted; harmony and bass muted (the bass alone is the difference)
    full = {k: pool.submit(run, k, SONG_MS, ev, None, True) for k, ev in {
        "all": [(PLAY_MS, "PLAY")],
        "harmony-muted": [(ENV_MS, "ENV"), (PLAY_MS, "PLAY")],
        "harmony-bass-muted": [(ENV_MS, "ENV"), (SEL_MS, "SCL"), (PLAY_MS, "PLAY")]}.items()}
    full = {k: f.result() for k, f in full.items()}
    hm = full["harmony-muted"]
    save_wav(OUT / "harmony-muted.wav", hm["raw"])
    st = {k: stats(r["ram"]) for k, r in full.items()}
    rep += ["# Step 0.7 prototype: emulator test report", "",
            f"Package: {PKG.relative_to(ROOT)}, emulator: fm1-live --bench (headless, emulated clock).", "",
            "## 1. Harmony muted", ""]
    check(all(s is not None for s in st.values()), "the stats block is in each run's RAM dump")
    va, vh = st["all"]["voices"], st["harmony-muted"]["voices"]
    nb = c["BARS"]
    fewer = sum(1 for b in range(1, nb) if vh[b] < va[b])
    check(fewer >= nb - 3, f"fewer voices with harmony muted in {fewer} of {nb - 1} bars "
                          f"(most: all layers {max(va)}, harmony muted {max(vh)})")
    first = next((i for i, v in enumerate(hm["left"]) if v), None)
    if first is None:
        raise SystemExit("proto_test: the harmony-muted run is silent (PLAY not taken?)")
    rep.append(f"- the first sound at frame {first} ({first / RATE * 1000:.0f} ms of audio)")
    la, lh = full["all"]["left"], hm["left"]
    n = min(len(la), len(lh))
    pre = next((i for i in range(n) if la[i] != lh[i]), n)
    check(pre >= first, f"the all-layers and harmony-muted audio are identical until the song starts (first "
                        f"difference at frame {pre})")
    check(rms(lh[first:]) > 300, f"harmony muted still plays: level {rms(lh[first:]):.0f} (all layers "
                                 f"{rms(la[first:]):.0f})")
    rep.append(f"- written: {(OUT / 'harmony-muted.wav').relative_to(ROOT)} ({len(hm['raw']) / 4 / RATE:.1f} s)")

    # timing: the audio frame of tick t, from the first sound (the pickup note at the pickup bar's last beat)
    t_first = (bar_ticks - c["PICKUP_BEATS"] * c["PPQ"])
    frame_of = lambda t: first + round((t - t_first) * tick_ms * RATE / 1000)
    # the audio frames vs the emulated time: frames output by the end of a run of ms (from the all-layers run)
    lag_ms = full["all"]["ms"] - len(la) / RATE * 1000
    rep.append(f"- audio output starts {lag_ms:.1f} ms after the emulated clock (fm1-live's WAV vs its end time)")

    # 2. screenshots at a few bars, and the chord vs the audio
    rep += ["", "## 2. Screen", ""]
    shot_bars = [(1, 1), (7, 2), (16, 3), (25, 1)]
    fut = {}
    for bar, beat in shot_bars:
        t = bar * bar_ticks + (beat - 1) * c["PPQ"] + c["PPQ"] // 2
        ms = round(lag_ms + frame_of(t) / RATE * 1000)
        fut[(bar, beat)] = pool.submit(run, f"bar-{bar}-{beat}", ms, [(ENV_MS, "ENV"), (PLAY_MS, "PLAY")])
    for (bar, beat), f in fut.items():
        r = f.result()
        lcd(r).save(OUT / f"bar-{bar:02d}-{beat}.png")
        rep.append(f"- bar {bar} beat {beat}: {(OUT / f'bar-{bar:02d}-{beat}.png').relative_to(ROOT)}")

    bass = [x - y for x, y in zip(lh, full["harmony-bass-muted"]["left"])]   # the bass (and its reverb) alone
    changes = [(t, s) for t, s in chords if t >= bar_ticks][1:]
    picks = [changes[0], changes[4], changes[9], next(x for x in changes if x[0] % bar_ticks), changes[20],
             changes[-1]]
    rep += ["", "Chord changes (the new chord's first audio frame from the bass alone; runs ending d ms before "
            "that frame is output):", "",
            "| chord | tick | first frame | " + " | ".join(f"d={d}" for d in DS) + " | lead |",
            "|---|---|---|" + "---|" * len(DS) + "---|"]
    ds = [-400] + DS
    for t, sym in picks:
        f0, jump = onset(bass, frame_of(t), 2048)
        t_audio = lag_ms + f0 / RATE * 1000          # the emulated time at which frame f0 is output
        runs = {d: pool.submit(run, f"chord-{t}-{d}", math.floor(t_audio - d), [(ENV_MS, "ENV"), (PLAY_MS, "PLAY")])
                for d in ds}
        runs = {d: f.result() for d, f in runs.items()}
        ref = runs[-400]["panel"].crop(CHORD_BAND)
        cells, lead = [], None
        for d in ds:
            r = runs[d]
            heard = len(r["left"]) > f0
            shown = same(r["panel"].crop(CHORD_BAND), ref)
            cells.append(("new" if shown else "old") + (" (heard)" if heard else ""))
            if heard and not shown:
                check(False, f"{sym} at tick {t}: its first frame is in the audio of the run ending at {r['ms']} ms "
                             f"but the screen still shows the old chord")
        for d in DS:                             # the lead: the largest d from 0 up with the new chord all the way
            if d < 0:
                continue
            if not same(runs[d]["panel"].crop(CHORD_BAND), ref):
                break
            lead = d
        rep.append(f"| {sym} | {t} | {f0} (x{jump:.0f}) | " + " | ".join(cells[1:]) + f" | {lead} ms |")
        check(lead is not None and lead >= 0, f"{sym} at tick {t}: on the screen {lead} ms or more before its first "
                                               f"frame is output")
    rep.append("")
    rep.append("'heard': the run's audio already holds the chord's first frame. A FAIL would be a 'heard' cell with "
               "the old chord.")

    # 3. flash ids
    rep += ["", "## 3. Flash ids", ""]
    ev = [(ENV_MS, "ENV"), (PLAY_MS, "PLAY")]
    ms = PLAY_MS + 6000
    fl = {fid: pool.submit(run, f"flash-{fid}", ms, ev, fid) for fid in ["856014", "b36014", "123456"]}
    fl = {k: f.result() for k, f in fl.items()}
    for k, r in fl.items():
        lcd(r).save(OUT / f"flash-{k}.png")
        lev = rms(r["left"][-RATE * 3:])
        check(lev > 300, f"flash id {k}: plays (level {lev:.0f} over the last 3 s), "
                         f"{(OUT / f'flash-{k}.png').relative_to(ROOT)}")
    check(same(lcd(fl["856014"]), lcd(fl["b36014"])), "856014 and b36014: identical screens (flash accepted)")
    top = (370, 42, 850, 42 + 80)
    check(not same(fl["856014"]["panel"].crop(top), fl["123456"]["panel"].crop(top)),
          "123456 (unknown): the top band differs (RAM ONLY shown; see the picture)")
    check(same(fl["856014"]["panel"].crop(CHORD_BAND), fl["123456"]["panel"].crop(CHORD_BAND)),
          "123456 (unknown): the same chord at the same time (the song plays on, no crash)")

    # 4. load
    rep += ["", "## 4. Load (emulator)", "",
            "Per bar: the most EAS voices, the platform's audio load (plat_cpu_pct, smoothed over ~16 blocks) and the "
            "longest EAS render in % of one block (256 frames, 5.8 ms).", "",
            "| bar | voices all | voices harmony muted | load all % | load harmony muted % | render all % |",
            "|---|---|---|---|---|---|"]
    sa, sh = st["all"], st["harmony-muted"]
    for b in range(nb):
        rep.append(f"| {b} | {sa['voices'][b]} | {sh['voices'][b]} | {sa['cpu'][b]} | {sh['cpu'][b]} | "
                   f"{100 * sa['render'][b] / sa['budget']:.1f} |")
    busiest = max(range(nb), key=lambda b: (sa["render"][b], sa["voices"][b]))
    rep += ["", f"Busiest bar (longest render, all layers): bar {busiest}: {sa['voices'][busiest]} voices, load "
            f"{sa['cpu'][busiest]} %, render {100 * sa['render'][busiest] / sa['budget']:.1f} % of the block. "
            f"Most voices anywhere: {max(sa['voices'])} of 24. Longest render: "
            f"{100 * sa['render_max'] / sa['budget']:.1f} %. Blocks played: {sa['blocks']}."]
    rep += ["", f"Result: {'ALL PASS' if ok else 'FAILURES above'}"]
    (OUT / "report.md").write_text("\n".join(rep) + "\n")
    print("\n".join(rep))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
