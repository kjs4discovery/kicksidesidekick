# fm1song: the compiled song file (.fm1s), versions 1.0, 1.1 and 1.2: writer and checking reader. Licence: TBD (see D017)
#
# 1.1 (M2) stores each event stream delta-coded (chunk "EZ" + layer + variant) instead of 8-byte records ("EV..");
# the device decodes them into RAM when it opens the song. 1.2 (M3) adds the CATG chunk: the song's category (16
# bytes, NUL-terminated printable ASCII, may be empty). The writer writes 1.2 (or 1.1 / 1.0 when asked: for an older
# device); the reader reads all three.
# Layout and limits: docs/song-format.md, "Compiled format". All numbers little-endian, every chunk 4-byte
# aligned. The reader checks everything the device parser (firmware/app/song.c) checks, so a file this
# reader accepts, the device accepts; tests/test_binformat.py keeps the two in step.
import struct
import zlib

from .errors import Diagnostics
from .source import LAYERS

MAGIC = b"FM1S"
VERSION = (1, 2)                                         # what compile_song writes
READS = (0, 1, 2)                                        # the minor versions this reader takes
HEADER = struct.Struct("<4sBBHII HBB I I HBB")          # 32 bytes
DIRENT = struct.Struct("<4sII")                          # fourcc, offset, size
MAX_FILE = 256 * 1024
MAX_CHUNKS = 24
META = struct.Struct("<BBH24s32s16s")                    # 76 bytes
SECT = struct.Struct("<HHHH16s")                         # 24 bytes
CHRD = struct.Struct("<8sBBH")                           # 12 bytes
CEVT = struct.Struct("<IBB5s5s")                         # 16 bytes
BARS = struct.Struct("<HBB")                             # 4 bytes
LAYR = struct.Struct("<BBBBBBH")                         # 8 bytes
EVNT = struct.Struct("<IBBBB")                           # 8 bytes
LIMITS = {"bars": 999, "sections": 32, "chords": 64, "chord_events": 2048, "events": 65535,
          "voicing_notes": 5, "voicing_levels": 2,
          "ez_events": 8192}                             # all EZ streams together: 64 KiB decoded on the device
LAYER_LETTER = "DBHM"                                    # event chunk "EV" + letter + variant digit
STATUSES = (0x80, 0x90, 0xB0, 0xE0)
NO_CHORD = 0xFF


def _cstr(text, size):
    raw = text.encode("ascii")
    assert len(raw) < size, text
    return raw + bytes(size - len(raw))


def _count_chunk(count, rec, items):
    return struct.pack("<HH", count, 0) + b"".join(rec.pack(*i) for i in items)


def _pad(n):
    return (-n) % 4


def ez_encode(evs):
    """an event stream, delta-coded (EZ chunk body): u32 count, then per event a first byte (bit 7: more delta
    bytes follow, bit 6: data2 is 0 and left out, bits 5-4: the status 0x80 0x90 0xB0 0xE0, bits 3-0: the tick
    delta's low 4 bits), up to 3 more delta bytes (7 bits each, low first, bit 7: another follows), data1, data2"""
    out, last = bytearray(struct.pack("<I", len(evs))), 0
    for t, st, a, b in evs:
        dt, last = t - last, t
        rest = dt >> 4
        out.append((0x80 if rest else 0) | (0x40 if b == 0 else 0) | STATUSES.index(st) << 4 | (dt & 15))
        while rest:
            out.append((rest & 0x7F) | (0x80 if rest >> 7 else 0))
            rest >>= 7
        out.append(a)
        if b:
            out.append(b)
    return bytes(out)


