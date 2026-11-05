#!/bin/sh
# fm1song: the song compiler's command line (tools/fm1song/cli.py), with the repo's venv.
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PYTHONPATH="$ROOT/tools${PYTHONPATH:+:$PYTHONPATH}" exec "$ROOT/.venv/bin/python" -m fm1song "$@"
