#!/usr/bin/env bash
# build.sh -- build the firmware: firmware/app/build/fm1acc.fwsc (and its app and update loader).
#   firmware/build.sh [build.py options, e.g. --host]
# SPDX-License-Identifier: GPL-3.0-or-later
# Uses fm1-base's tools/build.py (firmware/vendor/fm1-base), the JieLi toolchain and AC79 SDK fetched in Step 0.5
# (third_party/jieli), and the repo's Python venv (Pillow, fontTools). Builds only; installs nothing.
set -eu
cd "$(dirname "$0")"
ROOT="$(cd .. && pwd)"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$ROOT/third_party/jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$ROOT/third_party/jieli/fw-AC79_AIoT_SDK}"
PY="${PYTHON:-$ROOT/.venv/bin/python}"
[ -x "$JIELI_TOOLCHAIN/pi32v2/bin/clang" ] || { echo "build.sh: no JieLi toolchain in $JIELI_TOOLCHAIN (Step 0.5)"; exit 1; }
[ -f "$AC79_SDK/cpu/wl82/tools/uboot.boot" ] || { echo "build.sh: no AC79 SDK in $AC79_SDK (Step 0.5)"; exit 1; }
ulimit -n 8192 2>/dev/null || true
exec "$PY" vendor/fm1-base/tools/build.py app "$@"