def ez_decode(body, bad, where):
    """an EZ chunk body -> [[tick, status, data1, data2]] (not yet range-checked: read_compiled does that)"""
    if len(body) < 4:
        bad("too short", where)
    n, p, last, evs = struct.unpack_from("<I", body)[0], 4, 0, []
    if n > LIMITS["events"]:
        bad(f"{n} events", where)
    for k in range(n):
        if p >= len(body):
            bad(f"event {k}: the chunk ends early", where)
        first = body[p]
        p += 1
        dt, shift, more, extra = first & 15, 4, first & 0x80, 0
        while more:
            if p >= len(body) or extra == 3:
                bad(f"event {k}: its tick delta is cut short or longer than 3 bytes", where)
            dt |= (body[p] & 0x7F) << shift
            more = body[p] & 0x80
            p, shift, extra = p + 1, shift + 7, extra + 1
        need = 1 if first & 0x40 else 2
        if p + need > len(body):
            bad(f"event {k}: the chunk ends early", where)
        a, b = body[p], 0 if first & 0x40 else body[p + 1]
        p += need
        last += dt
        evs.append([last, STATUSES[first >> 4 & 3], a, b])
    if p != len(body):
        bad(f"{len(body) - p} bytes after the last event", where)
    return evs


def compile_song(song, version=VERSION):
    """Song (from source.load_song) -> bytes. Deterministic: same song, same bytes. version (1, 0): the M1 layout
    (8-byte event records), for tests and old devices."""
    chunks = []
    chunks.append((b"META", META.pack(song.key_root, song.key_mode, 0, _cstr(song.slug, 24),
                                      _cstr(song.title, 32), _cstr(song.key_name, 16))))
    chunks.append((b"SECT", _count_chunk(len(song.sections), SECT,
                                         [(s.first_bar, s.bar_count, s.lead_ticks, 0, _cstr(s.name, 16))
                                          for s in song.sections])))
    rows = []
    for sym, c in song.chord_table:
        if c is None:
            rows.append((_cstr(sym, 8), NO_CHORD, NO_CHORD, 0))
        else:
            rows.append((_cstr(sym, 8), c.root, NO_CHORD if c.bass is None else c.bass, c.mask))
    chunks.append((b"CHRD", _count_chunk(len(rows), CHRD, rows)))
    voice = lambda v: bytes(v) + bytes(LIMITS["voicing_notes"] - len(v))
    cev = struct.pack("<HBB", len(song.chord_events), LIMITS["voicing_levels"], LIMITS["voicing_notes"])
    bar_ticks = song.ppq * song.beats_per_bar
    for tick, idx, v1, v2 in song.chord_events:
        cev += CEVT.pack(tick, idx, 1 if tick % bar_ticks == 0 else 0, voice(v1), voice(v2))
    chunks.append((b"CEVT", cev))
    chunks.append((b"BARS", _count_chunk(len(song.bars), BARS, song.bars)))
    layr = struct.pack("<BBH", len(LAYERS), 0, 0)
    for name in LAYERS:
        ly = song.layers.get(name)
        if ly is None:
            layr += LAYR.pack(0xFF, 0xFF, 0, 0, 0, 0, 0)
        else:
            layr += LAYR.pack(ly.channel, 0xFF if ly.program is None else ly.program, len(ly.variants),
                              ly.volume, ly.pan, 0, 0)
    chunks.append((b"LAYR", layr))
    if version >= (1, 2):
        chunks.append((b"CATG", _cstr(song.category, 16)))
    for li, name in enumerate(LAYERS):
        ly = song.layers.get(name)
        for var in sorted(ly.variants) if ly else []:
            evs = ly.variants[var]
            tag = LAYER_LETTER[li].encode() + str(var).encode()
            if version >= (1, 1):
                chunks.append((b"EZ" + tag, ez_encode(evs)))
            else:
                body = struct.pack("<I", len(evs)) + b"".join(EVNT.pack(t, s, a, b, 0) for t, s, a, b in evs)
                chunks.append((b"EV" + tag, body))

    offset = HEADER.size + DIRENT.size * len(chunks)
    directory, payload = b"", b""
    for fourcc, body in chunks:
        directory += DIRENT.pack(fourcc, offset + len(payload), len(body))
        payload += body + bytes(_pad(len(body)))
    total = offset + len(payload)
    flags = 1 if song.swing else 0
    head = HEADER.pack(MAGIC, *version, len(chunks), total, 0, song.ppq, song.beats_per_bar, song.beat_unit,
                       song.tempo_us, song.end_tick, song.bar_count, song.pickup_beats, flags)
    blob = head + directory + payload
    crc = zlib.crc32(blob)
    return blob[:12] + struct.pack("<I", crc) + blob[16:]


