# fm1song: songs to and from the FM-1's song store over USB-MIDI (M2 part 1). Licence: TBD (see D017)
#
# The FM-1 side is firmware/app/library.c (its comment has the commands). Finding the FM-1 and the SysEx framing come
# from fm1-base's tools/fm1_install.py (vendored in firmware/vendor/fm1-base, MPL-2.0), as fm1-base's own backup and
# resource tools use them. Only FM-1 Accompaniment firmware (identity FM-1_46...) is ever sent anything: any other
# firmware (stock, Felucca, an update loader) is refused before a byte of song goes out.
#
# send / delete WRITE to the FM-1's flash (its song area, 0x97000..0xDBFFF only). Per CLAUDE.md the user runs them on
# the hardware; Claude Code runs them only against the emulator (--emu-pipe, tools/m2_test.py).
import queue
import struct
import sys
import threading
import time
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "firmware/vendor/fm1-base/tools"))
import fm1_install as I  # noqa: E402

OURS = "FM-1_46"                       # our firmware's identity prefix (firmware/app/fm1.toml id_prefix)
SC_LIST, SC_ENTRY, SC_END, SC_SEND, SC_DELETE, SC_RESULT = 0x70, 0x71, 0x72, 0x73, 0x74, 0x7F
SC_WIPE = 0x75                         # M5.1: every song erased (the whole song store)
BUSY = 10                              # M5.1: "making room": the FM-1 compacts from its main loop; send again
RESULTS = {0: "ok", 1: "the FM-1's flash is not usable (RAM ONLY): it cannot keep songs",
           2: "the file is too big for the FM-1 (M5: 256 KiB at most; M4: 64 KiB)", 3: "the transfer stopped (USB)",
           4: "the file arrived damaged (CRC)", 5: "the FM-1 refused the song", 6: "the FM-1 could not store it",
           7: "no such song on the FM-1", 8: "the FM-1 did not understand the request (an older firmware?)",
           9: "the FM-1 is in SYNTH mode: switch it off and on, choose LEARN, then send",
           10: "the FM-1 is still making room (try again in a minute)"}
SONG_ERRORS = {-1: "SHORT", -2: "MAGIC", -3: "VERSION", -4: "SIZE", -5: "CRC", -6: "HEADER", -7: "DIRECTORY",
               -8: "MISSING", -9: "META", -10: "SECTIONS", -11: "CHORDS", -12: "CHANGES", -13: "BARS", -14: "LAYERS",
               -15: "EVENTS", -16: "TOO BIG"}
STORE_ERRORS = {-1: "the song area is full (delete a song first)", -2: "too big", -3: "a flash erase failed",
                -4: "a flash write failed", -5: "not found", -6: "outside the song area (refused)",
                -7: "no flash", -8: "a send out of order (try again)"}
WAIT = {"first": 5.0, "idle": 10.0, "room": 600.0, "again": 1.0}   # (M5.1: making room: at most 10 min, asked each 1 s)


class DeviceError(Exception):
    def __init__(self, code, msg):
        super().__init__(msg)
        self.code = code                # exit code: 3 not found, 4 lost / no answer, 5 refused, 6 not our firmware


def parse_frame(pkt):
    """a device message in the update's framing -> (cmd, body) or None"""
    if len(pkt) < 2 or pkt[0] != 0xF0 or pkt[-1] != 0xF7:
        return None
    u = I.unpack7(pkt[1:-1])
    if len(u) < 7 or u[:2] != b"\x00\x59":
        return None
    n = u[3] | u[4] << 8 | u[5] << 16
    if n + 7 != len(u) or ~sum(u[6:6 + n]) & 0xFF != u[6 + n]:
        return None
    return u[2], u[6:6 + n]


# ------------------------------------------------------------------ the emulator's pipe (fm1-live FM1_USB_HOST)

class PipeLink:
    """the emulated FM-1's USB-MIDI as stdin / stdout lines of hex (tools/patches/fm1-emu-usb-host.patch)"""

    def __init__(self):
        self.q = queue.Queue()
        self.lost = False
        threading.Thread(target=self._rx, daemon=True).start()

    def _rx(self):
        for line in sys.stdin:
            try:
                b = bytes.fromhex(line.strip())
            except ValueError:
                continue
            if b[:1] == b"\xF0":
                self.q.put(b)
        self.lost = True

    def send(self, pkt):
        sys.stdout.write(pkt.hex() + "\n")
        sys.stdout.flush()
        return True

    def read(self, timeout):
        try:
            return self.q.get(timeout=max(timeout, 0))
        except queue.Empty:
            return None

    def drain(self):
        while not self.q.empty():
            self.q.get_nowait()

    def close(self):
        pass


class PipeBackend:
    NAME = "FM-1 Accompaniment (emulator pipe)"

    def __init__(self):
        self.link = PipeLink()

    def input_names(self):
        return [self.NAME]

    def output_names(self):
        return [self.NAME]

    def open(self, in_name, out_name):
        return self.link


# ------------------------------------------------------------------ the commands

