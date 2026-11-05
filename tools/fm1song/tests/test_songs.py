# The starter songs, the source checks (with bar:beat in the messages) and the chord helpers.
import pytest

from conftest import SONGS
from fm1song import SongError, compile_folder, load_song, read_compiled
from fm1song import chords as ch
from fm1song.smf import meta_tempo, write_midi


@pytest.mark.parametrize("folder", SONGS, ids=lambda p: p.name)
def test_starter_song_compiles_cleanly(folder):
    blob, diag = compile_folder(folder)
    assert not diag.items, [str(d) for d in diag.items]
    c = read_compiled(blob)
    assert set(c["layers"]) == {"drums", "bass", "harmony", "melody"}
    for name in ("drums", "bass", "harmony"):
        assert len(c["layers"][name]["variants"]) >= 2, name
    assert c["sections"] and c["chord_events"]


@pytest.mark.parametrize("folder", SONGS, ids=lambda p: p.name)
def test_compile_is_deterministic(folder):
    assert compile_folder(folder)[0] == compile_folder(folder)[0]


def test_amazing_grace_facts():
    song, _ = load_song(SONGS[0].parent / "amazing-grace")
    assert (song.beats_per_bar, song.pickup_beats, song.bar_count) == (3, 1, 33)
    assert song.start_tick == 2 * 480
    syms = [song.chord_table[i][0] for _, i, _, _ in song.chord_events]
    # bars 4-5 are both G: one event, so the screen's "next chord" is the real next change
    assert syms[:8] == ["N.C.", "G", "G7", "C", "G", "Em", "D", "D7"]
    assert song.bar_beat(31 * 1440 + 960 + 240) == "31:3+240"


def errors_of(folder):
    with pytest.raises(SongError) as e:
        compile_folder(folder)
    return [str(d) for d in e.value.diagnostics if d.level == "error"]


@pytest.mark.parametrize("old,new,expect", [
    ("format: fm1-song/1", "format: fm1-song/9", "format must be"),
    ("tempo_bpm: 84", "tempo_bpm: 400", "tempo 400 bpm"),
    ("tempo_bpm: 84", "tempo_bpm: 90", "tempo 84.0 bpm, song.yaml says 90"),
    ("time_signature: 3/4", "time_signature: 6/8", "v1 supports"),
    ("pickup_beats: 1 ", "pickup_beats: 3 ", "pickup_beats 3"),
    ("| G | G7 | C  | G | G  | Em | D | D7     |", "| G | G7 | C  | G | G  | Em | D |", "chart.verse has 15"),
    ("| G | G7 | C  | G | Em | D  | G | G . D7 |", "| G | G7 | C  | G | Em | D  | G | G . |", "one token per beat"),
    ("| G | G7 | C  | G | Em | D  | G | G . D7 |", "| G | G7 | Cx | G | Em | D  | G | G . D7 |", "unknown quality 'x'"),
    ("| G | G7 | C  | G | Em | D  | G | G . D7 |", "| G | G7 | C D | G | Em | D  | G | G . D7 |", "do not split 3 beats"),
    ("{ section: Verse 2, bars: 17-32, chart: verse }", "{ section: Verse 2, bars: 18-33, chart: verse }",
     "expected bar 17"),
    ("chart: verse }\n  - { section: Verse 2", "chart: chorus }\n  - { section: Verse 2", "no chart part 'chorus'"),
    ("    channel: 10", "    channel: 9", "drums must be on channel 10"),
    ("    program: 33 ", "    program: 0 ", "program 0"),
    ("title: Amazing Grace", "title: Amazing Grace é", "printable ASCII"),
    ("  pickup: \"| N.C. |\"\n", "", "needs a one-bar 'pickup'"),
])
def test_yaml_errors(song_copy, old, new, expect):
    folder, edit = song_copy()
    edit(old, new)
    errs = errors_of(folder)
    assert any(expect in e for e in errs), errs


