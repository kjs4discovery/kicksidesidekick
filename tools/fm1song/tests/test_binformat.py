# SPDX-License-Identifier: GPL-3.0-or-later
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


@pytest.fixture(scope="module")
def blobs13():
    return [compile_folder(f, (1, 3))[0] for f in SONGS]


@pytest.fixture(scope="module")
def blobs11():
    return [compile_folder(f, (1, 1))[0] for f in SONGS]


def _with_starts(folder, starts):
    """M7: a song with start levels (format 2.1), compiled"""
    from fm1song.binformat import compile_song
    from fm1song.source import load_song
    song, _ = load_song(folder)
    for name, lv in starts.items():
        song.layers[name].start_level = lv
    return compile_song(song)


@pytest.fixture(scope="module")
def blobs21():
    return [_with_starts(f, {"bass": 2, "harmony": 3 + k % 3, "drums": 1 + k % 3}) for k, f in enumerate(SONGS)]


def test_v21_start_levels(blobs, blobs21):
    """M7: 2.1 only when a layer opens on another level: the start level in LAYR's flags byte; 2.0 otherwise"""
    for b, b21 in zip(blobs, blobs21):
        assert b[4:6] == bytes([2, 0]) and b21[4:6] == bytes([2, 1])
        c = read_compiled(b21)
        assert c["version"] == "2.1" and c["layers"]["bass"]["start_level"] == 2
        assert read_compiled(b)["layers"]["bass"]["start_level"] == 1
        layr = b21.index(b"LAYR")                 # (the directory entry; the chunk's offset follows)
        off = struct.unpack_from("<I", b21, layr + 4)[0]
        assert b21[off + 4 + 8 + 5] == 2          # bass (layer 1): r[5]
    folder = SONGS[0]
    assert _with_starts(folder, {"bass": 1}) == compile_folder(folder)[0]   # start 1 = the default: 2.0, the same bytes


def test_v21_start_level_refused(blobs21):
    """a start level out of 2..levels, or in a 2.0 file, is refused"""
    b = blobs21[0]
    layr = b.index(b"LAYR")
    off = struct.unpack_from("<I", b, layr + 4)[0]
    for lv in (1, 5, 255):                        # bass has 4 levels; 1 is stored as 0
        x = bytearray(b)
        x[off + 4 + 8 + 5] = lv
        with pytest.raises(SongError):
            read_compiled(fix_crc(bytes(x)))
    x = bytearray(b)
    x[5] = 0                                      # 2.0 with a start level
    with pytest.raises(SongError):
        read_compiled(fix_crc(bytes(x)))


def test_v21_bad_yaml(song_copy):
    from fm1song.source import load_song
    folder, edit = song_copy("saints")
    edit("  bass:\n", "  bass:\n    start_level: 9\n")
    with pytest.raises(SongError):
        load_song(folder)


def test_header_layout(blobs):
    b = blobs[0]
    assert b[:4] == b"FM1S" and b[4:6] == bytes([2, 0])       # M5: our songs (with a style) are 2.0
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
    (0, b"FM1X", "magic"), (4, b"\x03", "version"), (8, b"\x00\x00\x01\x00", "header says"),
    (16, b"\x10\x00", "metre"), (18, b"\x09", "metre"), (20, b"\x01\x00\x00\x00", "tempo"),
    (28, b"\x00\x00", "bars"), (30, b"\x05", "pickup"), (6, b"\xff\x00", "chunks"),
])
def test_header_fields(blobs, offset, value, expect):
    b = bytearray(blobs[0])
    b[offset:offset + len(value)] = value
    with pytest.raises(SongError, match=expect):
        read_compiled(fix_crc(b))


def test_v10_to_v13_same_music(blobs13, blobs10, blobs11):
    """1.0 / 1.1 / 1.2 carry the first 3 levels of each layer; 1.3 (M4) all of them (harmony 5, bass 4, drums 3)"""
    for a, b, c in zip(blobs10, blobs11, blobs13):
        ca, cb, cc = read_compiled(a), read_compiled(b), read_compiled(c)
        assert ca["version"] == "1.0" and cb["version"] == "1.1" and cc["version"] == "1.3" and len(b) < len(a) * 0.6
        for k in ("chord_events", "bars", "sections", "chords", "title", "key"):
            assert ca[k] == cb[k] == cc[k]
        for name, ly in cc["layers"].items():
            assert len(ly["variants"]) == {"harmony": 5, "bass": 4, "drums": 3, "melody": 1}[name]
            for v in range(1, min(3, len(ly["variants"])) + 1):
                assert ca["layers"][name]["variants"][v] == cb["layers"][name]["variants"][v] == ly["variants"][v]
            assert len(cb["layers"][name]["variants"]) == min(3, len(ly["variants"]))
        assert ca["category"] == cb["category"] == "" and cc["category"]   # every song of ours has one


def test_category(tmp_path):
    """song.yaml category: optional, printable ASCII up to 15 characters; in 1.2's CATG, not in 1.1"""
    import shutil
    from fm1song.errors import SongError
    folder = tmp_path / "s"
    shutil.copytree(SONGS[0], folder)
    y = (folder / "song.yaml").read_text()
    assert "category:" in y
    (folder / "song.yaml").write_text(y.replace("category:", "# category:", 1))
    assert read_compiled(compile_folder(folder)[0])["category"] == ""
    (folder / "song.yaml").write_text(y.replace("category:", "category: A very long category name #", 1))
    with pytest.raises(SongError):
        compile_folder(folder)


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


def test_v20_same_music_smaller(blobs, blobs13):
    """2.0 (M5): the generated levels are made by the reader (the FM-1) from the chart and style: the same notes as
    1.3, which stores them"""
    for a, b in zip(blobs13, blobs):
        ca, cb = read_compiled(a), read_compiled(b)
        assert cb["version"] == "2.0" and len(b) < len(a) * 0.2
        for k in ("chord_events", "bars", "sections", "chords", "title", "key", "category"):
            assert ca[k] == cb[k]
        for name, ly in ca["layers"].items():
            assert cb["layers"][name]["variants"] == ly["variants"], name
        assert cb["stored"] == {"melody": [1]} or "melody" in cb["stored"]


def test_ez_rules(blobs13, blobs10):
    from fm1song.binformat import ez_encode
    b = blobs13[0]
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
        "over 16384 events in all (1.3)": lambda c: c.__setitem__(b"EZD1", ez_encode([[0, 0xB0, 1, 0]] * 16385)),
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


def test_device_parser_agrees(blobs, blobs10, blobs11, blobs13, blobs21, tmp_path):
    """the C parser (firmware/app/song.c, via build/fuzz/song_fuzz --diff) accepts exactly what this reader accepts"""
    import subprocess
    from conftest import ROOT
    exe = ROOT / "build/fuzz/song_fuzz"
    if not exe.exists():
        pytest.skip("build tools/song_fuzz first (tools/song_fuzz.sh)")
    rng = random.Random(4)
    files, expect = [], {}
    seeds = blobs + blobs10 + blobs11 + blobs13 + blobs21
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
