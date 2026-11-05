#!/usr/bin/env bash
# build_hello.sh -- build the Step 0.6 hello world (commit 4629478's firmware/), the fallback diagnostic for Step 0.7:
# if our prototype misbehaves on the FM-1, this tells a hardware problem from a prototype problem.
# Licence: TBD (see D017)
#
#   tools/build_hello.sh      -> build/hello/fm1acc-hello.fwsc (and its sha256)
#
# Builds from git (git archive), so the result is the 0.6 source whatever the working tree holds. Builds only;
# installs nothing.
set -eu
cd "$(dirname "$0")/.."
ROOT="$(pwd)"
COMMIT=4629478
OUT=build/hello
rm -rf "$OUT/src"
mkdir -p "$OUT/src"
git archive "$COMMIT" firmware | tar -x -C "$OUT/src"
JIELI_TOOLCHAIN="$ROOT/third_party/jieli/toolchain" AC79_SDK="$ROOT/third_party/jieli/fw-AC79_AIoT_SDK" \
  PYTHON="$ROOT/.venv/bin/python" "$OUT/src/firmware/build.sh" | grep -E '^(app|loader|package) '
cp "$OUT/src/firmware/app/build/fm1acc.fwsc" "$OUT/fm1acc-hello.fwsc"
(cd "$OUT" && sha256sum fm1acc-hello.fwsc | tee fm1acc-hello.fwsc.sha256)
echo "hello world (v0.0.1, commit $COMMIT): $OUT/fm1acc-hello.fwsc"
