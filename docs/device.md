# FM-1 device record

All of these must be filled in before any write to the unit is even proposed
(see CLAUDE.md, hardware safety rules). Note for each value how and when it was
measured.

## Stock firmware version

- Version: _TBD_
- How read / date: _TBD_

## USB ID

- VID:PID: 4c4a:c755
- `lsusb` line: `Bus 001 Device 004: ID 4c4a:c755  FM-1`
- How read / date: `lsusb | grep -i 4c4a`, 2026-11-04 (normal MIDI mode, not bootloader)

## Flash chip ID

- JEDEC ID: _TBD_ (expected 0x856014 Puya or 0xB36014 UCUN UC25HQ80)
- How read / date: _TBD_

## Flash backups

Backup files live in `backups/` (git-ignored). Their checksums are tracked in
`backups/SHA256SUMS`.

| File | Size (bytes) | sha256 | Made with | Date | Other copies |
|------|--------------|--------|-----------|------|--------------|
|      |              |        |           |      |              |
