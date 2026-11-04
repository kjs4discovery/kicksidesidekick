#!/usr/bin/env bash
# build_bases.sh -- build the Step 0.5 base candidates with the JieLi toolchain in third_party/jieli.
#
#   tools/build_bases.sh [NAME ...]     (default: all; names as in the list below)
#
# Runs each project's own build script (third-party code: read in Step 0.5, no network, no sudo,
# deletes only inside its own build folder). Writes nothing to the FM-1. Logs go to
# build-logs/NAME.log in the repo; prints the exit code, time and the package line of each.
set -u
cd "$(dirname "$0")/.."
ROOT=$PWD
export JIELI_TOOLCHAIN="$ROOT/third_party/jieli/toolchain"
export AC79_SDK="$ROOT/third_party/jieli/fw-AC79_AIoT_SDK"
export PYTHON="$ROOT/.venv/bin/python"
ulimit -n 8192 2>/dev/null || true
mkdir -p build-logs

# name | folder | command
TARGETS='
felucca|third_party/Felucca|./build.sh
fm1-base|third_party/fm1-base|'"$PYTHON"' tools/build.py template
polly|third_party/fm1-polly|./build.sh
trio|third_party/fm1-trio|./build.sh
fm1-dock|third_party/fm1-dock|./build.sh
fm1-chord|third_party/fm1-chord|./build.sh
coralflower|third_party/CoralFlower|./build.sh
'

want=" $* "
echo "$TARGETS" | while IFS='|' read -r name dir cmd; do
  [ -n "$name" ] || continue
  [ $# -eq 0 ] || [[ "$want" == *" $name "* ]] || continue
  start=$(date +%s)
  (cd "$dir" && eval "$cmd") > "build-logs/$name.log" 2>&1 < /dev/null
  rc=$?
  pkg=$(grep -E 'package +|\.fwsc' "build-logs/$name.log" | tail -1 | cut -c1-150)
  printf '%-12s exit %-3s %4ss  %s\n' "$name" "$rc" "$(( $(date +%s) - start ))" "${pkg:-(see build-logs/$name.log)}"
done
