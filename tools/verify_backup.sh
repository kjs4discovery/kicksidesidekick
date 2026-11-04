#!/usr/bin/env bash
# verify_backup.sh -- compare two full-flash dumps of the FM-1 and print their sha256.
#
#   tools/verify_backup.sh DUMP_A.bin DUMP_B.bin
#
# Reads the two files only; never talks to the device. Exit 0 if the dumps are
# identical and 1 MiB, 1 if they differ or look wrong, 2 on bad arguments.
set -euo pipefail

FLASH_SIZE=$((0x100000))            # 1 MiB SPI NOR
FW_START=$((0x4000)) FW_LEN=$((0x8F000))
# sha256 of 0x4000..0x92FFF in official V15 (from MvaveFM1Unbricker, V15_FLASH_FW_SHA256)
V15_FW_SHA256=6edf3c37fb5bbbc33607c89375ee024d5477c17914d72221c8c68e58a8255686

if [ $# -ne 2 ]; then
  sed -n '2,7p' "$0" >&2
  exit 2
fi
a=$1 b=$2
for f in "$a" "$b"; do
  [ -f "$f" ] || { echo "error: $f not found" >&2; exit 2; }
done

region_sha() {  # file start length
  dd if="$1" iflag=skip_bytes,count_bytes skip="$2" count="$3" status=none | sha256sum | cut -d' ' -f1
}

ok=1
echo "== files"
for f in "$a" "$b"; do
  size=$(stat -c %s "$f")
  printf '  %s  %8d bytes  %s\n' "$(sha256sum "$f" | cut -d' ' -f1)" "$size" "$f"
  if [ "$size" -ne "$FLASH_SIZE" ]; then
    echo "  !! $f is not $FLASH_SIZE bytes (1 MiB): not a full dump"
    ok=0
  fi
done

echo "== compare"
if cmp -s "$a" "$b"; then
  echo "  identical"
else
  ndiff=$( (cmp -l "$a" "$b" 2>/dev/null || true) | wc -l)
  first=$( (cmp "$a" "$b" 2>/dev/null || true) | sed -n 's/.* byte \([0-9]*\).*/\1/p')
  printf '  DIFFERENT: %d bytes differ, first at offset 0x%X\n' "$ndiff" "$((${first:-1} - 1))"
  echo "  -> the USB link is unreliable or the flash changed between reads. Do not trust either dump."
  ok=0
fi

echo "== content (of $a; informational)"
[ "$(stat -c %s "$a")" -eq "$FLASH_SIZE" ] && {
  printf '  boot head 0x0000..0x3FFF sha256   %s\n' "$(region_sha "$a" 0 "$FW_START")"
  fw=$(region_sha "$a" "$FW_START" "$FW_LEN")
  printf '  firmware  0x4000..0x92FFF sha256  %s' "$fw"
  if [ "$fw" = "$V15_FW_SHA256" ]; then echo "  = official V15"; else echo "  (not the official V15 region)"; fi
  used=$(tr -d '\377' < "$a" | wc -c)
  printf '  non-0xFF bytes in the whole flash: %d of %d\n' "$used" "$FLASH_SIZE"
}

echo "== result"
if [ "$ok" -eq 1 ]; then
  echo "  OK: two identical 1 MiB dumps. Line for backups/SHA256SUMS:"
  echo "  $(sha256sum "$a" | cut -d' ' -f1)  $a"
  exit 0
fi
echo "  NOT OK: see above. Nothing has been written to the FM-1 by this script."
exit 1
