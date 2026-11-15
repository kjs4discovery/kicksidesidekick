# fm1song: a small Standard MIDI File reader.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Our own reader (not mido) so every malformed byte gives a clear SongError with its offset instead of an
# arbitrary exception, and so the website can reuse it without extra packages. Reads formats 0 and 1 with
# ticks-per-quarter timing; keeps track names, notes, controllers, pitch bend, program changes and the
# tempo / time signature / key meta events. Everything else (SysEx, other meta events) is skipped.
from .errors import Diagnostics

MAX_TRACKS = 64
MAX_EVENTS = 200_000


class Event:
    __slots__ = ("tick", "kind", "channel", "a", "b", "data")

    def __init__(self, tick, kind, channel=None, a=0, b=0, data=None):
        self.tick = tick          # absolute tick in the track
        self.kind = kind          # "on" "off" "cc" "bend" "program" "pressure" "tempo" "timesig" "key" "name" "text"
        self.channel = channel    # 0..15 for channel events
        self.a, self.b = a, b     # note/velocity, controller/value, bend lsb/msb, program, ...
        self.data = data          # meta payload (bytes) where it applies

    def __repr__(self):
        return f"Event({self.tick}, {self.kind}, ch={self.channel}, {self.a}, {self.b})"


class Track:
    def __init__(self, index):
        self.index = index
        self.name = None
        self.events = []


class MidiFile:
    def __init__(self, fmt, ppq, tracks):
        self.format, self.ppq, self.tracks = fmt, ppq, tracks


def _fail(diag, offset, msg):
    diag.fatal(msg, "song.mid", f"byte {offset}")


def _u32(buf, pos):
    return int.from_bytes(buf[pos:pos + 4], "big")


def read_midi(data, diag=None):
    """bytes -> MidiFile; raises SongError on anything malformed"""
    diag = diag or Diagnostics()
    data = bytes(data)
    if len(data) < 14 or data[:4] != b"MThd":
        _fail(diag, 0, "not a Standard MIDI File (no 'MThd' header)")
    hlen = _u32(data, 4)
    if hlen < 6 or 8 + hlen > len(data):
        _fail(diag, 4, f"bad header length {hlen}")
    fmt = int.from_bytes(data[8:10], "big")
    ntracks = int.from_bytes(data[10:12], "big")
    division = int.from_bytes(data[12:14], "big")
    if fmt not in (0, 1):
        _fail(diag, 8, f"MIDI format {fmt} is not supported (use format 1, or 0)")
    if division & 0x8000:
        _fail(diag, 12, "SMPTE timing is not supported (use ticks per quarter note)")
    if not 24 <= division <= 960:
        _fail(diag, 12, f"{division} ticks per quarter note: must be 24..960")
    if not 1 <= ntracks <= MAX_TRACKS:
        _fail(diag, 10, f"{ntracks} tracks: must be 1..{MAX_TRACKS}")
    pos = 8 + hlen
    tracks, total = [], 0
    for ti in range(ntracks):
        # skip unknown chunks between tracks, as the SMF spec asks
        while True:
            if pos + 8 > len(data):
                _fail(diag, pos, f"file ends before track {ti + 1} of {ntracks}")
            cid, clen = data[pos:pos + 4], _u32(data, pos + 4)
            if pos + 8 + clen > len(data):
                _fail(diag, pos, f"chunk {cid!r} runs past the end of the file")
            if cid == b"MTrk":
                break
            pos += 8 + clen
        track = _read_track(data, pos + 8, pos + 8 + clen, ti, diag)
        total += len(track.events)
        if total > MAX_EVENTS:
            _fail(diag, pos, f"more than {MAX_EVENTS} events")
        tracks.append(track)
        pos += 8 + clen
    return MidiFile(fmt, division, tracks)


