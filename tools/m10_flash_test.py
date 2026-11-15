#!/usr/bin/env python3
# m10_flash_test.py -- M10 Part 7 (D040): saving user presets on the TIMED emulator (tools/m51_test.py's runner: erases 8 ms,
# page programs 2 ms, a read of the XIP window while a flash command has it off stalls until the watchdog resets, as on the
# chip). Laptop and emulator only: never touches the FM-1.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   .venv/bin/python -I tools/m10_flash_test.py [--pkg PKG] [--out DIR] [--jobs 3]     -> <out>/report.md
#
# Scenarios: 30 presets saved in one run (the watchdog, the unfed stretch, the song store byte for byte, the object read back);
# a power cut during a save at every one of the 20 flash operations of two saves (both copies of the A/B pair), a boot after each;
# junk in 0xDE000 at boot; the object untouched by sends, deletes and WIPE (and the song store by a save).
import argparse
import struct
import sys
import zlib
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import m51_test as m51                   # noqa: E402  (sets FM1_FLASH_TIMED=1)
import m5_test as m5                     # noqa: E402
import m10_test                      # noqa: E402,F401  (its stats parser: the M10 words)

OBJ_LO, OBJ_HI = 0xDE000, 0xE0000
P = m5.press_raw
SYNTH_PRE = ["4050:e0:2", "4200:PLAY", "4260:PLAY^"]
timed, healthy = m51.timed, m51.healthy
V_SYNTH = 7


def synth(name, ms, script, **kw):
    return timed(name, ms, SYNTH_PRE + script, mode=None, **kw)


