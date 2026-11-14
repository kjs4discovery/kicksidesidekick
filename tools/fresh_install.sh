#!/bin/bash
# fresh_install.sh -- install a package on the FM-1 and set it up like a new user's (M10.1 Part 6, D068).
# THE USER RUNS THIS, NEVER CLAUDE CODE: it WRITES to the FM-1 (the firmware, then the song store, the saved presets and the settings).
# Licence: TBD (see D017)
#
#   tools/fresh_install.sh PACKAGE.fwsc          e.g. build/step-M10.1/fm1acc-0.10.2-m10.1.fwsc
#
# Each step prints what it does and asks y/n before it writes. The first failure stops the script and prints the fallback.
#   (1) fm1_install.py --info                         read-only
#   (2) fm1_install.py PACKAGE                        WRITES the firmware (the same command as the install docs)
#   (3) wait until the FM-1 is back with the package's identity (fm1_install.py --info)
#   (4) a warning, then y/n: this erases ALL songs, saved presets (U01-U30) and settings
#   (5) fm1song factory-reset --yes                   WRITES: erases the song store, the preset object, resets the settings
#   (6) you switch the FM-1 off and on and choose SONG LEARN
#   (7) fm1song send every entry of songs/library.txt (the songs, then the backtracks)   WRITES the song store
#   (8) fm1song list: every entry of the library must be there and "ok"
#
# Emulator test only (tools/m101_test.py): FM1_FRESH_EMU=1 skips (1)-(3) and (6), answers y by itself and gives every fm1song command
# --emu-pipe (the fm1-live emulator's USB pipe on stdin / stdout). Never set it on a real FM-1.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1
PY="$ROOT/.venv/bin/python"
INSTALL=("$PY" -I "$ROOT/firmware/vendor/fm1-base/bsp/tools/fm1_install.py")
FM1SONG="$ROOT/tools/fm1song.sh"
LIB="$ROOT/songs/library.txt"
EMU="${FM1_FRESH_EMU:-}"
PIPE=()
[ -n "$EMU" ] && PIPE=(--emu-pipe)
PKG="${1:-}"
FALLBACK="Fallback: install the last good firmware again:  $PY -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M10/fm1acc-0.10.0-m10.fwsc   (then tools/fm1song.sh send the songs you want)"
say() { echo "$@" >&2; }

fail() {
    say ""
    say "STOPPED at step $1: $2"
    say "$FALLBACK"
    exit 1
}

ask() {            # ask "question"  -> 0 yes, 1 no
    [ -n "$EMU" ] && { say "$1 [y/n] y (emulator test)"; return 0; }
    local a
    read -r -p "$1 [y/n] " a
    [ "$a" = "y" ] || [ "$a" = "Y" ]
}

entries() {        # the library's paths of a kind ("songs/backtracks/" or the rest), comments and blank lines left out
    grep -v '^[[:space:]]*\(#\|$\)' "$LIB" | { if [ "$1" = bt ]; then grep '^songs/backtracks/'; else grep -v '^songs/backtracks/'; fi; }
}

[ -f "$LIB" ] || fail 0 "songs/library.txt is missing"
NSONGS=$(entries songs | wc -l)
NBT=$(entries bt | wc -l)
TOTAL=$((NSONGS + NBT))
for p in $(entries songs; entries bt); do
    [ -f "$ROOT/$p/song.yaml" ] || fail 0 "$p is not a song folder (songs/library.txt)"
done
say "The library: $NSONGS songs and $NBT backtracks ($TOTAL entries, songs/library.txt)."

if [ -z "$EMU" ]; then
    [ -n "$PKG" ] && [ -f "$PKG" ] || { say "usage: tools/fresh_install.sh PACKAGE.fwsc"; exit 2; }
    say ""
    say "== Step 1/8: read the FM-1's identity (read-only)"
    "${INSTALL[@]}" --info || fail 1 "no FM-1 answered (is it on, USB connected?)"
    WANT=$("$PY" -I -c "
import sys
sys.path.insert(0, '$ROOT/firmware/vendor/fm1-base/bsp/tools')
import fm1_install as I
print(I.product_of(open(sys.argv[1], 'rb').read()))" "$PKG") || fail 1 "$PKG is not a package"
    say ""
    say "== Step 2/8: install $PKG (identity $WANT) -- this WRITES the firmware, 1-2 minutes, do not unplug"
    ask "Install it now?" || { say "Not installed: nothing was written."; exit 0; }
    "${INSTALL[@]}" "$PKG" || fail 2 "the install did not finish"
    say ""
    say "== Step 3/8: wait for the FM-1 to come back as $WANT"
    ok=""
    for _ in $(seq 1 45); do
        if "${INSTALL[@]}" --info 2>/dev/null | grep -q "$WANT"; then ok=1; break; fi
        sleep 2
    done
    [ -n "$ok" ] || fail 3 "the FM-1 did not come back as $WANT within 90 s"
    say "The FM-1 runs $WANT."
fi

say ""
say "== Step 4/8: the clean start"
say "   THIS ERASES ALL SONGS, ALL SAVED PRESETS (U01-U30) AND ALL SETTINGS on the FM-1."
say "   (Presets you want to keep: write their values down first; there is no read-out.)"
ask "Erase them now?" || { say "Nothing erased. The new firmware is installed; the songs and presets are as they were."; exit 0; }

say ""
say "== Step 5/8: factory reset (WRITES: song store, preset object, settings)"
"$FM1SONG" factory-reset --yes "${PIPE[@]}" || fail 5 "the factory reset did not finish (the unit still boots; run this script again)"

say ""
if [ -z "$EMU" ]; then
    say "== Step 6/8: switch the FM-1 OFF and ON, then choose SONG LEARN on the mode screen."
    read -r -p "Press Enter when SONG LEARN is chosen: " _
else
    say "== Step 6/8: (emulator: no power cycle)"
fi

say ""
say "== Step 7/8: send the library ($TOTAL entries; WRITES the song store)"
ask "Send the songs and backtracks now?" || { say "Not sent. The FM-1 is clean (no songs); run this script again or send by hand."; exit 0; }
# shellcheck disable=SC2046
"$FM1SONG" send "${PIPE[@]}" $(entries songs) || fail 7 "sending the songs failed"
# shellcheck disable=SC2046
"$FM1SONG" send "${PIPE[@]}" $(entries bt) || fail 7 "sending the backtracks failed"

say ""
say "== Step 8/8: check (read-only): fm1song list"
TMP=$(mktemp)
if [ -n "$EMU" ]; then            # (the emulator's pipe is stdout: only stderr is captured)
    "$FM1SONG" list "${PIPE[@]}" 2>"$TMP" || { cat "$TMP" >&2; rm -f "$TMP"; fail 8 "fm1song list failed"; }
else
    "$FM1SONG" list >"$TMP" 2>&1 || { cat "$TMP" >&2; rm -f "$TMP"; fail 8 "fm1song list failed"; }
fi
OUT=$(cat "$TMP")
rm -f "$TMP"
echo "$OUT" >&2
GOOD=$(echo "$OUT" | grep -c '^  [^ ].* ok$')
if [ "$GOOD" -ne "$TOTAL" ] || echo "$OUT" | grep -q 'UNREADABLE\|DAMAGED'; then
    fail 8 "the list shows $GOOD ok entries, songs/library.txt has $TOTAL"
fi
say ""
say "DONE: $GOOD of $TOTAL entries are on the FM-1 and ok. Switch it off and on once more; settings and presets are the defaults."