def _read_track(data, pos, end, index, diag):
    track = Track(index)
    tick, status = 0, None

    def vlq():
        nonlocal pos
        value = 0
        for _ in range(4):
            if pos >= end:
                _fail(diag, pos, f"track {index + 1}: ends inside a number")
            byte = data[pos]
            pos += 1
            value = (value << 7) | (byte & 0x7F)
            if not byte & 0x80:
                return value
        _fail(diag, pos, f"track {index + 1}: number longer than 4 bytes")

    def need(n):
        if pos + n > end:
            _fail(diag, pos, f"track {index + 1}: ends inside an event")

    while pos < end:
        tick += vlq()
        if tick > 0x0FFFFFFF:
            _fail(diag, pos, f"track {index + 1}: tick {tick} is too large")
        need(1)
        byte = data[pos]
        if byte == 0xFF:                                # meta event
            need(2)
            mtype = data[pos + 1]
            pos += 2
            mlen = vlq()
            need(mlen)
            payload = data[pos:pos + mlen]
            pos += mlen
            status = None
            if mtype == 0x2F:
                break
            if mtype == 0x03 and track.name is None:
                track.name = payload.decode("latin-1")
            elif mtype == 0x51:
                if mlen != 3:
                    _fail(diag, pos, f"track {index + 1}: tempo event of length {mlen}")
                track.events.append(Event(tick, "tempo", a=int.from_bytes(payload, "big")))
            elif mtype == 0x58:
                if mlen != 4:
                    _fail(diag, pos, f"track {index + 1}: time signature event of length {mlen}")
                track.events.append(Event(tick, "timesig", a=payload[0], b=1 << min(payload[1], 6)))
            elif mtype == 0x59:
                if mlen != 2:
                    _fail(diag, pos, f"track {index + 1}: key signature event of length {mlen}")
                track.events.append(Event(tick, "key", a=int.from_bytes(payload[:1], "big", signed=True),
                                          b=payload[1]))
            elif 0x01 <= mtype <= 0x07:
                track.events.append(Event(tick, "text", a=mtype, data=bytes(payload)))
            continue
        if byte in (0xF0, 0xF7):                        # SysEx: skipped
            pos += 1
            slen = vlq()
            need(slen)
            pos += slen
            status = None
            continue
        if byte & 0x80:
            if byte >= 0xF0:
                _fail(diag, pos, f"track {index + 1}: system message 0x{byte:02X} in a file")
            status = byte
            pos += 1
        elif status is None:
            _fail(diag, pos, f"track {index + 1}: data byte 0x{byte:02X} with no status before it")
        hi, ch = status & 0xF0, status & 0x0F
        n = 1 if hi in (0xC0, 0xD0) else 2
        need(n)
        a = data[pos]
        b = data[pos + 1] if n == 2 else 0
        if a > 127 or b > 127:
            _fail(diag, pos, f"track {index + 1}: data byte above 127")
        pos += n
        if hi == 0x90 and b > 0:
            track.events.append(Event(tick, "on", ch, a, b))
        elif hi in (0x80, 0x90):
            track.events.append(Event(tick, "off", ch, a, 0))
        elif hi == 0xB0:
            track.events.append(Event(tick, "cc", ch, a, b))
        elif hi == 0xE0:
            track.events.append(Event(tick, "bend", ch, a, b))
        elif hi == 0xC0:
            track.events.append(Event(tick, "program", ch, a))
        else:                                           # polyphonic / channel pressure: kept for warnings only
            track.events.append(Event(tick, "pressure", ch, a, b))
    return track


# ------------------------------------------------------------------ writer (used by the song generators)

def vlq_bytes(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.insert(0, (n & 0x7F) | 0x80)
        n >>= 7
    return bytes(out)


def _meta(kind, payload):
    return bytes([0xFF, kind]) + vlq_bytes(len(payload)) + payload


def write_midi(ppq, tracks):
    """tracks: [(name, [(tick, bytes)])] -> SMF format 1 bytes. Events at one tick keep their list order
    after a stable sort by tick, so callers put note-offs before note-ons where it matters."""
    out = b"MThd" + (6).to_bytes(4, "big") + (1).to_bytes(2, "big") + len(tracks).to_bytes(2, "big") \
        + ppq.to_bytes(2, "big")
    for name, events in tracks:
        body = vlq_bytes(0) + _meta(0x03, name.encode("ascii")) if name else b""
        last = 0
        for tick, ev in sorted(events, key=lambda e: e[0]):
            body += vlq_bytes(tick - last) + ev
            last = tick
        body += vlq_bytes(0) + _meta(0x2F, b"")
        out += b"MTrk" + len(body).to_bytes(4, "big") + body
    return out


def meta_tempo(us):
    return _meta(0x51, us.to_bytes(3, "big"))


def meta_timesig(num, den):
    return _meta(0x58, bytes([num, den.bit_length() - 1, 24, 8]))


def meta_key(sharps, minor):
    return _meta(0x59, sharps.to_bytes(1, "big", signed=True) + bytes([minor]))


def meta_text(kind, text):
    return _meta(kind, text.encode("ascii"))
