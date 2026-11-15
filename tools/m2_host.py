#!/usr/bin/env python3
# m2_host.py -- a test-only USB host for tools/m2_test.py: run by fm1-live's FM1_USB_HOST (stdin / stdout = the
# emulated FM-1's USB-MIDI). Never used with a real FM-1.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   m2_host.py stall FILE.fm1s N     start a send, answer the FM-1's first N read requests, then go quiet
#                                    (a cable pulled half way): the FM-1 must give up and store nothing
#   m2_host.py synth-send FILE.fm1s [MODE]   (M5) ask LIST until the FM-1 is in SYNTH mode (M8: or MODE, 3 BACKTRACK),
#                                       then send: it must refuse
import sys
import time
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fm1song import device  # noqa: E402


def stall(path, n):
    blob = Path(path).read_bytes()
    dev = device.connect(device.PipeBackend())
    dev.link.send(device.I.frame(device.SC_SEND, device.struct.pack("<II", len(blob), zlib.crc32(blob))))
    answered, t = 0, time.monotonic()
    while time.monotonic() - t < 30:
        pkt = dev.link.read(0.2)
        if not pkt:
            continue
        r = device.I.parse_request(pkt)
        if r and answered < n:
            fl, addr, k = r
            dev.link.send(device.I.response(addr, blob[addr:addr + k], fl))
            answered += 1
            continue
        f = device.parse_frame(pkt)
        if f and f[0] == device.SC_RESULT:
            print(f"result {f[1][1]}", file=sys.stderr)
            return 0
    print("no result", file=sys.stderr)
    return 1


def synth_send(path, mode=2):
    blob = Path(path).read_bytes()
    dev = device.connect(device.PipeBackend())
    for _ in range(200):
        end = device.song_list(dev)[1]
        if end.get("mode") == mode:
            r = device.song_send(dev, blob)
            print(f"result {r['result']}: {device.explain(r)}", file=sys.stderr)
            return 0
        time.sleep(0.2)
    print("never in SYNTH mode", file=sys.stderr)
    return 1


if __name__ == "__main__":
    if sys.argv[1:2] == ["stall"]:
        sys.exit(stall(sys.argv[2], int(sys.argv[3])))
    if sys.argv[1:2] == ["synth-send"]:
        sys.exit(synth_send(sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 2))   # (M8: 3 BACKTRACK)
    sys.exit("usage: m2_host.py stall FILE.fm1s N")
