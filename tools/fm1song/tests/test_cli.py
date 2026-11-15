# SPDX-License-Identifier: GPL-3.0-or-later
from conftest import SONGS
from fm1song.cli import main


def test_validate_all(capsys):
    assert main(["validate"] + [str(s) for s in SONGS]) == 0
    assert capsys.readouterr().out.count(": ok") == len(SONGS)


def test_compile_info_dump(tmp_path, capsys):
    out = tmp_path / "ag.fm1s"
    assert main(["compile", str(SONGS[0]), "-o", str(out)]) == 0
    assert main(["info", str(out)]) == 0
    assert "Amazing Grace" in capsys.readouterr().out
    assert main(["dump", "--json", str(out)]) == 0
    assert '"slug": "amazing-grace"' in capsys.readouterr().out


def test_validate_reports_bar_beat(tmp_path, capsys):
    folder = tmp_path / "x"
    folder.mkdir()
    (folder / "song.yaml").write_text((SONGS[0] / "song.yaml").read_text().replace("G . D7", "G . Q7"))
    (folder / "song.mid").write_bytes((SONGS[0] / "song.mid").read_bytes())
    assert main(["validate", str(folder)]) == 1
    assert "chart.verse bar 16 beat 3" in capsys.readouterr().err


def test_missing_file(tmp_path, capsys):
    assert main(["validate", str(tmp_path)]) == 1
    assert "song.yaml not found" in capsys.readouterr().err


def test_send_skips_non_songs(tmp_path, capsys):
    # songs/* also matches make_songs.py and README.md: skipped with a note, and nothing at all to send -> exit 2
    # before any FM-1 is looked for (no device is touched)
    (tmp_path / "notes.txt").write_text("x")
    (tmp_path / "empty").mkdir()
    assert main(["send", str(tmp_path / "notes.txt"), str(tmp_path / "empty")]) == 2
    out = capsys.readouterr()
    assert out.out.count("skipped") == 2 and "no songs to send" in out.err
