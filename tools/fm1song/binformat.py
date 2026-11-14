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
from .arrange import STYLE_NAMES
from .arrange import canonical as arr_canonical
from .source import LAYERS

MAGIC = b"FM1S"
VERSION = (1, 3)                                         # what compile_song writes
READS = (0, 1, 2, 3)                                     # the minor versions this reader takes
MAX_VARIANTS = {0: 3, 1: 3, 2: 3, 3: 5}                  # levels a layer may have, by minor version (1.3: M4)
# Format 2.0 (M5, D029): the song has a style (STYL); the FM-1 generates harmony 1-5, bass 1-4, drums 1-3 from the
# chord chart; the file stores the melody and only the levels made by hand or imported (LAYR's last u16: the stored
# levels, bit v-1), each as an "NT" + layer letter + level chunk: notes by bar, every different bar once:
#   u16 patterns, u16 bars (= the song's), u16 pattern per bar (0xFFFF: none), then each pattern: u16 notes, and per
#   note: varint start in the bar, varint length (ticks; LEB128, at most 3 bytes), u8 pitch, u8 velocity (1..127)
VERSION2 = (2, 0)
VERSION21 = (2, 1)                                       # M7: 2.0 + a start level per layer (LAYR's flags byte, r[5])
VERSION22 = (2, 2)                                       # M8: 2.1 + FORM (kind, sections' role and energy), new styles
FORM = struct.Struct("<BBBBHH40s")                       # kind, backtrack number, scale, 0, sections, 0, numerals
FORM_SECT = struct.Struct("<BB")                         # per section: role (source.ROLES), energy (0..2)
KINDS = ("song", "backtrack")
SCALES = ("major", "minor", "blues")
ROLES = ("intro", "verse", "chorus", "bridge", "ending", "other")
GEN_LEVELS = {"harmony": 5, "bass": 4, "drums": 3}
BT_LEVELS = 4                                            # M9 (D052): a backtrack: tiers 1..4 in each generated layer
STYL = struct.Struct("<BBH")                             # style (arrange.STYLE_NAMES index), swing, 0
NT_MAX_NOTES = 65535
NT_MAX_PATTERN = 4096
HEADER = struct.Struct("<4sBBHII HBB I I HBB")          # 32 bytes
DIRENT = struct.Struct("<4sII")                          # fourcc, offset, size
MAX_FILE = 256 * 1024
MAX_CHUNKS = 24
INFO_MAX = 39                                            # M10 (D062): the optional INFO chunk: u8 length, that many ASCII bytes (M10-12: 39, not the plan's 38)
META = struct.Struct("<BBH24s32s16s")                    # 76 bytes
SECT = struct.Struct("<HHHH16s")                         # 24 bytes
CHRD = struct.Struct("<8sBBH")                           # 12 bytes
CEVT = struct.Struct("<IBB5s5s")                         # 16 bytes
BARS = struct.Struct("<HBB")                             # 4 bytes
LAYR = struct.Struct("<BBBBBBH")                         # 8 bytes
EVNT = struct.Struct("<IBBBB")                           # 8 bytes
LIMITS = {"bars": 999, "sections": 32, "chords": 64, "chord_events": 2048, "events": 65535,
          "voicing_notes": 5, "voicing_levels": 2,
          "ez_events": 8192, "ez_events_13": 16384,      # all EZ streams together: 64 KiB decoded on the device
          "levels_events": 24576}                        # 2.0: the largest level of each layer together (192 KiB)
                                                         # (1.3, M4: 16384, 128 KiB)
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


def _varint(n):
    out = b""
    while True:
        b = n & 0x7F
        n >>= 7
        out += bytes([b | (0x80 if n else 0)])
        if not n:
            return out


def notes_of(evs):
    """events [(tick, status, pitch, vel)] -> notes [(start, end, pitch, vel)] (controllers left out)"""
    on, out = {}, []
    for t, st, a, b in evs:
        if st == 0x90 and b > 0:
            on.setdefault(a, []).append((t, b))
        elif st == 0x80 or (st == 0x90 and b == 0):
            if on.get(a):
                t0, v = on[a].pop(0)
                out.append((t0, t, a, v))
    return sorted(out)


