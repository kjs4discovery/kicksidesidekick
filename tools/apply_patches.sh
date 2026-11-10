#!/usr/bin/env bash
# apply_patches.sh -- apply our local patches to the third-party checkouts in third_party/ and rebuild what needs it.
#   tools/apply_patches.sh
# fm1-emu: tools/patches/fm1-emu-jedec-id.patch (FM1_FLASH_ID=b36014 sets the emulated flash chip's JEDEC id),
#         tools/patches/fm1-emu-usb-host.patch (FM1_USB_HOST='CMD': a program plays the USB host in --bench runs),
#         tools/patches/fm1-emu-flash-timing.patch (M5.1: FM1_FLASH_TIMED=1 or "ERASE_US,PROG_US": erases and page
#         programs take their time (default 8000 / 2000 us), data-area XIP reads cost cache misses; every run prints
#         the watchdog's longest unfed stretch; a read of the XIP window while a flash command has it off stalls the
#         core until the watchdog resets it, as on the chip).
# Safe to run again: a patch already applied is skipped.
set -eu
cd "$(dirname "$0")/.."
emu=third_party/fm1-emu
for p in tools/patches/fm1-emu-jedec-id.patch tools/patches/fm1-emu-usb-host.patch tools/patches/fm1-emu-flash-timing.patch; do
  if git -C "$emu" apply --reverse --check "../../$p" 2>/dev/null; then
    echo "fm1-emu: $p already applied"
  else
    git -C "$emu" apply "../../$p"
    echo "fm1-emu: applied $p"
  fi
done
(cd "$emu" && "${CARGO:-$HOME/.cargo/bin/cargo}" build --release -q -p fm1-live -p fm1-emu)
echo "fm1-emu: built ($emu/target/release/fm1-live)"
