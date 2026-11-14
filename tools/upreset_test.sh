#!/bin/sh
# upreset_test.sh: tools/upreset_test.c with firmware/app/upreset.c and fm1-base's storage.c, under ASan + UBSan (host gcc).
# The user presets' object on a simulated flash, a power cut at every operation of a save. Laptop only.
# Licence: TBD (see D017)
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT="$ROOT/build/upreset"
mkdir -p "$OUT"
gcc -std=gnu99 -O1 -g -Wall -Wextra -Werror -Wno-unused-function -fsanitize=address,undefined -fno-sanitize-recover=all \
    -fno-omit-frame-pointer -I "$ROOT/firmware/vendor/fm1-base/include" -I "$ROOT/firmware/app" -o "$OUT/upreset_test" \
    "$ROOT/tools/upreset_test.c"
exec "$OUT/upreset_test"
