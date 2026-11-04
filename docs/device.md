# FM-1 device record

All of these must be filled in before any write to the unit is even proposed
(see CLAUDE.md, hardware safety rules). Note for each value how and when it was
measured.

## Stock firmware version

- Version: **FM-1_015** (stock V15), running mode (not the update loader)
- Raw output: `FM-1_015  [running]  port: FM-1:FM-1 MIDI 1 24:0`
- How read / date: `.venv/bin/python -I third_party/fm1-linux-update/.cache/fm1_install.py --info --port FM-1`
  (Felucca fm1_install.py at commit 7c391863, sha256 31c60266…26ee; sends only
  the identity query F0 00 32 45 00 00 00 40 7F F7), 2026-11-04 14:52
- Matching stock package on disk: `backups/stock-firmware/FM-1_015.fwsc` (see below)

## USB ID

- VID:PID: 4c4a:c755
- `lsusb` line: `Bus 001 Device 004: ID 4c4a:c755  FM-1`
- Descriptors (`lsusb -v -d 4c4a:c755`): iProduct "FM-1", iManufacturer none,
  iSerial `<redacted>`, bcdDevice 1.00. Interfaces: USB Audio (control +
  4 streaming) and one MIDI Streaming interface.
- Hint: fm1-linux-update's README says the product name reads "Jieli Technology
  USB Composite Device" before V15 and "FM-1" from V15 on, so this unit is
  probably V15 or later. Not a measurement of the version.
- ALSA: seq client 24 "FM-1", port 0 "FM-1 MIDI 1" (card 2); rawmidi `hw:2,0,0`
  "FM-1 MIDI 1"; mido port name `FM-1:FM-1 MIDI 1 24:0` (in and out). Client
  and card numbers can change between boots.
- How read / date: `lsusb`, `lsusb -v`, `aconnect -l`, `amidi -l`, 2026-11-04 14:46
  (unit on, running normally, not in the update loader)

## Flash chip ID

- JEDEC ID: _TBD_ (expected 0x856014 Puya or 0xB36014 UCUN UC25HQ80)
- How read / date: _TBD_

## Flash backups

Backup files live in `backups/` (git-ignored). Their checksums are tracked in
`backups/SHA256SUMS`.

| File | Size (bytes) | sha256 | Made with | Date | Other copies |
|------|--------------|--------|-----------|------|--------------|
|      |              |        |           |      |              |

## Stock firmware packages (safekeeping only, never sent anywhere)

These are M-VAVE's own update packages, not backups of this unit. They live in
`backups/stock-firmware/` (git-ignored) and are listed in `backups/SHA256SUMS`.

| File | Embedded name | Size (bytes) | sha256 | Source | Verified by |
|------|---------------|--------------|--------|--------|-------------|
| FM-1_015.fwsc | FM-1_015 | 699,956 | `db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a` | Wayback copy `web.archive.org/web/20261009203224id_/` of the M-VAVE URL below | sha256 equals the value pinned in fm1-linux-update (commit fa85094) |
| FM-1_016.fwsc | FM-1_016 | 718,196 | `d017fb65e70aca97ee29d944470c3515a74581df5c0576bf70de917c7b2b61d5` | `https://yms-file-store.oss-cn-hongkong.aliyuncs.com/software/firmware/FM-1.fwsc` (Last-Modified 2026-10-21 10:37:14 GMT; headers saved in `FM-1_016.headers.txt`) | MD5 equals server Content-MD5 `ohNo4HC1NM2r/mn0PVAj+g==` |

Downloaded 2026-11-04. The M-VAVE URL always serves the latest package; it
switched from V15 to V16 between 2026-10-18 and 2026-10-22 (Wayback CDX).
There is no V14 package yet; it is only needed if `--info` reports FM-1_014.
