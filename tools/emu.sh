#!/usr/bin/env bash
# emu.sh -- build the firmware and run it in fm1-emu. Never touches the FM-1.
# Licence: TBD (see D017)
#
#   tools/emu.sh                         build, then play it in the emulator window (keys A W S E D F ... = C4..)
#   tools/emu.sh --shot [MS] [KEYS]      build, run MS ms headless (default 2500, C4 held from 1500 ms),
#                                        write build/emu/panel.png and build/emu/audio.raw (s16le 44.1 kHz stereo)
#   tools/emu.sh --no-build ...          skip the build
#   FM1_FLASH_ID=b36014 tools/emu.sh     the emulated flash chip answers that JEDEC id (tools/patches/fm1-emu-jedec-id.patch;
#                                        run tools/apply_patches.sh once after refreshing third_party/fm1-emu)
set -eu
cd "$(dirname "$0")/.."
EMU=third_party/fm1-emu/target/release/fm1-live
PKG=firmware/app/build/fm1acc.fwsc
build=1
if [ "${1:-}" = "--no-build" ]; then build=0; shift; fi
[ -x "$EMU" ] || { echo "emu.sh: no $EMU (Step 0.4; then tools/apply_patches.sh)"; exit 1; }
if [ "$build" = 1 ]; then firmware/build.sh | grep -E '^(app|loader|package) ' ; fi
[ -f "$PKG" ] || { echo "emu.sh: no $PKG"; exit 1; }
if [ "${1:-}" = "--shot" ]; then
  ms=${2:-2500}
  keys=${3:-1500:C4}
  mkdir -p build/emu
  FM1_BENCH_KEYS="$keys" FM1_BENCH_WAV=build/emu/audio.raw FM1_BENCH_PANEL=build/emu/panel.ppm \
    "$EMU" --fwsc "$PKG" --bench "$ms" | grep -E 'emulated|halt|panel'
  python3 -I tools/ppm2png.py build/emu/panel.ppm build/emu/panel.png
else
  exec "$EMU" --fwsc "$PKG" --midi all
fi
