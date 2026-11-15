# SPDX-License-Identifier: GPL-3.0-or-later
# M5 Part 2 (D029, option C): the FM-1's generators (firmware/app/gen.c) and the laptop's (tools/fm1song/arrange.py)
# make identical events for every song and level. The C side runs via build/fuzz/song_fuzz --events (song_fuzz.sh).
import subprocess
from pathlib import Path

import pytest

from conftest import ROOT, SONGS
from fm1song import compile_folder, read_compiled

LAYERS = ("drums", "bass", "harmony", "melody")
PRIVATE = sorted(p for p in (ROOT / "songs/private").glob("*/") if (p / "song.yaml").is_file() and p.name != "hallelujah")   # (M10: no test reads hallelujah)
BACKTRACKS = sorted(p for p in (ROOT / "songs/backtracks").glob("*/") if (p / "song.yaml").is_file())   # M8


def c_events(exe, blob, tmp_path):
    f = tmp_path / "s.fm1s"
    f.write_bytes(blob)
    out = subprocess.run([str(exe), "--events", str(f)], capture_output=True, text=True, check=True).stdout
    got = {}
    for line in out.splitlines():
        l, v, t, st, a, b = map(int, line.split())
        got.setdefault((LAYERS[l], v), []).append([t, st, a, b])
    return got


@pytest.mark.parametrize("folder", SONGS + PRIVATE + BACKTRACKS, ids=lambda p: p.name)
def test_c_and_python_generate_the_same(folder, tmp_path):
    exe = ROOT / "build/fuzz/song_fuzz"
    if not exe.exists():
        pytest.skip("build tools/song_fuzz first (tools/song_fuzz.sh)")
    blob = compile_folder(folder)[0]
    c = read_compiled(blob)
    assert c["version"] in ("2.0", "2.1") if folder not in BACKTRACKS else c["version"] == "2.2"   # (M7: 2.1 adds
    # start levels; the generators are the same. M8: the backtracks are 2.2)
    got = c_events(exe, blob, tmp_path)
    for name, ly in c["layers"].items():
        for v, evs in ly["variants"].items():
            want = [e for e in evs if e[1] in (0x80, 0x90)]
            assert got.get((name, v), []) == want, (folder.name, name, v)


# ------------------------------------------------------------------------------------------- M8 (D047, D048)
from fm1song import arrange as arr                                  # noqa: E402
from fm1song import compile_song                                    # noqa: E402
from fm1song.smf import meta_tempo, meta_timesig, write_midi       # noqa: E402
from fm1song.source import song_from_data                           # noqa: E402


