# Step 0.3 — Flash chip ID and full flash backup (procedure)

Prepared by Claude Code on 2026-11-04. **The user runs every device command.**
Nothing here writes to the FM-1's flash. Tools reviewed:

| Tool | Commit | Used for |
|------|--------|----------|
| `third_party/MvaveFM1Unbricker/fm1_unbrick.py` 1.2.1 | 9b68a82 | `softkey`, `find`, `info`, `backup` (main route) |
| `…/MvaveFM1Unbricker/jl-uboot-tool` (kagaimiq) | adb3f18 (pinned by the unbricker) | the USB protocol under `info`/`backup`; `wl82loader.bin` sha256 d41da612…5376 (matches the pin) |
| `third_party/m-flasher` 0.8.2 (built from source) | 55da95c | optional cross-check backup with an independent loader |

## What each step sends to the unit

| Step | Command | Sent to the FM-1 | Flash written? |
|------|---------|------------------|----------------|
| a. into ROM boot | `fm1_unbrick.py softkey` | MIDI: identity query `F0 00 32 45 00 00 00 40 7F F7`, then the key `F0 22 24 35 7D F7` (up to 3×). Stock V15 jumps into the chip's mask-ROM download mode (`WL82 UBOOT1.00`, USB `4c4a:8057`). | No |
| b. chip key + flash ID | `fm1_unbrick.py info` | SCSI INQUIRY; ROM `READ_MEMORY 0xFD07` (512 B of RAM at 0x1C7C000); ROM `WRITE_MEMORY 0xFB06` + `JUMP 0xFB08` = uploads jl-uboot-tool's loader **into RAM**; loader `READ_KEY 0xFC09`, `GET_ONLINE_DEVICE 0xFC0A`, `GET_USB_BUFF_SIZE 0xFC14` | No (RAM only, gone at power-off) |
| c. full flash ×2 | `fm1_unbrick.py backup DIR` | as b, then loader `READ_FLASH 0xFD05` over 0x0–0xFFFFF, twice, compared in the tool | No |
| d. back to normal | unplug, power off, 5 s, power on | nothing | No |

Flash-changing loader commands are `ERASE_FLASH_BLOCK 0xFB00`, `ERASE_FLASH_SECTOR 0xFB01`,
`ERASE_FLASH_CHIP 0xFB02`, `WRITE_FLASH 0xFB04` and `WRITE_KEY 0xFC12`. In `fm1_unbrick.py`
they are reached only from `restore` (`write_and_verify`, line 1345) and `repair-settings`
(`repair_sectors`, line 1586). **Never run `restore` or `repair-settings`.**

