#!/bin/sh
# song_fuzz.sh [SECONDS]: build tools/song_fuzz.c with firmware/app/song.c under ASan + UBSan (host gcc) and fuzz the
# device's song parser with the compiled starter songs as seeds, in formats 1.2, 1.1 and 1.0 (default 600 s). Laptop
# only.
# Licence: TBD (see D017)
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/build/fuzz"
mkdir -p "$OUT"
rm -f "$OUT"/*.fm1s
for s in "$ROOT"/songs/*/; do       # seeds in all versions: 1.2 (+ category), 1.1 (delta-coded events), 1.0 (8-byte)
  [ -f "$s/song.yaml" ] || continue  # (songs/private/: a folder of folders)
  "$ROOT/tools/fm1song.sh" compile "$s" -o "$OUT/$(basename "$s").fm1s" >/dev/null
  "$ROOT/tools/fm1song.sh" compile "$s" --format 1.1 -o "$OUT/$(basename "$s")-v11.fm1s" >/dev/null
  "$ROOT/tools/fm1song.sh" compile "$s" --format 1.0 -o "$OUT/$(basename "$s")-v10.fm1s" >/dev/null
done
gcc -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all \
    -fno-omit-frame-pointer -o "$OUT/song_fuzz" "$ROOT/tools/song_fuzz.c" "$ROOT/firmware/app/song.c"
exec "$OUT/song_fuzz" "${1:-600}" "$OUT"/*.fm1s