def chart_song(style, bpb=4, swing=False, energy=True, kind="song", bars2=False, data_only=False):
    """a test chart (format 2.2): sections with roles and energy, a chord a bar, and (bars2) two chords in some bars"""
    row = (lambda *cs: "| " + " | ".join(cs) + " |")
    two = lambda a, b: " ".join([a] + ["."] * (bpb // 2 - 1) + [b] + ["."] * (bpb - bpb // 2 - 1))
    verse = row("C", "G", "Am", "F") if not bars2 else row(two("C", "G"), "Am", two("F", "Dm7"), "G7")
    data = {"format": "fm1-song/1", "slug": f"t-{style}", "title": f"Test {style}", "kind": kind,
            "rights": {"composition": "test"}, "credits": {},
            "music": {"key": "C", "time_signature": f"{bpb}/4", "tempo_bpm": 110, "style": style,
                      "feel": "swing" if swing else "straight"},
            "form": [{"section": "Intro", "bars": "1-4", "chart": "verse", "role": "intro"},
                     {"section": "Verse", "bars": "5-12", "chart": "verse2", "role": "verse"},
                     {"section": "Chorus", "bars": "13-20", "chart": "chorus", "role": "chorus",
                      "energy": 1 if energy and kind == "backtrack" else 0},      # (M9: energy is for backtracks only)
                     {"section": "Bridge", "bars": "21-24", "chart": "bridge", "role": "bridge",
                      "energy": 2 if energy and kind == "backtrack" else 0},
                     {"section": "Ending", "bars": "25", "chart": "ending_", "role": "ending"}],
            "chart": {"verse": verse, "verse2": verse + "\n" + verse, "chorus": row("Am", "F", "C", "G") * 1 + "\n" +
                      row("Am", "F", "C", "G7"), "bridge": row("F", "G", "Em", "G7"), "ending_": row("C")},
            "layers": {"drums": {"channel": 10}, "bass": {"channel": 3, "program": 33},
                       "harmony": {"channel": 2, "program": 5}}}
    if kind == "backtrack":
        data["backtrack"] = {"number": 1, "numerals": "I V vi IV", "scale": "major"}
    midi = write_midi(480, [("conductor", [(0, meta_tempo(round(60e6 / 110))), (0, meta_timesig(bpb, 4))])])
    if data_only:
        return data, midi
    song, _ = song_from_data(data, midi)
    return song


CASES = [(st, 4, False, False) for st in arr.NEW_STYLES] + [(st, 3, False, False) for st in arr.NEW_STYLES] + \
    [("funk", 4, True, False), ("bossa", 5, False, True), ("rock", 4, False, True), ("indie", 7, False, True),
     ("pop", 4, False, False), ("shuffle", 4, True, True)]


@pytest.mark.parametrize("case", CASES, ids=lambda c: f"{c[0]}-{c[1]}4{'-swing' if c[2] else ''}"
                                                      f"{'-2chords' if c[3] else ''}")
def test_m8_styles_c_and_python_the_same(case, tmp_path):
    """each new style (and old ones with energy): every level, with a chorus at energy +1 and a bridge at +2"""
    exe = ROOT / "build/fuzz/song_fuzz"
    if not exe.exists():
        pytest.skip("build tools/song_fuzz first (tools/song_fuzz.sh)")
    style, bpb, swing, two = case
    blob = compile_song(chart_song(style, bpb, swing, bars2=two))
    c = read_compiled(blob)
    assert c["version"] == "2.2" and [s["energy"] for s in c["sections"]] == [0, 0, 0, 0, 0]   # (M9: songs: none)
    got = c_events(exe, blob, tmp_path)
    for name, ly in c["layers"].items():
        for v, evs in ly["variants"].items():
            want = [e for e in evs if e[1] in (0x80, 0x90)]
            assert want, (style, name, v)
            assert got.get((name, v), []) == want, (style, name, v)


def test_m9_energy_is_for_backtracks_only():
    """the compiler refuses an energy offset in a song (the chorus lift is BACKTRACK's)"""
    import pytest
    from fm1song.errors import SongError
    data, midi = chart_song("rock", energy=False, data_only=True)
    data["form"][2]["energy"] = 1
    with pytest.raises(SongError, match="energy is for backtracks only"):
        song_from_data(data, midi)
    data["kind"] = "backtrack"
    data["backtrack"] = {"number": 1, "numerals": "I V vi IV", "scale": "major"}
    song_from_data(data, midi)                                      # (a backtrack takes it)


def test_m9_energy_changes_the_chorus_and_nothing_else():
    """BACKTRACK: energy +1 in the chorus, +2 in the bridge: its bars play that many tiers up (capped at 4); every
    other bar as without energy"""
    on = read_compiled(compile_song(chart_song("rock", energy=True, kind="backtrack")))
    off = read_compiled(compile_song(chart_song("rock", energy=False, kind="backtrack")))
    bt = 4 * 480
    assert [s["energy"] for s in on["sections"]] == [0, 0, 1, 2, 0]
    for name in ("drums", "bass", "harmony"):
        top = max(off["layers"][name]["variants"])
        assert top == 4
        for v in range(1, top + 1):
            for b in range(25):
                sec = on["bars"][b]["section"]
                e = on["sections"][sec]["energy"]
                pick = lambda c, lv: [x for x in c["layers"][name]["variants"][lv] if x[1] == 0x90 and x[0] // bt == b]
                assert pick(on, v) == pick(off, min(v + e, top)), (name, v, b)


STYLE_FM1 = list(range(len(arr.STYLE_NAMES)))


@pytest.mark.parametrize("style", STYLE_FM1, ids=lambda i: arr.STYLE_NAMES[i])
def test_m8_restyle_on_the_fm1_is_a_recompile(style, tmp_path):
    """the FM-1's song_restyle of a 4/4 POP song (M9: of kind song; a backtrack has the tier tables, tests/
    test_band_styles.py): WALTZ in 3/4, SHUFFLE swung: its events equal the same chart compiled in that style and
    metre"""
    exe = ROOT / "build/fuzz/song_fuzz"
    if not exe.exists():
        pytest.skip("build tools/song_fuzz first (tools/song_fuzz.sh)")
    name = arr.STYLE_NAMES[style]
    bpb = 3 if name == "waltz" else 4
    blob = compile_song(chart_song("pop"))
    want_c = read_compiled(compile_song(chart_song(name, bpb, name == "shuffle")))   # (a song: a backtrack is 4/4)
    f = tmp_path / "s.fm1s"
    f.write_bytes(blob)
    out = subprocess.run([str(exe), "--events", str(f), str(style), str(bpb)], capture_output=True, text=True,
                         check=True).stdout
    got = {}
    for line in out.splitlines():
        l, v, t, st, a, b = map(int, line.split())
        got.setdefault((LAYERS[l], v), []).append([t, st, a, b])
    for lname, ly in want_c["layers"].items():
        for v, evs in ly["variants"].items():
            assert got.get((lname, v), []) == [e for e in evs if e[1] in (0x80, 0x90)], (name, lname, v)
