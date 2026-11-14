# M10 Part 5 (D062): the optional INFO chunk (the song info line): u8 length, that many printable ASCII bytes; 2.x only.
import shutil
import struct
import zlib

import pytest

from conftest import ROOT, SONGS
from fm1song import compile_folder, read_compiled
from fm1song import binformat as bf
from fm1song.errors import SongError

OLDER = ["amazing-grace", "twelve-bar-blues", "saints", "four-chords", "scarborough-fair", "greensleeves", "happy-birthday",
         "danny-boy", "oh-susanna"]
NEW = ["camptown-races", "clementine", "auld-lang-syne", "drunken-sailor", "blow-the-man-down", "michael-row"]
INFO = {"amazing-grace": "Trad. hymn tune, 1830s; words J. Newton", "twelve-bar-blues": "Tuneka original, 12-bar blues form",
        "saints": "Traditional spiritual, c. 1896", "four-chords": "Tuneka original",
        "scarborough-fair": "Traditional English ballad", "greensleeves": "Traditional English, 16th century",
        "happy-birthday": "Mildred J. Hill & Patty Hill, 1893", "danny-boy": "Irish traditional, first printed 1855",
        "oh-susanna": "Stephen Foster, 1848"}


def strip_chunk(blob, fourcc):
    """the file without one chunk, laid out as compile_song lays a file out (the directory, offsets, size and CRC rewritten)"""
    magic, vmaj, vmin, n, size, crc, *rest = bf.HEADER.unpack_from(blob)
    chunks = []
    for i in range(n):
        f, off, ln = bf.DIRENT.unpack_from(blob, bf.HEADER.size + i * bf.DIRENT.size)
        if f != fourcc:
            chunks.append((f, blob[off:off + ln]))
    offset = bf.HEADER.size + bf.DIRENT.size * len(chunks)
    directory, payload = b"", b""
    for f, body in chunks:
        directory += bf.DIRENT.pack(f, offset + len(payload), len(body))
        payload += body + bytes(-len(body) % 4)
    head = bf.HEADER.pack(magic, vmaj, vmin, len(chunks), offset + len(payload), 0, *rest)
    out = head + directory + payload
    return out[:12] + struct.pack("<I", zlib.crc32(out)) + out[16:]


def folder_without_info(slug, tmp_path):
    d = tmp_path / slug
    shutil.copytree(ROOT / "songs" / slug, d)
    y = d / "song.yaml"
    y.write_text("".join(l for l in y.read_text().splitlines(True) if not l.startswith("info:")))
    return d


@pytest.mark.parametrize("slug", OLDER)
def test_an_older_song_is_the_m9_file_plus_the_info_chunk(slug, tmp_path):
    """the compiled file minus INFO = the file without `info:` (the M9 file): byte for byte; every event stream identical"""
    with_info = compile_folder(ROOT / "songs" / slug)[0]
    without = compile_folder(folder_without_info(slug, tmp_path))[0]
    a, b = read_compiled(with_info), read_compiled(without)
    assert a["info"] == INFO[slug] and b["info"] == ""
    assert strip_chunk(with_info, b"INFO") == without
    assert len(with_info) > len(without) and a["version"] == b["version"] and a["version"] in ("2.0", "2.1")
    assert a["unknown_chunks"] == [] and a["layers"] == b["layers"]
    assert all(len(v) <= 39 for v in INFO.values())


@pytest.mark.parametrize("slug", NEW)
def test_a_new_song_has_its_info_line(slug):
    c = read_compiled(compile_folder(ROOT / "songs" / slug)[0])
    assert 1 <= len(c["info"]) <= 39 and c["unknown_chunks"] == []


def test_private_and_imported_songs_have_no_info():
    for d in (ROOT / "songs/private").glob("*/"):
        if (d / "song.yaml").is_file() and d.name != "hallelujah":
            assert read_compiled(compile_folder(d)[0])["info"] == ""


def test_the_info_chunk_is_last_and_not_in_an_old_format(tmp_path):
    blob = compile_folder(ROOT / "songs/saints")[0]
    n = struct.unpack_from("<H", blob, 6)[0]
    assert bf.DIRENT.unpack_from(blob, bf.HEADER.size + (n - 1) * bf.DIRENT.size)[0] == b"INFO"
    for fmt in ("1.3", "1.1", "1.0"):
        old = compile_folder(ROOT / "songs/saints", fmt) if False else None
    import subprocess
    out = tmp_path / "o.fm1s"
    subprocess.run([str(ROOT / "tools/fm1song.sh"), "compile", str(ROOT / "songs/saints"), "--format", "1.3", "-o", str(out)],
                   check=True, capture_output=True)
    assert b"INFO" not in out.read_bytes()[:400] and read_compiled(out.read_bytes())["info"] == ""


def _damage(blob, new_body):
    """the INFO chunk's body replaced (it is the last chunk): lengths and CRC fixed up"""
    n = struct.unpack_from("<H", blob, 6)[0]
    f, off, ln = bf.DIRENT.unpack_from(blob, bf.HEADER.size + (n - 1) * bf.DIRENT.size)
    out = bytearray(blob[:off] + new_body + bytes(-len(new_body) % 4))
    struct.pack_into("<I", out, bf.HEADER.size + (n - 1) * bf.DIRENT.size + 8, len(new_body))
    struct.pack_into("<I", out, 8, len(out))
    struct.pack_into("<I", out, 12, 0)
    struct.pack_into("<I", out, 12, zlib.crc32(bytes(out)))
    return bytes(out)


@pytest.mark.parametrize("body", [b"", b"\x05abc", b"\x03abcd", b"\x28" + b"a" * 40, b"\x03a\x01b", b"\x03a\xe9b", b"\x00x"])
def test_a_damaged_info_chunk_is_refused(body):
    blob = compile_folder(ROOT / "songs/saints")[0]
    with pytest.raises(SongError, match="INFO"):
        read_compiled(_damage(blob, body))


def test_an_info_line_of_39_is_the_longest(tmp_path):
    d = tmp_path / "s"
    shutil.copytree(ROOT / "songs/saints", d)
    y = d / "song.yaml"
    txt = y.read_text()
    y.write_text(txt.replace("info: 'Traditional spiritual, c. 1896'", "info: '" + "x" * 40 + "'"))
    with pytest.raises(SongError, match="longer than 39"):
        compile_folder(d)
    y.write_text(txt.replace("info: 'Traditional spiritual, c. 1896'", "info: '" + "x" * 39 + "'"))
    assert read_compiled(compile_folder(d)[0])["info"] == "x" * 39