def _midi_song(song_copy, tracks):
    folder, _ = song_copy()
    conductor = ("conductor", [(0, meta_tempo(714286))])
    (folder / "song.mid").write_bytes(write_midi(480, [conductor] + tracks))
    (folder / "song.yaml").write_text((folder / "song.yaml").read_text().replace(
        "  drums:\n    channel: 10\n    variants:\n      1: kick on 1, ride on every beat\n"
        "      2: adds side-stick on 2 and 3, small fills every 8 bars\n", ""))
    return folder


def _track(name, notes, chan):
    evs = []
    for start, end, pitch in notes:
        evs += [(start, bytes([0x90 | chan, pitch, 80]))]
        if end is not None:
            evs += [(end, bytes([0x80 | chan, pitch, 0]))]
    return (name, evs)


def _ok_tracks():
    return [_track("bass:1", [(960, 1400, 43)], 2), _track("bass:2", [(960, 1400, 43)], 2),
            _track("harmony:1", [(960, 1400, 67)], 1), _track("harmony:2", [(960, 1400, 67)], 1),
            _track("melody", [(960, 1400, 62)], 0)]


def test_minimal_midi_is_fine(song_copy):
    folder = _midi_song(song_copy, _ok_tracks())
    blob, diag = compile_folder(folder)
    assert "drums" not in read_compiled(blob)["layers"]


@pytest.mark.parametrize("bad,expect", [
    (_track("melody", [(5000, None, 62)], 0), "no-off at"),
    (_track("melody", [(47520, 47600, 62)], 0), "starts after the end of the song (bar 32)"),
    (_track("melody", [(100, 500, 62)], 0), "starts before the pickup"),
    (_track("bass:3", [(960, 1000, 40)], 2), "must be 1, 1-2 or 1-3"),
    (_track("harmony:1", [(960, 1000, 60)], 1), "two tracks for harmony:1"),
])
def test_midi_errors(song_copy, bad, expect):
    tracks = [t for t in _ok_tracks() if not (bad[0] == "melody" and t[0] == "melody")] + [bad]
    if bad[0] == "bass:3":
        tracks = [t for t in tracks if not t[0].startswith("bass")] + [bad]
    folder = _midi_song(song_copy, tracks)
    errs = errors_of(folder)
    if expect == "no-off at":
        assert any("D4 has no note-off" in e and "at 3:2+200" in e for e in errs), errs
    else:
        assert any(expect in e for e in errs), errs


def test_midi_bytes_errors(song_copy):
    folder, _ = song_copy()
    mid = folder / "song.mid"
    data = mid.read_bytes()
    for blob, expect in [(b"RIFF" + data[4:], "not a Standard MIDI File"),
                         (data[:200], "runs past the end"),
                         (data[:14] + b"MTrk\x00\x00\x00\x03\x00\x99\x40", "ends inside an event")]:
        mid.write_bytes(blob)
        errs = errors_of(folder)
        assert any(expect in e for e in errs), (expect, errs)


def test_chord_symbols():
    assert ch.parse_chord("N.C.") is None
    c = ch.parse_chord("F#m7b5")
    assert (c.root, c.intervals) == (6, (0, 3, 6, 10))
    assert ch.parse_chord("C/E").bass == 4
    assert ch.parse_chord("Bb").root == 10
    for bad in ("H", "Cmaj13", "C/H", "", "Abmaj7/Eb"):
        with pytest.raises(ValueError):
            ch.parse_chord(bad)


def test_voicings():
    g, c, d7 = (ch.parse_chord(s) for s in ("G", "C", "D7"))
    assert ch.voicing_level1(g) == [55, 59, 62]                  # G3 B3 D4
    assert ch.voicing_level1(c) == [60, 64, 67]
    v = ch.voicings([g, c, d7, g])
    assert v[1][1] == [55, 60, 64]                                # C after G: G3 C4 E4, a common tone kept
    assert all(len(l2) <= ch.MAX_NOTES and 52 <= min(l2) <= 63 for _, l2 in v)
