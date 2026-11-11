# M5 Part 2 (D029, option C): the FM-1's generators (firmware/app/gen.c) and the laptop's (tools/fm1song/arrange.py)
# make identical events for every song and level. The C side runs via build/fuzz/song_fuzz --events (song_fuzz.sh).
import subprocess
from pathlib import Path

import pytest

from conftest import ROOT, SONGS
from fm1song import compile_folder, read_compiled

LAYERS = ("drums", "bass", "harmony", "melody")
PRIVATE = sorted(p for p in (ROOT / "songs/private").glob("*/") if (p / "song.yaml").is_file())


def c_events(exe, blob, tmp_path):
    f = tmp_path / "s.fm1s"
    f.write_bytes(blob)
    out = subprocess.run([str(exe), "--events", str(f)], capture_output=True, text=True, check=True).stdout
    got = {}
    for line in out.splitlines():
        l, v, t, st, a, b = map(int, line.split())
        got.setdefault((LAYERS[l], v), []).append([t, st, a, b])
    return got


@pytest.mark.parametrize("folder", SONGS + PRIVATE, ids=lambda p: p.name)
def test_c_and_python_generate_the_same(folder, tmp_path):
    exe = ROOT / "build/fuzz/song_fuzz"
    if not exe.exists():
        pytest.skip("build tools/song_fuzz first (tools/song_fuzz.sh)")
    blob = compile_folder(folder)[0]
    c = read_compiled(blob)
    assert c["version"] in ("2.0", "2.1")             # (M7: 2.1 adds start levels; the generators are the same)
    got = c_events(exe, blob, tmp_path)
    for name, ly in c["layers"].items():
        for v, evs in ly["variants"].items():
            want = [e for e in evs if e[1] in (0x80, 0x90)]
            assert got.get((name, v), []) == want, (folder.name, name, v)
