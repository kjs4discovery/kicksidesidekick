#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# store_blocks.py: the song store's blocks (512 B; a 32-byte head + the file, whole blocks) for songs / backtracks, no device.
# Records of up to 8 blocks share sectors; a longer one starts on a sector (8-block units): reported as plain blocks here.
#   .venv/bin/python -I tools/store_blocks.py [--private]
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from fm1song import compile_folder   # noqa: E402


def blocks(folder):
    n = len(compile_folder(folder)[0]) + 32
    return -(-n // 512)


def main():
    priv = "--private" in sys.argv
    songs = sorted(p for p in (ROOT / "songs").iterdir() if (p / "song.yaml").is_file())
    if priv:
        songs += sorted(p for p in (ROOT / "songs/private").iterdir() if (p / "song.yaml").is_file() and p.name != "hallelujah")
    bts = sorted(p for p in (ROOT / "songs/backtracks").iterdir() if (p / "song.yaml").is_file())
    a, b = sum(map(blocks, songs)), sum(map(blocks, bts))
    print(f"songs {len(songs)}: {a} blocks; backtracks {len(bts)}: {b} blocks; total {a + b} of 552")


if __name__ == "__main__":
    main()