def nt_encode(notes, bar_ticks, nbars):
    """a stored level's notes -> an NT chunk body (bars as patterns, each different bar once)"""
    pats, idx, seq = [], {}, []
    by = {}
    for a, e, p, v in sorted(notes):
        by.setdefault(a // bar_ticks, []).append((a % bar_ticks, max(1, e - a), p, v))
    for b in range(nbars):
        key = tuple(by.get(b, ()))
        if not key:
            seq.append(0xFFFF)
            continue
        if key not in idx:
            idx[key] = len(pats)
            pats.append(key)
        seq.append(idx[key])
    body = struct.pack("<HH", len(pats), nbars) + b"".join(struct.pack("<H", x) for x in seq)
    for pat in pats:
        body += struct.pack("<H", len(pat)) + b"".join(_varint(o) + _varint(d) + bytes([p, v]) for o, d, p, v in pat)
    return body


def _rd_varint(body, p, bad, where):
    n, shift = 0, 0
    for k in range(3):
        if p >= len(body):
            bad("cut short", where)
        b = body[p]
        p += 1
        n |= (b & 0x7F) << shift
        shift += 7
        if not b & 0x80:
            return n, p
    bad("a number longer than 3 bytes", where)


def nt_decode(body, bar_ticks, nbars, end_tick, bad, where):
    """an NT chunk body -> notes [(start, end, pitch, vel)], every bound checked (as song.c nt_check)"""
    if len(body) < 4:
        bad("too short", where)
    npat, nb = struct.unpack_from("<HH", body)
    if nb != nbars or npat > nbars or len(body) < 4 + 2 * nb:
        bad(f"{npat} patterns for {nb} bars (the song has {nbars})", where)
    seq = struct.unpack_from(f"<{nb}H", body, 4)
    p, pats, total = 4 + 2 * nb, [], 0
    for k in range(npat):
        if p + 2 > len(body):
            bad("cut short", where)
        n = struct.unpack_from("<H", body, p)[0]
        p += 2
        if n < 1 or n > NT_MAX_PATTERN:
            bad(f"pattern {k}: {n} notes", where)
        pat = []
        for j in range(n):
            o, p = _rd_varint(body, p, bad, where)
            d, p = _rd_varint(body, p, bad, where)
            if p + 2 > len(body):
                bad("cut short", where)
            pi, v = body[p], body[p + 1]
            p += 2
            if o >= bar_ticks or d < 1 or pi > 127 or not 1 <= v <= 127:
                bad(f"pattern {k} note {j} out of range", where)
            pat.append((o, d, pi, v))
        pats.append(pat)
    if p != len(body):
        bad(f"{len(body) - p} bytes after the last pattern", where)
    notes = []
    for b, x in enumerate(seq):
        if x == 0xFFFF:
            continue
        if x >= npat:
            bad(f"bar {b}: pattern {x} of {npat}", where)
        for o, d, pi, v in pats[x]:
            a = b * bar_ticks + o
            if a + d > end_tick:
                bad(f"bar {b}: a note runs past the end of the song", where)
            notes.append((a, a + d, pi, v))
    if len(notes) > NT_MAX_NOTES:
        bad(f"{len(notes)} notes", where)
    return notes


def gen_cells(chord_events, chords, bar_ticks, ppq, nbars):
    """the generator's input from the compiled song (the FM-1 builds the same): per bar the chords sounding in it
    [(beat, beats, chord, voicing 1, voicing 2)], a chord as root, bass and intervals (from CHRD's mask)"""
    class GChord:
        __slots__ = ("root", "bass", "intervals")
    out = []
    for b in range(nbars):
        t0, t1, row = b * bar_ticks, (b + 1) * bar_ticks, []
        for k, e in enumerate(chord_events):
            a = max(e["tick"], t0)
            z = min(chord_events[k + 1]["tick"] if k + 1 < len(chord_events) else 1 << 40, t1)
            if a >= z:
                continue
            ch_ = chords[e["chord"]]
            c = None
            if ch_["root"] is not None:
                c = GChord()
                c.root, c.bass = ch_["root"], ch_["bass"]
                c.intervals = sorted({(pc - c.root) % 12 for pc in range(12) if ch_["pitch_mask"] >> pc & 1})
            row.append(((a - t0) // ppq, (z - a) // ppq, c, [n for n in e["voicing"][0] if n],
                        [n for n in e["voicing"][1] if n]))
        out.append(row)
    return out


def generate(compiled, layer, level):
    """a generated level's events, as the FM-1 makes them (tools/fm1song/arrange.py on the compiled chart)"""
    from . import arrange as arr
    return arr.canonical(generate_notes(compiled, layer, level))


def generate_notes(compiled, layer, level):
    """a generated level's notes before arrange.canonical (their number sizes the FM-1's buffer)"""
    from . import arrange as arr
    c = compiled
    bpb = int(c["time_signature"][0])
    bar_ticks = bpb * c["ppq"]
    cells = gen_cells(c["chord_events"], c["chords"], bar_ticks, c["ppq"], c["bar_count"])
    first = c["sections"][0]["first_bar"]
    starts = {s["first_bar"] for s in c["sections"]} - {first}
    sty = arr.STYLES[c["style"]]
    final = len(cells) - 1
    if c.get("kind") == "backtrack":       # M9 (D052): the tier tables, 4 levels in each layer
        secs = c["sections"]
        alt = [(b - secs[x["section"]]["first_bar"]) % 2 for b, x in enumerate(c["bars"])]
        if layer == "harmony":
            every = arr.band_harmony(cells, bpb, c["style"], final, alt)
        elif layer == "bass":
            every = arr.band_bass(cells, bpb, c["style"], final, alt)
        else:
            per_bar = [(secs[x["section"]]["first_bar"], secs[x["section"]]["bars"], secs[x["section"]]["role"],
                        len(secs), x["section"]) for x in c["bars"]]
            every = arr.band_drums(len(cells), bpb, c["style"], c["pickup_beats"], per_bar, final)
    elif layer == "harmony":
        every = arr.harmony(cells, bpb, sty["harmony"], c["swing"], final)
    elif layer == "bass":
        every = arr.bass(cells, bpb, sty["bass"], c["swing"], final)
    else:
        every = arr.drums(len(cells), bpb, sty["drums"], c["swing"], c["pickup_beats"], starts, final)
    # M8 (2.2): a section's energy adds to the level in its bars (capped at the top level), for a layer with no stored
    # level; a note belongs to the bar it starts in
    energy = [c["sections"][b["section"]].get("energy", 0) for b in c["bars"]]
    if not any(energy) or c.get("stored", {}).get(layer):
        return every[level]
    top = max(every)
    out = []
    for lv in sorted({min(level + e, top) for e in energy}):
        out += [n for n in every[lv] if min(level + energy[min(n[0] // bar_ticks, len(energy) - 1)], top) == lv]
    return out


def restyled(c, style, bpb):
    """M9: what the FM-1's song_restyle (firmware/app/song.c) makes of an opened backtrack in another style (a name of
    arrange.STYLE_NAMES) and metre, as the Python generators see it: the compiled dict with the style, the swing, the
    metre and every chord change moved onto the new bar lines. generate / generate_notes then give the FM-1's levels"""
    import copy
    r = copy.deepcopy(c)
    old = int(c["time_signature"][0]) * c["ppq"]
    new = bpb * c["ppq"]
    r["style"], r["swing"] = style, style == "shuffle"
    r["time_signature"] = [str(bpb), "4"]
    for e in r["chord_events"]:
        e["tick"] = e["tick"] // old * new
    r["end_tick"] = c["bar_count"] * new
    return r


def need_bytes(c):
    """the FM-1's build need of a compiled (or restyled) song: per layer the largest level's events (2 a generated
    note) x 8 bytes, summed (song.c levels2; library.c lib_open sums them)"""
    return 8 * sum(max(2 * len(generate_notes(c, name, lv)) for lv in ly["variants"])
                   for name, ly in c["layers"].items() if name in GEN_LEVELS)


def needs_22(song):
    """M8: a song that uses anything of 2.2 (a backtrack, a section's role or energy, a new style)"""
    return bool(song.style) and (getattr(song, "kind", "song") != "song" or
                                 any(sc.role is not None or sc.energy for sc in song.sections) or
                                 STYLE_NAMES.index(song.style) >= 6)


def compile_song(song, version=None):
    starts = song.style and any(ly.start_level > 1 for ly in song.layers.values())
    if version is None:              # (2.1 only when a layer opens on another level: every other song stays 2.0;
        # M8: 2.2 only for what needs it)
        version = (VERSION22 if needs_22(song) else VERSION21 if starts else VERSION2) if song.style else VERSION
    if version in (VERSION2, VERSION21) and needs_22(song):
        raise ValueError(f"{song.slug}: needs format 2.2 (a backtrack, roles / energy, or a new style)")
    if song.style and version < VERSION2:
        song = _expanded(song)           # (an older device: the generated levels as events)
    nvars = lambda ly: min(len(ly.variants), MAX_VARIANTS[version[1]]) if version[0] == 1 else ly.levels or \
        len(ly.variants)
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
            mask = sum(1 << (v - 1) for v in ly.variants) if version[0] == 2 else 0
            start = ly.start_level if version in (VERSION21, VERSION22) and ly.start_level > 1 else 0
            layr += LAYR.pack(ly.channel, 0xFF if ly.program is None else ly.program, nvars(ly),
                              ly.volume, ly.pan, start, mask)
    chunks.append((b"LAYR", layr))
    if version >= (1, 2):
        chunks.append((b"CATG", _cstr(song.category, 16)))
    if version[0] == 2:
        chunks.append((b"STYL", STYL.pack(STYLE_NAMES.index(song.style), 1 if song.swing else 0, 0)))
        if version == VERSION22:          # M8: what the file is, and its sections' roles and energy
            kind = getattr(song, "kind", "song")
            bt = kind == "backtrack"
            body = FORM.pack(KINDS.index(kind), song.bt_number if bt else 0,
                             SCALES.index(song.bt_scale) if bt else 0, 0, len(song.sections), 0,
                             _cstr(song.bt_numerals if bt else "", 40))
            for sc in song.sections:
                body += FORM_SECT.pack(ROLES.index(sc.role or "other"), sc.energy)
            chunks.append((b"FORM", body))
        for li, name in enumerate(LAYERS):
            ly = song.layers.get(name)
            for var in sorted(ly.variants) if ly else []:
                chunks.append((b"NT" + LAYER_LETTER[li].encode() + str(var).encode(),
                               nt_encode(notes_of(ly.variants[var]), bar_ticks, song.bar_count)))
    info = getattr(song, "info", "")
    if version[0] == 2 and info:          # M10: the song's info line, last (so that without it the file is M9's)
        chunks.append((b"INFO", bytes([len(info)]) + info.encode("ascii")))
    for li, name in enumerate(LAYERS if version[0] == 1 else ()):
        ly = song.layers.get(name)
        for var in sorted(ly.variants)[:nvars(ly)] if ly else []:
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


def _expanded(song):
    """a 2.0 song with its generated levels made into events (for an older device's format)"""
    import copy
    blob = compile_song(song, VERSION2)
    c = read_compiled(blob)
    s2 = copy.copy(song)
    s2.layers = {}
    for name, ly in song.layers.items():
        l2 = copy.copy(ly)
        l2.variants = {v: [tuple(e) for e in evs] for v, evs in c["layers"][name]["variants"].items()}
        l2.levels = 0
        s2.layers[name] = l2
    s2.style = None
    return s2


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
    if not (vmaj == 1 and vmin in READS or (vmaj, vmin) in (VERSION2, VERSION21, VERSION22)):
        bad(f"format version {vmaj}.{vmin}; this reader reads 1.0 to 1.{READS[-1]}, 2.0 .. 2.2", "header")
    fmt2 = vmaj == 2
    fmt22 = (vmaj, vmin) == VERSION22
    fmt21 = (vmaj, vmin) in (VERSION21, VERSION22)       # (2.2 has 2.1's start levels)
    vminor2 = vmin
    if fmt2:
        vmin = 3                         # (2.0 has every 1.3 rule; its own below)
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
    if fmt2 and ppq != 480:
        bad(f"{ppq} ticks a beat: 2.0 has 480 (the generators')", "header")
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
    out = Compiled(version=f"{vmaj}.{vminor2 if fmt2 else vmin}", size=size, crc32=f"{crc:08x}", ppq=ppq,
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
        if fmt2 and chords[idx][1] != NO_CHORD and (not any(v1) or not any(v2)):
            bad(f"chord event {k}: a chord with no notes to play", "CEVT")
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

    style = None
    if fmt2:                               # 2.0: the style (M8: 2.2 has five more)
        st = chunk(b"STYL")
        if len(st) != STYL.size:
            bad(f"{len(st)} bytes", "STYL")
        si, sw, z = STYL.unpack(st)
        if si >= (len(STYLE_NAMES) if fmt22 else 6) or sw > 1 or z or bool(sw) != bool(flags & 1):
            bad("style out of range", "STYL")
        style = STYLE_NAMES[si]
    out["kind"], out["number"], out["scale"], out["numerals"] = "song", 0, "major", ""
    for sc in out["sections"]:
        sc["role"], sc["energy"] = "other", 0
    if fmt22:                              # M8: FORM: the kind, a backtrack's number, scale and numerals; per section
        fm = chunk(b"FORM")                # its role and energy
        if len(fm) < FORM.size:
            bad(f"{len(fm)} bytes", "FORM")
        kind, num, scl, z1, ns, z2, numerals = FORM.unpack_from(fm)
        if kind >= len(KINDS) or scl >= len(SCALES) or z1 or z2 or ns != len(out["sections"]) or \
                len(fm) != FORM.size + ns * FORM_SECT.size or (kind == 0) != (num == 0) or (kind == 0 and scl):
            bad("kind, number, scale or sections out of range", "FORM")
        out["kind"], out["number"], out["scale"] = KINDS[kind], num, SCALES[scl]
        out["numerals"] = _str(diag, numerals, "FORM numerals")
        if kind == 0 and out["numerals"]:
            bad("a song has no numerals", "FORM")
        for k, sc in enumerate(out["sections"]):
            role, en = FORM_SECT.unpack_from(fm, FORM.size + k * FORM_SECT.size)
            if role >= len(ROLES) or en > 2:
                bad(f"section {k}: role {role}, energy {en} out of range", "FORM")
            sc["role"], sc["energy"] = ROLES[role], en
    out["info"] = ""
    if b"INFO" in chunks:                  # M10: the optional song info line (2.x): u8 length, the text
        ib = chunks[b"INFO"]
        if not fmt2 or len(ib) < 1 or ib[0] > INFO_MAX or len(ib) != 1 + ib[0] or any(not 32 <= b <= 126 for b in ib[1:]):
            bad("a length and up to 39 printable ASCII bytes, in a 2.x file", "INFO")
        out["info"] = ib[1:].decode("ascii")
    out["style"], out["stored"] = style, {}
    layr = chunk(b"LAYR")
    if len(layr) != 4 + len(LAYERS) * LAYR.size or layr[0] != len(LAYERS):
        bad("needs exactly 4 layers", "LAYR")
    out["layers"] = {}
    ez_total, raw_notes = 0, {}
    for li, name in enumerate(LAYERS):
        chan, prog, nvar, vol, pan, lfl, mask = LAYR.unpack_from(layr, 4 + li * LAYR.size)
        if nvar == 0:
            if chan != 0xFF or mask:
                bad(f"{name}: no variants but a channel", "LAYR")
            continue
        if chan > 15 or nvar > MAX_VARIANTS[vmin] or vol > 127 or pan > 127 or prog > 127 and prog != 0xFF or \
                (lfl and not (fmt21 and 2 <= lfl <= nvar)):
            bad(f"{name} out of range (2.1: the start level 2..levels, else 0)", "LAYR")
        variants = {}
        if fmt2:                           # 2.0: each level stored (NT) or generated
            full = (1 << nvar) - 1
            gen_n = BT_LEVELS if out["kind"] == "backtrack" and name in GEN_LEVELS else GEN_LEVELS.get(name)
            if mask & ~full or (name not in GEN_LEVELS and mask != full) or \
                    (name in GEN_LEVELS and mask != full and nvar != gen_n):
                bad(f"{name}: levels {nvar}, stored {mask:#x}: a melody is stored; a generated layer has "
                    f"{gen_n} levels", "LAYR")
            for v in range(1, nvar + 1):
                tag = b"NT" + LAYER_LETTER[li].encode() + str(v).encode()
                if mask >> (v - 1) & 1:
                    notes = nt_decode(chunk(tag), bar_ticks, bar_count, end_tick, bad, tag.decode())
                    raw_notes[(name, v)] = len(notes)
                    variants[v] = arr_canonical(notes)
                elif tag in chunks:
                    bad("a generated level stored too", tag.decode())
            out["stored"][name] = [v for v in range(1, nvar + 1) if mask >> (v - 1) & 1]
            out["layers"][name] = {"channel": chan, "program": None if prog == 0xFF else prog,
                                   "volume": vol, "pan": pan, "variants": variants, "levels": nvar,
                                   "start_level": lfl or 1}
            continue
        for v in range(1, nvar + 1):
            tag = LAYER_LETTER[li].encode() + str(v).encode()
            fourcc, ez = b"EV" + tag, b"EZ" + tag in chunks
            if ez and (fourcc in chunks or vmin < 1):
                bad(f"{(b'EZ' + tag).decode()}: needs version 1.1, and no {fourcc.decode()} beside it", "directory")
            if ez:
                fourcc = b"EZ" + tag
                raw = ez_decode(chunk(fourcc), bad, fourcc.decode())
                ez_total += len(raw)
                cap = LIMITS["ez_events_13" if vmin >= 3 else "ez_events"]
                if ez_total > cap:
                    bad(f"more than {cap} events in all EZ streams together", fourcc.decode())
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
    if fmt2:                               # the generated levels, as the FM-1 makes them
        for name, ly in out["layers"].items():
            for v in range(1, ly["levels"] + 1):
                if v not in ly["variants"]:
                    notes = generate_notes(out, name, v)
                    raw_notes[(name, v)] = len(notes)
                    ly["variants"][v] = arr_canonical(notes)
            ly["variants"] = dict(sorted(ly["variants"].items()))
            del ly["levels"]
        need = sum(max(2 * raw_notes[(name, v)] for v in ly["variants"]) for name, ly in out["layers"].items())
        if need > LIMITS["levels_events"]:
            bad(f"{need} events with the largest level of each layer: at most {LIMITS['levels_events']}", "LAYR")
    known = {b"META", b"SECT", b"CHRD", b"CEVT", b"BARS", b"LAYR"} | ({b"CATG"} if vmin >= 2 else set()) | \
        ({b"STYL"} if fmt2 else set()) | ({b"FORM"} if fmt22 else set()) | ({b"INFO"} if fmt2 else set())
    out["unknown_chunks"] = sorted(c.decode("latin-1") for c in chunks
                                   if c not in known and not c.startswith(b"EV") and not c.startswith(b"EZ")
                                   and not (fmt2 and c.startswith(b"NT")))
    return out
