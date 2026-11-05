# The compiled format: round trip, every corruption refused with SongError, and fuzzing of all three inputs
# (compiled bytes, song.mid bytes, song.yaml text): the only exception allowed is SongError.
import random
import struct
import zlib

import pytest
import yaml

from conftest import SONGS
from fm1song import SongError, compile_folder, read_compiled, song_from_data
from fm1song.binformat import HEADER


def fix_crc(blob):
    blob = bytearray(blob)
    blob[12:16] = bytes(4)
    blob[12:16] = struct.pack("<I", zlib.crc32(bytes(blob)))
    return bytes(blob)


@pytest.fixture(scope="module")
def blobs():
    return [compile_folder(f)[0] for f in SONGS]


@pytest.fixture(scope="module")
def blobs10():
    return [compile_folder(f, (1, 0))[0] for f in SONGS]


def test_header_layout(blobs):
    b = blobs[0]
    assert b[:4] == b"FM1S" and b[4:6] == bytes([1, 1])
    assert struct.unpack_from("<I", b, 8)[0] == len(b)
    assert HEADER.size == 32 and len(b) % 4 == 0


def test_dump_round_trip(blobs):
    for b in blobs:
        c = read_compiled(b)
        assert c["size"] == len(b)
        bpb = int(c["time_signature"][0])
        # every bar points at the chord sounding at its first beat
        for i, bar in enumerate(c["bars"]):
            ev = c["chord_events"][bar["chord_event"]]
            assert ev["tick"] <= i * bpb * c["ppq"]


@pytest.mark.parametrize("cut", [0, 4, 31, 32, 100, -4, -1])
def test_truncated(blobs, cut):
    with pytest.raises(SongError):
        read_compiled(blobs[0][:cut])


def test_checksum(blobs):
    b = bytearray(blobs[0])
    b[200] ^= 1
    with pytest.raises(SongError, match="checksum"):
        read_compiled(bytes(b))


@pytest.mark.parametrize("offset,value,expect", [
    (0, b"FM1X", "magic"), (4, b"\x02", "version"), (8, b"\x00\x00\x01\x00", "header says"),
    (16, b"\x10\x00", "metre"), (18, b"\x09", "metre"), (20, b"\x01\x00\x00\x00", "tempo"),
    (28, b"\x00\x00", "bars"), (30, b"\x05", "pickup"), (6, b"\xff\x00", "chunks"),
])
def test_header_fields(blobs, offset, value, expect):
    b = bytearray(blobs[0])
    b[offset:offset + len(value)] = value
    with pytest.raises(SongError, match=expect):
        read_compiled(fix_crc(b))


def test_v10_and_v11_same_music(blobs, blobs10):
    for a, b in zip(blobs10, blobs):
        ca, cb = read_compiled(a), read_compiled(b)
        assert ca["version"] == "1.0" and cb["version"] == "1.1" and len(b) < len(a) * 0.6
        for k in ("layers", "chord_events", "bars", "sections", "chords"):
            assert ca[k] == cb[k]


def rebuild(blob, edit):
    """the file's chunks (fourcc -> body) changed by edit(dict) and written again, directory and CRC new"""
    from fm1song.binformat import DIRENT
    n = struct.unpack_from("<H", blob, 6)[0]
    chunks = {}
    for i in range(n):
        cc, off, ln = DIRENT.unpack_from(blob, 32 + 12 * i)
        chunks[cc] = blob[off:off + ln]
    edit(chunks)
    off = 32 + 12 * len(chunks)
    head, body = b"", b""
    for cc, b in chunks.items():
        head += DIRENT.pack(cc, off + len(body), len(b))
        body += b + bytes(-len(b) % 4)
    out = bytearray(blob[:32] + head + body)
    struct.pack_into("<HI", out, 6, len(chunks), len(out))
    return fix_crc(bytes(out))


