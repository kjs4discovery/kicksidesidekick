#!/bin/sh
# vsynth_test.sh: build tools/vsynth_test.c with firmware/app/vsynth.c (and the vendored Felucca-Engines) under ASan +
# UBSan (host gcc) and run it: every SYNTH preset held, released and arpeggiated. Laptop only.
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/build/vsynth"
mkdir -p "$OUT"
gcc -std=c99 -O1 -g -Wall -Wextra -Wno-unused-const-variable -Wno-unused-function -fsanitize=address,undefined \
    -fno-sanitize-recover=all -fno-omit-frame-pointer -I"$ROOT/firmware/vendor/felucca-engines/src" \
    -o "$OUT/vsynth_test" "$ROOT/tools/vsynth_test.c" "$ROOT/firmware/app/vsynth.c" -lm
exec "$OUT/vsynth_test"