What cannot be checked from source: the firmware's handling of the soft key (we only see
what the host sends), and the two RAM loaders, which are binary blobs (JieLi's
`wl82loader.bin`; M-Flasher's `mflr-loader-a.bin`, sha256 ae0cdf49…04d7). Both are used by
the community for exactly these reads.

## If the flash ID is 0xB36014 instead of 0x856014

| Tool | Behaviour with 0xB36014 | With an unknown ID |
|------|-------------------------|--------------------|
| fm1_install.py `--info` | does not look at the flash (MIDI identity only) | same |
| fm1_unbrick.py | `FLASH_IDS_OK = (0x856014, 0xB36014)`: `info`/`backup` proceed | refuses: "this does not look like an FM-1 … nothing was written" (unless `--i-know`: never use it) |
| jl-uboot-tool | no ID check of its own; the loader blob talks to the chip | — |
| M-Flasher | `FM1.other_flash = [0xB36014]` ("UCUN UC25HQ80, in units sold from late 2026"): read as an FM-1 hint. If its loader cannot size the flash from the ID, every read is refused and it stops: "Nothing was written". Installing m-bootloader on 0xB36014 warns that such a unit did not start afterwards (a write: not part of this step) | model hint missing; reads still work if the loader sized the flash |

**Project rule (HANDOFF 2026-11-04 15:02):** if `info` reports **0xB36014**, record it and
**stop before the backup**. The planning chat decides whether the backup goes ahead. Both
tools list 0xB36014 explicitly, which should make that decision quick.

## One-time preparation (done by the user)

Already done by Claude Code: jl-uboot-tool cloned at the pinned commit (no `setup` needed),
its Python requirements (crcmod, pyyaml, pycryptodomex, tqdm) installed in `.venv`,
`backups/flash/` created, `tools/verify_backup.sh` written and tested on fake dumps.
The `sg` driver is built into this kernel (no `modprobe` needed).

**This laptop's other SCSI devices: `/dev/sg0` = internal SSD (APPLE SSD SM0512),
`/dev/sg1` = SD card reader. Never pass `--device` at all.** The tools pick the one device
reporting `WL82 UBOOT1.00`.

Access to the download-mode device without sudo (recommended, so no third-party script runs
as root). This udev rule from M-Flasher only matches USB `4c4a:8057` (the FM-1 in ROM
boot mode) and gives the logged-in user access to it:

```bash
sudo cp third_party/m-flasher/linux/70-m-flasher.rules /etc/udev/rules.d/ && sudo udevadm control --reload
```

Undo: `sudo rm /etc/udev/rules.d/70-m-flasher.rules && sudo udevadm control --reload`.
(Without it, steps 5–8 need `sudo` in front, and their output files become root-owned.)

## Checklist

Run everything from `~/dev/fm1-accompaniment`. FM-1 on, plugged in **directly** (no hub)
with the data cable. Close anything else using its MIDI port.

**General rule: if any command asks you to type `WRITE`, `REPAIR` or `yes`, you are running
the wrong command. Press Ctrl+C. Never add `--i-know`, `--force-write`, `--yes` or `--device`.**

1. **Unit is running stock V15.**
   ```bash
   lsusb | grep -i 4c4a
   ```
   ```bash
   .venv/bin/python -I third_party/fm1-linux-update/.cache/fm1_install.py --info --port FM-1
   ```
   Expect `ID 4c4a:c755  FM-1` and `FM-1_015  [running]`.
   Anything else → stop.

2. **No device is in download mode yet.**
   ```bash
   .venv/bin/python -I third_party/MvaveFM1Unbricker/fm1_unbrick.py find --log-dir backups/flash/logs
   ```
   Expect `No WL82 UBOOT1.00 device found.` (then an error line saying the same: fine).

3. **(a) Into ROM boot mode.**
   ```bash
   .venv/bin/python -I third_party/MvaveFM1Unbricker/fm1_unbrick.py softkey --port FM-1 --log-dir backups/flash/logs
   ```
   Expect `Identity    FM-1_015 = M-VAVE stock firmware V15`, then a prompt: type `UBOOT`.
   Then `Key sent    F0 22 24 35 7D F7`, `Found       /dev/sgN  WL82 UBOOT1.00 …` and
   `The FM-1 is in ROM boot mode`. The screen may go dark or freeze; that is expected.
   If the desktop shows a disk/"unable to mount"/"format" dialog: **Cancel / ignore. Never format.**
   - "does not answer the identity query" → stop (do not use `--i-know`). Power-cycle.
   - "No WL82 UBOOT1.00 disk appeared within 15 s" → **seen on this laptop**: the first USB
     descriptor read times out (`error -110`) and the disk appears ~17 s after the key, just
     past the tool's wait. Wait 10 s more, then run `lsusb | grep -i 4c4a`.
     `4c4a:8057` = it is in ROM boot: continue with step 4. `4c4a:c755` = it ignored the key:
     stop and report. Nothing at all → step 9 (power cycle) and report.

4. **Confirm the ROM boot device.**
   ```bash
   lsusb | grep -i 4c4a
   ```
   ```bash
   .venv/bin/python -I third_party/MvaveFM1Unbricker/fm1_unbrick.py find --log-dir backups/flash/logs
   ```
   Expect `4c4a:8057` and exactly one `/dev/sgN  WL82 UBOOT1.00 (sgN, USB)` (N ≥ 2), then
   `Exactly one device: OK`. Two devices or a refusal → step 9, stop.
   "permission denied" in later steps → the udev rule is missing (see preparation).

5. **(b) Chip key and flash ID.**
   ```bash
   .venv/bin/python -I third_party/MvaveFM1Unbricker/fm1_unbrick.py info --log-dir backups/flash/logs
   ```
   Expect `Device      /dev/sgN: WL82 UBOOT1.00`, `USB loader uploaded and running`,
   `Chip key    0x980F`, `Flash ID    0x856014` or `0xB36014`, `This is an FM-1 in UBOOT mode: OK`.
   It also saves 512 bytes of RAM as `backups/fm1-recovery-*/retained-ram.bin` (diagnostic).
   - **Flash ID 0xB36014 → stop here.** Do step 9, paste the output. (Project rule.)
   - Any refusal ("does not look like an FM-1") → step 9, paste the output, stop.

6. **(c) Full backup, run 1** (the tool reads the whole 1 MiB twice and compares).
   ```bash
   .venv/bin/python -I third_party/MvaveFM1Unbricker/fm1_unbrick.py backup backups/flash --package backups/stock-firmware/FM-1_015.fwsc --log-dir backups/flash/logs
   ```
   Expect `read 1 sha256 X`, `read 2 sha256 X` (same X), `Backup OK   backups/flash/fm1-recovery-…/full-flash-before.bin`,
   and an analysis. For a stock V15 unit: `firmware region = official M-VAVE V15 (exact)`,
   `firmware sectors identical 143/143` and `boot head matches the official V15 package's head`.
   (Output format checked with `--dry-run` against the tool's mock FM-1 on 2026-11-04.)
   A different head or fewer than 143/143 is not an error for a backup; just paste it.
   - "the two backup reads DIFFER" → nothing was written. Step 9, try another cable/port, start again.
   - **Measured 2026-11-04: `backup` right after `info` in the same ROM boot session fails.**
     Re-uploading the loader over the one `info` started gives `OSError: [Errno 19] No such
     device` in `mem_write` (RAM only, no flash command reached); the unit then reset by itself
     and booted stock V15 normally. So: **only one loader-uploading command (`info` or `backup`)
     per ROM boot session.** Do step 9, then steps 3–4 again, then this step. `backup` checks
     the chip key and flash ID itself, so `info` is only needed the first time.

7. **(c) Full backup, run 2** (a second, independent session folder).
   First a fresh ROM boot session: step 9, then steps 3–4. Then the same command as step 6.
   Expect the same sha256 as run 1.

8. **Compare the two runs.**
   ```bash
   ls backups/flash/
   ```
   ```bash
   tools/verify_backup.sh backups/flash/fm1-recovery-<RUN1>/full-flash-before.bin backups/flash/fm1-recovery-<RUN2>/full-flash-before.bin
   ```
   Expect `identical`, `= official V15` and `OK: two identical 1 MiB dumps`.
   Anything else → keep the files, paste the output.

9. **(d) Back to normal.** Unplug the USB cable. Switch the FM-1 off. Wait 5 seconds.
   Switch it on. Plug USB back in. Then:
   ```bash
   lsusb | grep -i 4c4a
   ```
   ```bash
   .venv/bin/python -I third_party/fm1-linux-update/.cache/fm1_install.py --info --port FM-1
   ```
   Expect `4c4a:c755` and `FM-1_015  [running]`; the unit plays normally.
   If the screen stays black: switch off, wait, switch on once more. If it still does not
   start, leave it **off**, do not run any other tool (especially not `restore`), and report.
   The ROM boot mode is in mask ROM and cannot be damaged by these reads.

10. **Paste** the output of steps 5, 6, 7, 8 and 9 into the Claude Code chat. Claude Code then
    records the chip ID and the backup sha256 in docs/device.md and backups/SHA256SUMS.

### Optional cross-check with M-Flasher (independent loader)

Only after step 9, in a fresh ROM boot session (steps 3–4 again; never mix both tools in
one session: M-Flasher refuses to start when another loader is running). M-Flasher reads
the flash once and checks it against a CRC32 the chip computes. It also shows "M-FLASHER
CONNECTED" on the unit's screen (RAM/display only).

```bash
third_party/m-flasher/target/release/m-flasher info
```
```bash
third_party/m-flasher/target/release/m-flasher backup backups/flash/m-flasher
```
Expect `flash 0x856014` (or 0xB36014), `Checked: the chip's CRC32 of the flash matches the read`,
`Backup saved: backups/flash/m-flasher/fm1-<date>.bin`. Then step 9. Its sha256 should
equal the unbricker's. Do not run `m-flasher restore`, `firmware`, `bootloader` or `run-ram`.
