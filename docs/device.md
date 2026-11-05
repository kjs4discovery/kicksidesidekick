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

- JEDEC ID: **0x856014** (Puya, 1 MiB SPI NOR), device type 0x03 (SPI0 NOR)
- Chip key: **0x980F**
- Raw output: `Chip key    0x980F (expected 0x980F)` /
  `Flash ID    0x856014 (expected 0x856014 or 0xB36014, 1 MiB), type 0x03` /
  `USB buffer  32768 bytes` / `This is an FM-1 in UBOOT mode: OK`
- How read / date: `fm1_unbrick.py info` (MvaveFM1Unbricker 9b68a82, jl-uboot-tool adb3f18,
  wl82loader.bin loaded at 0x1C02000), run by the user, 2026-11-04 15:21, unit in ROM boot
  mode (`/dev/sg2`, USB `4c4a:8057`, descriptor strings "WL80UBOOT1.00", SCSI "WL82 UBOOT1.00")
  entered with `fm1_unbrick.py softkey`. Log: `backups/flash/logs/unbrick-20261104-152122.log`.
- ROM boot entry note: after the soft key the first USB descriptor read timed out
  (`error -110`); the device enumerated ~17 s later, just past softkey's 15 s wait.
- Retained RAM (diagnostic, 512 B at 0x1C7C000): `backups/fm1-recovery-20261104-152122/retained-ram.bin`,
  sha256 `cd1e8568463a88152aba5911bac28555f1c0cba02b014e8c7a54a36cd7fc9163`, no Melodee crash record.

## Flash backups

Backup files live in `backups/` (git-ignored). Their checksums are tracked in
`backups/SHA256SUMS`. Full-flash dumps go to `backups/flash/` (procedure:
docs/step-0.3-flash-backup.md; compare with `tools/verify_backup.sh`).
Keep three copies: the repo's `backups/flash/`, another disk, and cloud storage.

| File | Size (bytes) | sha256 | Made with | Date | Other copies |
|------|--------------|--------|-----------|------|--------------|
| `backups/flash/FM-1_015_full-flash_856014_2026-11-04.bin` (copy of run 1) | 1,048,576 | `794d72f01c293dc5d1c69ee4e54e4db956112bfe8734d0449fe4ea36e92f43f4` | see below | 2026-11-04 | another disk: **USB thumb drive** (the user's, kept unplugged; the user reports it checked, 2026-11-04; sha256 not re-read by Claude Code); cloud: **none** |
| `backups/flash/fm1-recovery-20261104-152541/full-flash-before.bin` (run 1) | 1,048,576 | same | fm1_unbrick.py `backup` | 2026-11-04 15:25 | — |
| `backups/flash/fm1-recovery-20261104-153009/full-flash-before.bin` (run 2) | 1,048,576 | same | fm1_unbrick.py `backup` | 2026-11-04 15:30 | — |

How made: `fm1_unbrick.py backup backups/flash --package backups/stock-firmware/FM-1_015.fwsc`
(MvaveFM1Unbricker 9b68a82, jl-uboot-tool adb3f18), run by the user in two separate ROM boot
sessions (soft key → backup → power cycle). Each run reads the whole flash twice and compares:
4 full reads, all sha256 `794d72f0…2f43f4`. `tools/verify_backup.sh` on runs 1 and 2: identical.
Unit: stock FM-1_015, chip key 0x980F, flash 0x856014.

What the dump contains (tool analysis + offline comparison, 2026-11-04):
- Boot head 0x0000–0x3FFF sha256 `d9f43191777656de01c7bed4f9e9ba2e34e94832c5d2ed02c3fc7ab7a6d9bd67`
  = the official V15 package's head (same constant as fm1-linux-update's README).
