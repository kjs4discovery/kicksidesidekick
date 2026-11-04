#!/usr/bin/env bash
# apply_patches.sh -- apply our local patches to the third-party checkouts in third_party/ and rebuild what needs it.
#   tools/apply_patches.sh
# fm1-emu: tools/patches/fm1-emu-jedec-id.patch (FM1_FLASH_ID=b36014 sets the emulated flash chip's JEDEC id).
# Safe to run again: a patch already applied is skipped.
set -eu
cd "$(dirname "$0")/.."
emu=third_party/fm1-emu
p=tools/patches/fm1-emu-jedec-id.patch
if git -C "$emu" apply --reverse --check "../../$p" 2>/dev/null; then
  echo "fm1-emu: $p already applied"
else
  git -C "$emu" apply "../../$p"
  echo "fm1-emu: applied $p"
fi
(cd "$emu" && "${CARGO:-$HOME/.cargo/bin/cargo}" build --release -q -p fm1-live -p fm1-emu)
echo "fm1-emu: built ($emu/target/release/fm1-live)"
