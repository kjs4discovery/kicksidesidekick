import shutil
import sys
from pathlib import Path

import pytest

TOOLS = Path(__file__).resolve().parents[2]
ROOT = TOOLS.parent
sys.path.insert(0, str(TOOLS))
SONGS = sorted(p for p in (ROOT / "songs").iterdir() if (p / "song.yaml").is_file())


@pytest.fixture
def song_copy(tmp_path):
    """copy a song folder to tmp and return (folder, edit_yaml(old, new))"""
    def make(slug="amazing-grace"):
        dst = tmp_path / slug
        shutil.copytree(ROOT / "songs" / slug, dst, ignore=shutil.ignore_patterns("previews", "*.py"))

        def edit(old, new):
            p = dst / "song.yaml"
            text = p.read_text()
            assert old in text, old
            p.write_text(text.replace(old, new, 1))
        return dst, edit
    return make