- Firmware 0x4000–0x92FFF sha256 `8c1d13a5934eb3c70a5fec644577618a31b2a96c8748a7b692c506a86505dbb1`.
  **Not** the official V15 package region (`6edf3c37…`): 0/143 sectors identical, 92.8 % of
  bytes differ, no constant XOR pattern (17,717 distinct 32-byte patterns), a few early bytes
  equal. Cause unknown (per-unit encryption? factory image vs update package?). Consequence:
  writing the package's region would not reproduce this unit's flash; this dump is the
  bit-exact way back.
- 64 KiB blocks with data: 0x00000–0x9FFFF and 0xE0000–0xFFFFF. 638,285 of 1,048,576 bytes are not 0xFF.

## Restore options (D015, accepted 2026-11-04)

1. **Normal return to stock:** install `backups/stock-firmware/FM-1_015.fwsc` through the
   normal update path (M-VAVE's two-step USB-MIDI update). The update loader handles the
   per-chip encryption of the firmware region.
2. **Last resort, bit-exact:** write the full dump
   `backups/flash/FM-1_015_full-flash_856014_2026-11-04.bin` through ROM boot mode. This
   also brings back calibration, the Bluetooth address and the settings exactly.

Keep both. Why the dump's firmware region (0x4000–0x92FFF) differs from the package's: most
likely the firmware is stored encrypted with the chip key (m-flasher decrypts the flash with
chip key 0x980F); not yet confirmed by decrypting our dump.

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

## Hardware model (from the fm1-emu emulator)

Source: `third_party/fm1-emu` at 9de1ef0 (v0.4.0, 2026-11-02): `crates/fm1-soc/src/{lib,lcd,spiflash,periph,script}.rs`,
`crates/fm1-live/src/main.rs`, `docs/recon.md`. These are the emulator's model of the board,
built from community reverse engineering and Felucca's HAL; not measured on our unit.

- **SoC:** JieLi AC791N (WL82 family), two pi32v2 cores, 240 MHz (`CPU_HZ`).
- **Display:** ST7789-class, **240 × 240**, **RGB565** (16-bit) framebuffer, on SPI1
  (PC9 CLK, PC10 DO, PC7 CS, PC8 D/C, backlight PA2). A full frame takes ~77 ms at 12 MHz SPI.
- **Keys:** **27 note keys**, F3–G5 (MIDI 53–79; matrix ids 14–40, names `C4`, `F#3`, `n0`..`n26`).
- **Buttons (14, with LEDs):** OCTDN, OCTUP, FX, SCL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP,
  SEQ, PLAY, REC (matrix ids 0–13).
- **Encoders (7):** SELECT (0), ALGO (1), K1–K4 (2–5), PRESET (6).
- **Pot:** MASTER volume (SARADC channel 4, 0–1023).
- **Velocity / pressure:** the keys report how hard they land (script `ID@VEL`, `ID~VEL`).
- **Audio:** I2S DAC, **44.1 kHz stereo**, 24-bit samples in 32-bit words (Felucca `audio.c`
  DAC_TICKS). Speaker plus headphone jack on the same line (the jack mutes the speaker).
- **MIDI:** TRS MIDI IN on UART1 (MIDI out probably not wired); USB-C (USB-MIDI and USB audio
  depending on the firmware); BLE unused by most firmwares. In the emulator M-VAVE's firmware
  listens on MIDI channel 2; Felucca plays its parts on channels 1–4.
- **Memory map:** XIP flash (code) at `0x02000000`; data RAM at `0x01C00000` (recon: "≥512 KiB";
  the emulator gives 1 MiB); SRAM banks at `0x01F00000`, 256 KiB (cleared by early init); boot
  state at `0x01C7FD80`.
- **Flash:** 1 MiB SPI NOR on SPI0 (CS PD0). The emulator's flash answers JEDEC `85 60 14`
  (our chip) and has no option to report `B3 60 14` (only the SMK-37 board switches it, to
  `EB 60 14`). Testing our 0xB36014 path will need a small emulator patch or option.
