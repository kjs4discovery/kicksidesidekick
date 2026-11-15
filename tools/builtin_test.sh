#!/bin/sh
# builtin_test.sh [SECONDS]: the built-in starter library (M10.4) on the laptop: tools/builtin_pack.py's header for the current songs, the
# library's files compiled the way `fm1song send` compiles them, then tools/builtin_test.c with firmware/app/builtin.c and song.c under
# ASan + UBSan: every built-in unpacks to the compiled file byte for byte, its CRC and metadata match, song_open and every level build, and
# SECONDS (default 30) of random corruptions of the pack give errors only. Also: the pack tool refuses private and unlisted paths. Laptop only.
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/build/builtin"
mkdir -p "$OUT"
rm -f "${OUT:?}"/*.fm1s
PY="$ROOT/.venv/bin/python"
for p in songs/private/anything songs/hallelujah ../etc/passwd /etc/passwd; do   # the refusals
  if "$PY" "$ROOT/tools/builtin_pack.py" --check "$p" >/dev/null 2>&1; then echo "FAIL: builtin_pack --check $p was accepted"; exit 1; fi
done
echo "builtin_pack refuses private, unlisted and outside paths: ok"
(cd "$ROOT/firmware/app" && "$PY" "$ROOT/tools/builtin_pack.py" "$OUT/builtin_gen.h")
grep -v '^[[:space:]]*\(#\|$\)' "$ROOT/songs/library.txt" | while read -r e; do
  # (a song's slug is its folder's name: the same name the compiled file gets here)
  "$ROOT/tools/fm1song.sh" compile "$ROOT/$e" -o "$OUT/$(basename "$e").fm1s" >/dev/null
done
gcc -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all \
    -fno-omit-frame-pointer -I"$OUT" -I"$ROOT/firmware/app" -o "$OUT/builtin_test" "$ROOT/tools/builtin_test.c" "$ROOT/firmware/app/builtin.c" \
    "$ROOT/firmware/app/song.c" "$ROOT/firmware/app/gen.c"
exec "$OUT/builtin_test" "$OUT" "${1:-30}"
