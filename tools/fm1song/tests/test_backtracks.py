# M10 (D060) Part 4: the backtrack generator's numeral parser (tools/backtracks.py) and the table of 42.
import importlib.util

import pytest

from conftest import ROOT

spec = importlib.util.spec_from_file_location("backtracks", ROOT / "tools/backtracks.py")
bt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bt)


@pytest.mark.parametrize("numeral,tonic,want", [
    ("imaj7", 9, "Ammaj7"), ("i6", 9, "Am6"), ("i7", 9, "Am7"), ("ii7b5", 9, "Bm7b5"), ("II", 0, "D"), ("bII", 9, "Bb"),
    ("IVmaj7", 0, "Fmaj7"), ("iii7", 0, "Em7"), ("Imaj7", 0, "Cmaj7"), ("VI7", 0, "A7"), ("ii7", 0, "Dm7"), ("V7", 0, "G7"),
    ("bVII", 0, "Bb"), ("iv", 9, "Dm"), ("I", 0, "C"),
])
def test_numerals_make_the_chords(numeral, tonic, want):
    assert bt.chord(numeral, tonic) == want


@pytest.mark.parametrize("numeral", ["I6", "V6", "ii9", "Imaj9", "I7b5", "vi7b5x", "iiimaj", "VIII", "i#", "ivmaj7b5"])
def test_an_unknown_numeral_or_extension_is_an_error_not_a_silent_drop(numeral):
    with pytest.raises(SystemExit, match="backtracks: numeral"):
        bt.chord(numeral, 0)


def test_the_table_has_42_in_order_and_the_new_six_exactly():
    rows = bt.rows()
    assert [r["number"] for r in rows] == list(range(1, 43))
    new = {r["number"]: r for r in rows if r["number"] > 36}
    assert [(r["name"], " ".join(r["verse"]), r["chorus"], r["mode"]) for r in new.values()] == [
        ("PHRYGIAN", "i bII", None, "minor"), ("LINE CLICHE", "i imaj7 i7 i6", None, "minor"),
        ("MINOR CADENCE", "i iv V i", None, "minor"), ("DESCENT", "IVmaj7 iii7 ii7 Imaj7", None, "major"),
        ("LYDIAN", "I II", None, "major"), ("JAZZ BLUES", "I7 IV7 I7 I7 IV7 IV7 I7 VI7 ii7 V7 I7 V7", None, "blues")]


def test_the_new_progressions_play_the_right_chords():
    out = {r["number"]: bt.song_data(r)["chart"] for r in bt.rows() if r["number"] > 36}
    assert list(out[38].values())[0] == "| Am | Ammaj7 | Am7 | Am6 |"
    assert list(out[37].values())[0] == "| Am | Bb | Am | Bb |"
    assert list(out[41].values())[0] == "| C | D | C | D |"
    assert "| C7 | F7 | C7 | C7 | F7 | F7 | C7 | A7 | Dm7 | G7 | C7 | G7 |" in out[42].values()


def test_every_backtrack_has_numerals_that_fit_the_forms_field():
    for r in bt.rows():
        assert len(bt.song_data(r)["backtrack"]["numerals"]) <= 39