def read_object(flash):
    """the preset object as fm1-base's store keeps it: [(slot, name)] of the current copy (the valid one with the highest seq),
    or None; also the copies' seqs"""
    f = Path(flash).read_bytes()
    best, seqs = None, []
    for copy in (0, 1):
        o = OBJ_LO + copy * 4096
        h = f[o:o + 32]
        magic, typ, slot, seq, ln, crc, r1, r2, hcrc = struct.unpack("<IHHIIIIII", h)
        if magic != 0x30583058 or typ != 2 or hcrc != zlib.crc32(h[:28]) or ln > 3840:
            seqs.append(None)
            continue
        pl = f[o + 256:o + 256 + ln]
        if zlib.crc32(pl) != crc:
            seqs.append(None)
            continue
        seqs.append(seq)
        if best is None or seq > best[0]:
            best = (seq, pl)
    if best is None:
        return None, seqs
    pl = best[1]
    if len(pl) != 8 + 30 * 64 or pl[:4] != b"UPRS":
        return None, seqs
    out = []
    for s in range(30):
        r = pl[8 + s * 64:8 + s * 64 + 64]
        if r[0]:
            out.append((s + 1, r[2:14].split(b"\0")[0].decode()))
    return out, seqs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pkg")
    ap.add_argument("--out")
    ap.add_argument("--jobs", type=int, default=3)
    a = ap.parse_args()
    m5.OUT = Path(a.out).resolve() if a.out else ROOT / "build/emu/m10flash"
    if a.pkg:
        m5.PKG = Path(a.pkg).resolve()
    m5.OUT.mkdir(parents=True, exist_ok=True)
    m5.prepare_flash()
    songs = str(m5.FLASH["songs"])
    lines, st, check = m5.checker("M10 Part 7: user presets on the timed emulator (D040)")

    # 1. 30 saves in one run
    sc = []
    t = 5000
    for n in range(30):
        sc += [f"{t}:e6:1"] + P(t + 60, "SAVE") + P(t + 360, "SAVE")      # PRESET +1 (a new name each time), SAVE, SAVE
        t += 800
    r30 = synth("m10f-30", t + 800, sc, flash=songs)
    obj, seqs = read_object(r30["flash"])
    names = [n for _, n in obj] if obj else []
    check(obj is not None and len(obj) == 30 and [s for s, _ in obj] == list(range(1, 31)) and healthy(r30) and
          r30["stats"]["preset_saves"] == 30,
          f"30 saves, each into the next empty slot: the object read back from the flash has all 30 ({len(obj or [])}); "
          f"watchdog resets {r30['wdt']}, XIP reads with the flash command running {r30['xip']}, longest unfed stretch "
          f"{r30['gap']} ms (limit {m51.GAP_LIMIT_MS}), saves {r30['stats']['preset_saves']}, flash operations {r30['ops']}")
    check(m51.area(r30["flash"]) == m51.area(songs),
          "the song store is byte for byte what it was (a save touches nothing of it; the settings sector 0xFC000 too: "
          f"{Path(r30['flash']).read_bytes()[0xFC000:0xFE000] == Path(songs).read_bytes()[0xFC000:0xFE000]})")
    base_img = r30["flash"]

    # 2. a power cut at every flash operation of two saves (one preset in the flash first: copy A holds seq 1)
    one = synth("m10f-one", 6600, P(5000, "SAVE") + P(5300, "SAVE"), flash=songs)
    # SAVE (the screen), SAVE (saves: the sound is U02 now), PRESET -2 (a stock preset again), SAVE, SAVE (U03)
    pair = P(5000, "SAVE") + P(5300, "SAVE") + ["6000:e6:-2"] + P(6100, "SAVE") + P(6400, "SAVE")
    full = synth("m10f-two-full", 8000, pair, flash=str(one["flash"]))
    total = full["ops"]
    counts, bad = [], []
    with ThreadPoolExecutor(a.jobs) as ex:
        futs = [(k, ex.submit(lambda k=k: synth(f"m10f-cut-{k}", 8000, pair, flash=str(one["flash"]), cut=k))) for k in
                range(1, total + 1)]
        cuts = [(k, f.result()) for k, f in futs]
        boots = [(k, ex.submit(lambda k=k, c=c: synth(f"m10f-boot-{k}", 6000, [], flash=str(c["flash"])))) for k, c in cuts]
        boots = [(k, f.result()) for k, f in boots]
    for (k, c), (_, b) in zip(cuts, boots):
        o, _ = read_object(c["flash"])
        n = b["stats"]["preset_used"]
        counts.append(n)
        if not (healthy(c) and healthy(b) and not b["halt"] and b["stats"]["ui_view"] == V_SYNTH and o is not None and
                len(o) == n and 1 <= n <= 3):
            bad.append((k, n, c["wdt"], c["xip"], c["gap"], b["wdt"]))
    check(not bad and total == 20 and counts == sorted(counts) and counts[-1] in (2, 3) and counts[0] == 1,
          f"a power cut at each of the {total} flash operations of two saves (an erase and 9 page writes each; copy B, then copy "
          f"A erased again): every boot afterwards finds a whole object (presets after each cut {counts}), no watchdog "
          f"reset, no XIP read with a flash command running, unfed stretch under {m51.GAP_LIMIT_MS} ms {bad[:3] or ''}")

    # 3. junk in 0xDE000 at boot; the object untouched by sends, deletes, WIPE
    import random
    f = bytearray(Path(songs).read_bytes())
    rnd = random.Random(9)
    f[OBJ_LO:OBJ_HI] = bytes(rnd.randrange(256) for _ in range(OBJ_HI - OBJ_LO))
    junk = m5.OUT / "flash" / "junk-obj.bin"
    junk.write_bytes(f)
    jb = synth("m10f-junk", 6000, [], flash=str(junk))
    check(jb["stats"]["preset_used"] == 0 and jb["ops"] == 0 and healthy(jb) and not jb["halt"],
          f"junk in 0xDE000..0xDFFFF at boot: no preset, no crash, no flash operation ({jb['ops']}), no watchdog reset")
    obj_before = Path(base_img).read_bytes()[OBJ_LO:OBJ_HI]
    snd = timed("m10f-send", 90000, [], flash=str(base_img), host=m51.send([ROOT / "songs/saints"]))
    dl = timed("m10f-delete", 60000, [], flash=str(snd["flash"]), host=f"{m5.SEND} delete --emu-pipe saints")
    wp = timed("m10f-wipe", 60000, [], flash=str(dl["flash"]), host=f"{m5.SEND} wipe --emu-pipe --yes")
    same = [Path(x["flash"]).read_bytes()[OBJ_LO:OBJ_HI] == obj_before for x in (snd, dl, wp)]
    check(all(same) and "stored" in snd["out"] and "song store wiped" in wp["out"] and healthy(snd) and healthy(dl) and healthy(wp),
          f"the preset object is untouched by a send, a delete and WIPE SONG STORE (byte for byte: {same}); the preset list "
          f"after the WIPE: {len(read_object(wp['flash'])[0] or [])} presets")
    boot = synth("m10f-boot-saved", 5600, [], flash=str(base_img))
    check(boot["stats"]["preset_used"] == 30 and healthy(boot),
          f"a power cycle with 30 saved: all 30 are there ({boot['stats']['preset_used']})")
    lines.append(f"  - longest unfed stretch over every run: {max(r[3] for r in m51.RUNS)} ms; watchdog resets {sum(r[1] for r in m51.RUNS)}; "
                 f"XIP reads with the flash command running {sum(r[2] for r in m51.RUNS)} ({len(m51.RUNS)} runs)")
    text = ["# M10 timed-flash tests (presets)\n", f"Package: {m5.PKG}\n"] + lines + \
           [f"\n**{'ALL PASS' if st['ok'] else 'FAILURES'}**\n"]
    (m5.OUT / "report.md").write_text("\n".join(text))
    print("\n".join(text))
    sys.exit(0 if st["ok"] else 1)


if __name__ == "__main__":
    main()
