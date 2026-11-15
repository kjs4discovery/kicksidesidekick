#!/bin/sh
# song_fuzz.sh [SECONDS]: build tools/song_fuzz.c with firmware/app/song.c under ASan + UBSan (host gcc) and fuzz the
# device's song parser (and the 2.0 generators, firmware/app/gen.c) with the compiled starter songs as seeds, in formats
# 2.0, 2.1 (M7), 2.2 (M8), 1.3, 1.1 and 1.0 (default 600 s). Laptop only.
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/build/fuzz"
mkdir -p "$OUT"
rm -f "$OUT"/*.fm1s
for s in "$ROOT"/songs/*/; do       # seeds in all versions: 2.0 (generated levels), 1.3 (5 levels), 1.1 (delta-coded
  [ -f "$s/song.yaml" ] || continue  # events), 1.0 (8-byte) (songs/private/: a folder of folders)
  "$ROOT/tools/fm1song.sh" compile "$s" -o "$OUT/$(basename "$s").fm1s" >/dev/null
  "$ROOT/tools/fm1song.sh" compile "$s" --format 1.3 -o "$OUT/$(basename "$s")-v13.fm1s" >/dev/null
  "$ROOT/tools/fm1song.sh" compile "$s" --format 1.1 -o "$OUT/$(basename "$s")-v11.fm1s" >/dev/null
  "$ROOT/tools/fm1song.sh" compile "$s" --format 1.0 -o "$OUT/$(basename "$s")-v10.fm1s" >/dev/null
done
# M7: a 2.1 seed (start levels in LAYR's flags byte): a copy of saints with bass and harmony opening on L2 / L3
T="$OUT/v21-saints"
rm -rf "$T" && cp -r "$ROOT/songs/saints" "$T" && rm -rf "$T/previews"
sed -i 's/^  bass:$/  bass:\n    start_level: 2/; s/^  harmony:$/  harmony:\n    start_level: 3/' "$T/song.yaml"
"$ROOT/tools/fm1song.sh" compile "$T" -o "$OUT/saints-v21.fm1s" >/dev/null
# M8: 2.2 seeds: that copy in FUNK with roles (FORM; M9: energy is for backtracks only), and every backtrack
# (songs/backtracks/: roles, energy, the tier tables)
sed -i 's/^  style: march/  style: funk/; 0,/chart: \([a-z]*\) }/s//chart: \1, role: verse }/; s/chart: \([a-z]*\) }$/chart: \1, role: chorus }/' "$T/song.yaml"      # (M9: energy is for backtracks only: the backtracks below)
"$ROOT/tools/fm1song.sh" compile "$T" -o "$OUT/saints-v22.fm1s" >/dev/null
for s in "$ROOT"/songs/backtracks/*/; do
  [ -f "$s/song.yaml" ] || continue
  "$ROOT/tools/fm1song.sh" compile "$s" -o "$OUT/bt-$(basename "$s").fm1s" >/dev/null
done
gcc -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all \
    -fno-omit-frame-pointer -o "$OUT/song_fuzz" "$ROOT/tools/song_fuzz.c" "$ROOT/firmware/app/song.c" "$ROOT/firmware/app/gen.c"
exec "$OUT/song_fuzz" "${1:-600}" "$OUT"/*.fm1s
