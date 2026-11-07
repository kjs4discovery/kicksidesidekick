#!/bin/sh
# store_test.sh: build tools/store_test.c with firmware/app/store.c and song.c under ASan + UBSan (host gcc) and run it
# on the starter songs: the song store on a simulated flash, with a power cut at every operation. Laptop only.
# Licence: TBD (see D017)
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/build/store"
mkdir -p "$OUT"
for s in "$ROOT"/songs/*/; do         # (songs/private/: a folder of folders)
  [ -f "$s/song.yaml" ] || continue
  "$ROOT/tools/fm1song.sh" compile "$s" -o "$OUT/$(basename "$s").fm1s" >/dev/null
done
"$ROOT/tools/fm1song.sh" compile "$ROOT/songs/amazing-grace" --format 1.0 -o "$OUT/amazing-grace-v10.fm1s" >/dev/null
gcc -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all \
    -fno-omit-frame-pointer -o "$OUT/store_test" "$ROOT/tools/store_test.c" "$ROOT/firmware/app/store.c" \
    "$ROOT/firmware/app/song.c" "$ROOT/firmware/app/gen.c"
cd "$OUT"
exec ./store_test amazing-grace.fm1s amazing-grace-v10.fm1s four-chords.fm1s saints.fm1s scarborough-fair.fm1s \
    twelve-bar-blues.fm1s
