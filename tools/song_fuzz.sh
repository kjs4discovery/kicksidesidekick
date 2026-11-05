#!/bin/sh
# song_fuzz.sh [SECONDS]: build tools/song_fuzz.c with firmware/app/song.c under ASan + UBSan (host gcc) and fuzz the
# device's song parser with the compiled starter songs as seeds (default 600 s). Laptop only.
# Licence: TBD (see D017)
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/build/fuzz"
mkdir -p "$OUT"
for s in "$ROOT"/songs/*/; do "$ROOT/tools/fm1song.sh" compile "$s" -o "$OUT/$(basename "$s").fm1s" >/dev/null; done
gcc -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all \
    -fno-omit-frame-pointer -o "$OUT/song_fuzz" "$ROOT/tools/song_fuzz.c" "$ROOT/firmware/app/song.c"
exec "$OUT/song_fuzz" "${1:-600}" "$OUT"/*.fm1s