def test_ez_rules(blobs, blobs10):
    from fm1song.binformat import ez_encode
    b = blobs[0]
    read_compiled(rebuild(b, lambda c: None))                   # the rebuild itself is faithful
    bad = {
        "EZ in a 1.0 file": lambda c: None,
        "EZ beside its EV": lambda c: c.__setitem__(b"EVD1", struct.pack("<I", 0)),
        "bytes after the last event": lambda c: c.__setitem__(b"EZD1", c[b"EZD1"] + b"\0"),
        "cut short": lambda c: c.__setitem__(b"EZD1", c[b"EZD1"][:-1]),
        "delta longer than 3 bytes": lambda c: c.__setitem__(b"EZD1", struct.pack("<I", 1) + b"\x80\xff\xff\xff\x01\x24"),
        "count larger than the events": lambda c: c.__setitem__(b"EZD1", struct.pack("<I", 99) + c[b"EZD1"][4:]),
        "note-on with velocity left out": lambda c: c.__setitem__(b"EZD1", struct.pack("<I", 1) + bytes([0x50, 36])),
        "past the end tick": lambda c: c.__setitem__(b"EZD1", ez_encode([[10 ** 7, 0x80, 36, 0]])),
        "over 8192 events in all": lambda c: c.__setitem__(b"EZD1", ez_encode([[0, 0xB0, 1, 0]] * 8193)),
    }
    for what, edit in bad.items():
        f = rebuild(b, edit)
        if what == "EZ in a 1.0 file":
            f = fix_crc(f[:5] + b"\0" + f[6:])
        with pytest.raises(SongError):
            read_compiled(f)
            print("accepted:", what)


def test_unknown_chunk_is_skipped(blobs):
    c = read_compiled(blobs[0])
    assert c["unknown_chunks"] == []


def mutate(rng, data):
    data = bytearray(data)
    for _ in range(rng.randint(1, 8)):
        op = rng.random()
        if not data:
            break
        i = rng.randrange(len(data))
        if op < 0.5:
            data[i] = rng.randrange(256)
        elif op < 0.65:
            data[i] = rng.choice([0, 1, 0x7F, 0x80, 0xFF])
        elif op < 0.8:
            del data[i:i + rng.randint(1, 16)]
        elif op < 0.9:
            data[i:i] = bytes(rng.randrange(256) for _ in range(rng.randint(1, 8)))
        else:
            j = rng.randrange(len(data))
            data[i:i + 4] = data[j:j + 4]
    return bytes(data)


def test_fuzz_compiled(blobs):
    rng = random.Random(1)
    accepted = 0
    for k in range(3000):
        b = mutate(rng, rng.choice(blobs))
        if k % 2:
            b = fix_crc(b) if len(b) >= 16 else b     # get past the checksum, into the structure checks
        try:
            read_compiled(b)
            accepted += 1
        except SongError:
            pass
    assert accepted < 3000


def test_fuzz_midi():
    rng = random.Random(2)
    data = yaml.safe_load((SONGS[0] / "song.yaml").read_text())
    mid = (SONGS[0] / "song.mid").read_bytes()
    for _ in range(400):
        try:
            song_from_data(data, mutate(rng, mid))
        except SongError:
            pass


def test_fuzz_yaml():
    rng = random.Random(3)
    text = (SONGS[0] / "song.yaml").read_text()
    mid = (SONGS[0] / "song.mid").read_bytes()
    for _ in range(400):
        t = mutate(rng, text.encode()).decode("latin-1")
        try:
            data = yaml.safe_load(t)
        except yaml.YAMLError:
            continue
        try:
            song_from_data(data, mid)
        except SongError:
            pass


def test_device_parser_agrees(blobs, blobs10, tmp_path):
    """the C parser (firmware/app/song.c, via build/fuzz/song_fuzz --diff) accepts exactly what this reader accepts"""
    import subprocess
    from conftest import ROOT
    exe = ROOT / "build/fuzz/song_fuzz"
    if not exe.exists():
        pytest.skip("build tools/song_fuzz first (tools/song_fuzz.sh)")
    rng = random.Random(4)
    files, expect = [], {}
    seeds = blobs + blobs10
    for k in range(3000):
        b = mutate(rng, rng.choice(seeds))
        if k % 4:
            b = fix_crc(b) if len(b) >= 16 else b
        if k % 8 == 0 and len(b) >= 12:         # the size field follows the length
            b = fix_crc(b[:8] + struct.pack("<I", len(b)) + b[12:])
        p = tmp_path / f"m{k}.fm1s"
        p.write_bytes(b)
        files.append(str(p))
        try:
            read_compiled(b)
            expect[str(p)] = True
        except SongError:
            expect[str(p)] = False
    for b in seeds:
        p = tmp_path / f"seed{len(files)}.fm1s"
        p.write_bytes(b)
        files.append(str(p))
        expect[str(p)] = True
    out = subprocess.run([str(exe), "--diff"] + files, capture_output=True, text=True, check=True).stdout
    got = {line.split(" ", 1)[1]: line.split(" ", 1)[0] == "0" for line in out.splitlines()}
    differ = [f for f in files if got[f] != expect[f]]
    assert not differ, [(f, expect[f]) for f in differ[:5]]
    assert sum(expect.values()) > len(seeds)       # some mutants are valid, so the comparison means something