# ------------------------------------------------------------------ reader

class Compiled(dict):
    """the decoded file as plain data (dict / list / int / str), ready for JSON"""


def _str(diag, raw, where):
    if 0 not in raw:
        diag.fatal("text field has no terminating NUL", "song.fm1s", where)
    text = raw[:raw.index(0)]
    if any(not 32 <= b <= 126 for b in text):
        diag.fatal("text field has bytes outside printable ASCII", "song.fm1s", where)
    return text.decode("ascii")


def read_compiled(blob, diag=None):
    """bytes -> Compiled; raises SongError with the first problem found (like the device parser)"""
    diag = diag or Diagnostics()
    blob = bytes(blob)
    bad = lambda msg, where=None: diag.fatal(msg, "song.fm1s", where)
    if len(blob) < HEADER.size:
        bad(f"{len(blob)} bytes: shorter than the {HEADER.size}-byte header")
    (magic, vmaj, vmin, nchunks, size, crc, ppq, bpb, unit, tempo_us, end_tick, bar_count, pickup,
     flags) = HEADER.unpack_from(blob)
    if magic != MAGIC:
        bad(f"magic {magic!r}, expected {MAGIC!r}", "header")
    if vmaj != VERSION[0] or vmin not in READS:
        bad(f"format version {vmaj}.{vmin}; this reader reads {VERSION[0]}.0 to {VERSION[0]}.{READS[-1]}", "header")
    if size != len(blob) or size > MAX_FILE:
        bad(f"header says {size} bytes, the file has {len(blob)} (max {MAX_FILE})", "header")
    if crc != zlib.crc32(blob[:12] + bytes(4) + blob[16:]):
        bad("checksum does not match (file damaged)", "header")
    if not 24 <= ppq <= 960 or not 2 <= bpb <= 7 or unit != 4:
        bad(f"metre {bpb}/{unit} at {ppq} ticks per beat is outside v1's range", "header")
    if not 240_000 <= tempo_us <= 2_000_000:
        bad(f"tempo {tempo_us} us per beat is outside 30..250 bpm", "header")
    if not 1 <= bar_count <= LIMITS["bars"] or end_tick != bar_count * bpb * ppq:
        bad(f"{bar_count} bars and end tick {end_tick} do not agree", "header")
    if pickup >= bpb or flags & ~1:
        bad(f"pickup {pickup} beats / flags 0x{flags:02x} out of range", "header")
    if not 1 <= nchunks <= MAX_CHUNKS or HEADER.size + nchunks * DIRENT.size > size:
        bad(f"{nchunks} chunks", "directory")
    chunks, prev_end = {}, HEADER.size + nchunks * DIRENT.size
    for i in range(nchunks):
        fourcc, off, length = DIRENT.unpack_from(blob, HEADER.size + i * DIRENT.size)
        name = fourcc.decode("latin-1")
        if off % 4 or off < prev_end or length > size or off + length > size:
            bad(f"chunk {name!r} at {off}+{length} is misplaced (must be aligned, in order, inside the file)",
                "directory")
        if fourcc in chunks:
            bad(f"chunk {name!r} appears twice", "directory")
        chunks[fourcc] = blob[off:off + length]
        prev_end = off + length

    def chunk(fourcc, rec=None, head=4, count_fmt="<H", limit=None):
        body = chunks.get(fourcc)
        if body is None:
            bad(f"required chunk {fourcc.decode()!r} is missing", "directory")
        if rec is None:
            return body
        if len(body) < head:
            bad("too short", fourcc.decode())
        count = struct.unpack_from(count_fmt, body)[0]
        if limit is not None and not 1 <= count <= limit:
            bad(f"{count} entries: must be 1..{limit}", fourcc.decode())
        if len(body) != head + count * rec.size:
            bad(f"{len(body)} bytes for {count} entries of {rec.size}", fourcc.decode())
        return [rec.unpack_from(body, head + k * rec.size) for k in range(count)], body

    bar_ticks = bpb * ppq
    out = Compiled(version=f"{vmaj}.{vmin}", size=size, crc32=f"{crc:08x}", ppq=ppq,
                   time_signature=f"{bpb}/{unit}", tempo_us=tempo_us, tempo_bpm=round(60e6 / tempo_us, 2),
                   end_tick=end_tick, bar_count=bar_count, pickup_beats=pickup, swing=bool(flags & 1))
    meta = chunk(b"META")
    if len(meta) != META.size:
        bad(f"{len(meta)} bytes, expected {META.size}", "META")
    root, mode, _, slug, title, key = META.unpack(meta)
    if root > 11 or mode > 6:
        bad("key out of range", "META")
    out.update(slug=_str(diag, slug, "META slug"), title=_str(diag, title, "META title"),
               key=_str(diag, key, "META key"), key_root=root, key_mode=mode, category="")
    if vmin >= 2:                         # 1.2: the category (required, may be empty)
        catg = chunk(b"CATG")
        if len(catg) != 16:
            bad(f"{len(catg)} bytes, expected 16", "CATG")
        out["category"] = _str(diag, catg, "CATG category")

    sects, _ = chunk(b"SECT", SECT, limit=LIMITS["sections"])
    out["sections"], expect = [], None
    for k, (first, count, lead, _, name) in enumerate(sects):
        if expect is not None and first != expect or count == 0 or first + count > bar_count or lead >= bar_ticks:
            bad(f"section {k} (bars {first}+{count}, lead {lead}) out of range or not in order", "SECT")
        expect = first + count
        out["sections"].append({"name": _str(diag, name, f"SECT {k}"), "first_bar": first, "bars": count,
                                "lead_ticks": lead})
    if expect != bar_count or sects[0][0] > 1 or sects[0][0] != (1 if pickup else 0):
        bad("sections do not cover the song", "SECT")

    chords, _ = chunk(b"CHRD", CHRD, limit=LIMITS["chords"])
    out["chords"] = []
    for k, (sym, root, bass, mask) in enumerate(chords):
        if root == NO_CHORD and (bass != NO_CHORD or mask) or root != NO_CHORD and (root > 11 or not mask) \
                or bass != NO_CHORD and bass > 11 or mask >> 12:
            bad(f"chord {k} out of range", "CHRD")
        out["chords"].append({"symbol": _str(diag, sym, f"CHRD {k}"), "root": None if root == NO_CHORD else root,
                              "bass": None if bass == NO_CHORD else bass, "pitch_mask": mask})

    body = chunk(b"CEVT")
    if len(body) < 4:
        bad("too short", "CEVT")
    count, levels, per = struct.unpack_from("<HBB", body)
    if not 1 <= count <= LIMITS["chord_events"] or levels != LIMITS["voicing_levels"] \
            or per != LIMITS["voicing_notes"] or len(body) != 4 + count * CEVT.size:
        bad(f"{count} events, {levels} levels x {per} notes, {len(body)} bytes", "CEVT")
    out["chord_events"], last = [], -1
    for k in range(count):
        tick, idx, flg, v1, v2 = CEVT.unpack_from(body, 4 + k * CEVT.size)
        if tick <= last or tick >= end_tick or (k == 0) != (tick == 0) or idx >= len(chords) or flg & ~1 \
                or (flg & 1) != (tick % bar_ticks == 0) or any(n > 127 for n in v1 + v2):
            bad(f"chord event {k} at tick {tick} out of range or not in order", "CEVT")
        last = tick
        out["chord_events"].append({"tick": tick, "chord": idx, "voicing": [[n for n in v1 if n], [n for n in v2 if n]]})

    bars, _ = chunk(b"BARS", BARS, limit=LIMITS["bars"])
    if len(bars) != bar_count:
        bad(f"{len(bars)} bars, header says {bar_count}", "BARS")
    out["bars"] = []
    cev = out["chord_events"]
    for b, (idx, sec, flg) in enumerate(bars):
        t0 = b * bar_ticks
        if idx >= len(cev) or cev[idx]["tick"] > t0 or idx + 1 < len(cev) and cev[idx + 1]["tick"] <= t0 \
                or sec >= len(sects) or flg & ~3 or bool(flg & 1) != (cev[idx]["tick"] == t0):
            bad(f"bar {b} does not match the chord events / sections", "BARS")
        out["bars"].append({"chord_event": idx, "section": sec, "flags": flg})

    layr = chunk(b"LAYR")
    if len(layr) != 4 + len(LAYERS) * LAYR.size or layr[0] != len(LAYERS):
        bad("needs exactly 4 layers", "LAYR")
    out["layers"] = {}
    ez_total = 0
    for li, name in enumerate(LAYERS):
        chan, prog, nvar, vol, pan, lfl, _ = LAYR.unpack_from(layr, 4 + li * LAYR.size)
        if nvar == 0:
            if chan != 0xFF:
                bad(f"{name}: no variants but a channel", "LAYR")
            continue
        if chan > 15 or nvar > 3 or vol > 127 or pan > 127 or lfl or prog > 127 and prog != 0xFF:
            bad(f"{name} out of range", "LAYR")
        variants = {}
        for v in range(1, nvar + 1):
            tag = LAYER_LETTER[li].encode() + str(v).encode()
            fourcc, ez = b"EV" + tag, b"EZ" + tag in chunks
            if ez and (fourcc in chunks or vmin < 1):
                bad(f"{(b'EZ' + tag).decode()}: needs version 1.1, and no {fourcc.decode()} beside it", "directory")
            if ez:
                fourcc = b"EZ" + tag
                raw = ez_decode(chunk(fourcc), bad, fourcc.decode())
                ez_total += len(raw)
                if ez_total > LIMITS["ez_events"]:
                    bad(f"more than {LIMITS['ez_events']} events in all EZ streams together", fourcc.decode())
                raw = [(t, st, a, b2, 0) for t, st, a, b2 in raw]
            else:
                evb = chunk(fourcc)
                if len(evb) < 4:
                    bad("too short", fourcc.decode())
                n = struct.unpack_from("<I", evb)[0]
                if n > LIMITS["events"] or len(evb) != 4 + n * EVNT.size:
                    bad(f"{n} events in {len(evb)} bytes", fourcc.decode())
                raw = [EVNT.unpack_from(evb, 4 + k * EVNT.size) for k in range(n)]
            evs, last = [], 0
            for k, (tick, st, a, b2, r) in enumerate(raw):
                if tick < last or tick > end_tick or st not in STATUSES or a > 127 or b2 > 127 or r \
                        or st == 0x90 and (b2 == 0 or tick >= end_tick):
                    bad(f"event {k} (tick {tick}, status 0x{st:02x}) out of range or not in order", fourcc.decode())
                last = tick
                evs.append([tick, st, a, b2])
            variants[v] = evs
        out["layers"][name] = {"channel": chan, "program": None if prog == 0xFF else prog,
                               "volume": vol, "pan": pan, "variants": variants}
    if not out["layers"]:
        bad("no layers", "LAYR")
    known = {b"META", b"SECT", b"CHRD", b"CEVT", b"BARS", b"LAYR"} | ({b"CATG"} if vmin >= 2 else set())
    out["unknown_chunks"] = sorted(c.decode("latin-1") for c in chunks
                                   if c not in known and not c.startswith(b"EV") and not c.startswith(b"EZ"))
    return out