def connect(backend, port=None, tries=10):
    """our FM-1 on USB -> fm1_install's Device; DeviceError if there is none, or another firmware"""
    up = I.Updater(backend, port)
    dev = None
    for _ in range(tries):              # (an FM-1 just plugged in, or the emulator still starting)
        dev = up.find(only_port=bool(port))
        if dev:
            break
        time.sleep(0.5)
    if not dev:
        raise DeviceError(3, "no FM-1 found on USB (is it on, and running FM-1 Accompaniment?)")
    if not dev.id.text.startswith(OURS):
        dev.link.close()
        raise DeviceError(6, f"the FM-1 runs {dev.id.text}, not FM-1 Accompaniment ({OURS}...): nothing was sent")
    return dev


def _wait(link, want, serve=None, first=WAIT["first"]):
    """frames until want(cmd, body) returns something not None; serve: the file the FM-1 reads from"""
    last, idle = time.monotonic(), first
    while True:
        pkt = link.read(0.2)
        if pkt is None:
            if link.lost:
                raise DeviceError(4, "the FM-1 was disconnected")
            if time.monotonic() - last > idle:
                raise DeviceError(4, "the FM-1 stopped answering")
            continue
        last, idle = time.monotonic(), WAIT["idle"]
        if serve is not None:
            r = I.parse_request(pkt)
            if r:
                fl, addr, n = r
                if n > I.MAXDATA or addr + n > len(serve):
                    raise DeviceError(4, f"the FM-1 asked for {addr:#x}+{n}, outside the file")
                link.send(I.response(addr, serve[addr:addr + n], fl))
                continue
        f = parse_frame(pkt)
        if f:
            got = want(*f)
            if got is not None:
                return got


def song_list(dev):
    """-> ([{slug, len, sectors, blocks, status}], {count, free, largest, sectors, minor, result, major, free_blocks,
    blocks}) (M5's FM-1: major 2, it reads 2.0; the store in 512-byte blocks; older: major 1, blocks None)"""
    entries = []

    def want(cmd, body):
        if cmd == SC_ENTRY and len(body) == 32:
            entries.append({"slug": body[7:29].split(b"\0")[0].decode("ascii", "replace"),
                            "len": body[4] | body[5] << 8 | body[6] << 16, "sectors": body[3],
                            "blocks": body[30] | body[31] << 8 or None,
                            "status": ("ok", "DAMAGED (record CRC)", "UNREADABLE")[min(body[2], 2)]})
        elif cmd == SC_END and len(body) >= 6:
            m5 = len(body) >= 11
            return {"count": body[0], "free": body[1], "largest": body[2], "sectors": body[3], "minor": body[4],
                    "result": body[5], "major": body[10] if m5 else 1,
                    "free_blocks": body[6] | body[7] << 8 if m5 else None, "blocks": body[8] | body[9] << 8 if m5 else None,
                    "mode": body[11] if len(body) >= 12 else None,
                    "minor2": body[12] if len(body) >= 13 else 0}   # M7: it reads 2.0 .. 2.minor2 (2.1: start levels)
        return None

    dev.link.drain()
    dev.link.send(I.frame(SC_LIST))
    end = _wait(dev.link, want)
    return entries, end


def _result(cmd):
    def want(c, body):
        if c == SC_RESULT and len(body) >= 5 and body[0] == cmd:
            return {"result": body[1], "detail": struct.unpack("b", body[2:3])[0], "songs": body[3], "free": body[4],
                    "free_blocks": body[5] | body[6] << 8 if len(body) >= 7 else None}
        return None
    return want


def explain(r):
    text = RESULTS.get(r["result"], f"result {r['result']}")
    if r["result"] == 5:
        text += f" ({SONG_ERRORS.get(r['detail'], r['detail'])})"
    elif r["result"] == 6:
        text += f" ({STORE_ERRORS.get(r['detail'], r['detail'])})"
    return text


def song_send(dev, blob, slug=None, busy=None):
    """one compiled song into the FM-1's store -> its result dict (result 0: stored). slug: an M5 FM-1 (it reads 2.0)
    is told the song's slug, so the song's older copy can make way if the store has no room for both. M5.1: an FM-1
    that must compact first answers BUSY at once and makes the room from its main loop (the screen shows it); the
    send is then asked again each second until it goes through (busy(steps_left): told each time)"""
    body = struct.pack("<II", len(blob), zlib.crc32(blob))
    if slug is not None:
        body += slug.encode("ascii")[:23].ljust(24, b"\0")
    t0 = time.monotonic()
    while True:
        dev.link.drain()
        dev.link.send(I.frame(SC_SEND, body))
        r = _wait(dev.link, _result(SC_SEND), serve=blob, first=60.0)   # (M5: it may compact the store first)
        if r["result"] != BUSY or time.monotonic() - t0 > WAIT["room"]:
            return r
        if busy:
            busy(r["detail"])
        time.sleep(WAIT["again"])


def song_wipe(dev):
    """M5.1: every song on the FM-1 erased (its whole song store, 0x97000..0xDBFFF) -> the result dict"""
    dev.link.drain()
    dev.link.send(I.frame(SC_WIPE, b"WIPE"))
    return _wait(dev.link, _result(SC_WIPE), first=60.0)


def song_delete(dev, slug):
    raw = slug.encode("ascii")
    if not 1 <= len(raw) <= 23:
        raise DeviceError(2, f"{slug!r}: a song's slug is 1..23 characters")
    dev.link.drain()
    dev.link.send(I.frame(SC_DELETE, raw + b"\0"))
    return _wait(dev.link, _result(SC_DELETE))
