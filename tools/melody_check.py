#!/usr/bin/env python3
# melody_check.py -- M2 part 2 item 7 ("crackle in Four Chords"): the melody of a song alone, on several GM
# instruments, in the emulator; for each the peak, how much of the sound is above 8 kHz (harsh, aliasing on EAS's
# 22.05 kHz synth) and above 11 kHz. Emulator only. Licence: TBD (see D017)
#
#   melody_check.py SONG_DIR PROGRAM...       GM programs counted from 1 (81 = Lead 1, square)
import sys
import struct
import zlib
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import m2_test as T  # noqa: E402
from fm1song import compile_folder, read_compiled  # noqa: E402
from fm1song.binformat import DIRENT  # noqa: E402


def variant(blob, program, k):
    """the song with the melody's program changed and a slug of its own (so each is stored as another song)"""
    b = bytearray(blob)
    n = struct.unpack_from("<H", b, 6)[0]
    for i in range(n):
        cc, off, ln = DIRENT.unpack_from(b, 32 + 12 * i)
        if cc == b"LAYR":
            b[off + 4 + 3 * 8 + 1] = program - 1          # melody = the 4th layer record, its program byte
        if cc == b"META":
            slug = f"mel{k:02d}-{program}".encode()
            b[off + 4:off + 28] = slug + bytes(24 - len(slug))
            title = f"Melody {k:02d} prog {program}".encode()
            b[off + 28:off + 60] = title + bytes(32 - len(title))
    b[12:16] = bytes(4)
    b[12:16] = struct.pack("<I", zlib.crc32(bytes(b)))
    return bytes(b)


def main():
    folder, progs = Path(sys.argv[1]), [int(p) for p in sys.argv[2:]]
    T.OUT.mkdir(parents=True, exist_ok=True)
    T.prepare_flash()
    blob = compile_folder(folder)[0]
    c = read_compiled(blob)
    d = T.OUT / "melody"
    d.mkdir(exist_ok=True)
    files = []
    for k, p in enumerate(progs):
        f = d / f"mel{k:02d}.fm1s"
        f.write_bytes(variant(blob, p, k))
        files.append(str(f))
    r = T.run("melody-send", 120000, [], flash="empty", host=f"{T.SEND} send --emu-pipe --no-validate {' '.join(files)}")
    ms = int(T.PLAY_MS + min(T.song_ms(c, count_in=False), 16000))
    print(f"{c['title']}: melody alone (drums, bass, harmony muted), MASTER 100 %, {ms - T.PLAY_MS} ms")
    for k, p in enumerate(progs):              # the list is by title: "Melody 00 ..", "Melody 01 ..": index k
        script = T.open_song(k) + ["1500:pot:4096"] + T.press(T.OPEN_MS + 300, "ARP") \
            + T.press(T.OPEN_MS + 400, "FX") + T.press(T.OPEN_MS + 600, "SCL") + T.press(T.OPEN_MS + 800, "ENV") \
            + T.press(T.PLAY_MS, "PLAY")
        run = T.run(f"melody-{p}", ms, script, flash=r["flash"])
        x = np.array(run["left"][int((T.PLAY_MS + 300) / 1000 * T.RATE):], dtype=float)
        spec = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
        f = np.fft.rfftfreq(len(x), 1 / T.RATE)
        tot = spec.sum() or 1.0
        print(f"  program {p:3}: peak {np.abs(x).max():6.0f}, rms {np.sqrt((x ** 2).mean()):6.0f}, "
              f"energy above 8 kHz {100 * spec[f > 8000].sum() / tot:5.2f} %, above 11 kHz "
              f"{100 * spec[f > 11025].sum() / tot:5.2f} %")


if __name__ == "__main__":
    main()
