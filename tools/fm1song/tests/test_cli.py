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
