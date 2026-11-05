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


def test_header_layout(blobs):
    b = blobs[0]
    assert b[:4] == b"FM1S" and b[4:6] == bytes([1, 0])
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
