# LOG

Written by Claude Code, append-only, newest entry at the bottom. The planning
chat reads this. One entry after every step and before ending a session, in
this format:

```
YYYY-MM-DD HH:MM — Step X.Y —
Status: done | partial | blocked
Did: bullets
Learned / measured: bullets (versions, IDs, numbers, file paths)
Hardware touched: none | read-only | WRITE (what, by whom)
Questions for the planning chat: bullets, or "none"
Next: the next step as you understand it
```

---

2026-11-04 14:32 — Step 0.1 — Repo setup
Status: done
Did:
- Created repo ~/dev/fm1-accompaniment (git init, branch main, nothing committed yet).
- Wrote CLAUDE.md (project summary + hardware safety rules, project rules, handoff rules).
- Created docs/device.md (template), docs/HANDOFF.md (placeholder; it did not exist), docs/LOG.md, and the folders third_party/, tools/, songs/, firmware/, backups/.
- .gitignore ignores backups/* except backups/SHA256SUMS (empty file, tracked).
- Created .venv (Python 3.12.3), installed mido 1.3.3 and python-rtmidi 1.5.8 (ALSA + JACK backends).
Learned / measured:
- Host: Linux Mint 22.3.
- FM-1 USB ID 4c4a:c755, product string "FM-1" (lsusb: Bus 001 Device 004). Recorded in docs/device.md.
- FM-1 shows up as ALSA seq client 24 "FM-1", port 0 "FM-1 MIDI 1", card 2; mido sees it as 'FM-1:FM-1 MIDI 1 24:0' (in and out).
- Toolchain: git 2.43.0, gcc/g++ 13.3.0, make 4.3, build-essential 12.10ubuntu1, pkg-config, libasound2-dev, python3-dev, python3-venv are installed. Rust (rustup/cargo/rustc) is NOT installed.
- User kjs is in the audio group in /etc/group, but the current login session doesn't have it yet (needs a re-login). The ALSA sequencer works anyway: /dev/snd/seq is root:audio 0660 with an ACL (+), and aconnect -l succeeds.
Hardware touched: read-only (lsusb, aconnect -l, port enumeration via mido)
Questions for the planning chat:
- Rust is not installed. Is Rust the planned language for the firmware and/or the fm1-emu emulator? If so, which targets (e.g. a thumbv* embedded target) should be added?
- Stock firmware version and flash chip ID are still unknown. Which read-only tool/step should read them?
Next: Wait for docs/HANDOFF.md; likely next is reading the stock firmware version and flash chip ID (read-only) and making a full flash backup.

2026-11-04 14:37 — Step 0.1 (addendum) — Rust installed
Status: done
Did:
- User installed Rust via rustup; verified.
Learned / measured:
- rustup 1.29.3, cargo 1.99.1, rustc 1.99.1, default toolchain stable-x86_64-unknown-linux-gnu. Binaries are in ~/.cargo/bin; no embedded targets added yet.
Hardware touched: none
Questions for the planning chat:
- Same as above: which embedded target(s), if any, should rustup add?
Next: Wait for docs/HANDOFF.md.

2026-11-04 14:40 — Step 0.1 (addendum) — audio group active
Status: done
Did:
- User logged back in; `id -nG` now includes audio.
Learned / measured:
- The audio group is now active in the user's session, so the re-login note in the first entry is resolved.
Hardware touched: none
Questions for the planning chat: none
Next: Wait for docs/HANDOFF.md.

2026-11-04 14:48 — Step 0.2 — Identify the unit (read-only)
Status: partial (identity query not run yet: blocked by Claude Code's auto-mode safety check on downloaded code; handed to the user)
Did:
- Recorded USB and ALSA details of the FM-1 (lsusb, lsusb -v, aconnect -l, amidi -l) in docs/device.md.
- Cloned https://github.com/fuleo/fm1-linux-update into third_party/ (HEAD fa850941dbd69f9172ed78dea5a1d01178a5fa2d, 2026-10-03). Read README.md and fm1-update.sh.
- Found that `fm1-update.sh info` just runs `.cache/fm1_install.py --info`, which `setup` downloads (setup also downloads firmware, so it was NOT run). Downloaded only that script from Felucca at commit 7c391863f26f235116744df845e5f4e01a8ae9c2, sha256 31c602665a5b6b86cd9da17797c80613f5a645261f8b3be020955e34cb0226ee (matches the pin), to third_party/fm1-linux-update/.cache/fm1_install.py.
- Read the whole --info path: it sends only HS_QUERY (F0 00 32 45 00 00 00 40 7F F7), at most twice per port, parses the identity reply, and returns before any package or write code. UPGRADE (F0 22 24 35 7F F7) and data replies are only reachable from install(). Probes only ports matching fm-1|felucca|ota|composite|sinco|usb-midi.
- Tried to run `.venv/bin/python -I third_party/fm1-linux-update/.cache/fm1_install.py --info --port FM-1`; the session's auto-mode check refused to run downloaded code. Nothing was sent to the FM-1.
- Downloaded stock packages for safekeeping into backups/stock-firmware/ (git-ignored) and recorded sha256 in docs/device.md and backups/SHA256SUMS.
Learned / measured:
- USB: 4c4a:c755, iProduct "FM-1", serial <redacted>, bcdDevice 1.00. ALSA seq 24:0 "FM-1 MIDI 1", rawmidi hw:2,0,0, mido 'FM-1:FM-1 MIDI 1 24:0'.
- The product name "FM-1" (not "Jieli Technology USB Composite Device") suggests V15 or later, per the fm1-linux-update README. Unconfirmed.
- M-VAVE's FM-1.fwsc URL now serves FM-1_016 (718,196 B, sha256 d017fb65…2b61d5, Last-Modified 2026-10-21). It switched from V15 between 2026-10-18 and 2026-10-22 (Wayback CDX). fm1-linux-update's pinned FW_SHA256 therefore no longer matches what its `setup` downloads; its setup would fail the checksum.
- V15 (FM-1_015, 699,956 B, sha256 db1642b2…7edb8a) was recovered from the Wayback Machine snapshot 20261009203224 and matches the pinned sha256.
- No V14 package found or fetched.
Hardware touched: read-only (lsusb, aconnect -l, amidi -l). No SysEx sent.
Questions for the planning chat:
- The stock URL now serves V16. Does that change anything for the chosen base (Felucca / Baud Girl need V15) or for D012?
- If the unit turns out to be FM-1_014, do we need a V14 package for safekeeping, and from where?
Next: User runs the --info command and pastes the output; then record the stock version in docs/device.md and close Step 0.2.

2026-11-04 14:52 — Step 0.2 — Identify the unit (read-only), completed
Status: done
Did:
- User approved a one-command allow rule in .claude/settings.local.json (git-ignored) for exactly `.venv/bin/python -I third_party/fm1-linux-update/.cache/fm1_install.py --info --port FM-1`, then Claude Code ran it.
- Recorded the stock version in docs/device.md.
Learned / measured:
- Identity: `FM-1_015  [running]  port: FM-1:FM-1 MIDI 1 24:0`. The unit runs stock V15 in normal running mode.
- This agrees with the lsusb product-name hint ("FM-1" means V15 or later).
- The matching stock package is on disk: backups/stock-firmware/FM-1_015.fwsc (sha256 db1642b2…7edb8a). No V14 package is needed.
Hardware touched: read-only (one identity-query SysEx via fm1_install.py --info; nothing written)
Questions for the planning chat:
- M-VAVE's URL now serves V16 (FM-1_016). Per D012 the unit stays on V15. Confirm nothing in the plan assumes V16.
Next: Step 0.3 — flash chip ID + full flash backup (Claude only prepares the procedure; the user runs every device command).

2026-11-04 15:11 — Step 0.3 — Flash chip ID + full flash backup (prepare only)
Status: partial (procedure ready; waiting for the user to run the device steps)
Did:
- Read HANDOFF (14:58 and 15:02 notes). Followed the 0xB36014 rule: the checklist stops after the ID read if the chip is 0xB36014.
- Cloned into third_party/: MvaveFM1Unbricker (9b68a8282615329973cd0e42ea3a258225451648), m-flasher (55da95cc978e45d60d833a10edc3292c2a5918c2), and jl-uboot-tool at the unbricker's pinned commit adb3f18889e88ac512ce0a3c4d8cc3d3cb30696a into MvaveFM1Unbricker/jl-uboot-tool (wl82loader.bin sha256 d41da612…5376 matches the pin). Did not run the unbricker's `setup`.
- Read fm1_unbrick.py (softkey, find, info, backup, the write gate), jl-uboot-tool jltech/uboot.py (command opcodes), m-flasher src/jl.rs, boards.rs, ops.rs, cli.rs, midi.rs, docs.
- Installed jl-uboot-tool's requirements into .venv: crcmod 1.7, PyYAML 6.0.3, pycryptodomex 3.24.1, tqdm 4.70.1.
- Built m-flasher 0.8.2 from source: `cargo build --release` OK in 7 min on Linux Mint 22.3, no extra system packages. Not run against hardware (only --version/--help).
- Ran fm1_unbrick.py `find` (sysfs scan, no device) and `info`/`backup --dry-run` (tool's mock FM-1, no hardware) to confirm the expected output text.
- Wrote tools/verify_backup.sh (compares two dumps, sha256, size check, V15 firmware-region check); tested on fake dumps (identical / 1-byte diff / short / missing).
- Wrote docs/step-0.3-flash-backup.md: what each command sends, B36014 behaviour per tool, one-time prep, 10-step checklist with expected output and what to do when something is unexpected, optional M-Flasher cross-check.
- Created backups/flash/.
Learned / measured:
- Soft key into ROM boot: F0 22 24 35 7D F7 over USB-MIDI (both tools use the same bytes). The unbricker asks the identity first and requires typing UBOOT; m-flasher's download-mode sends it without an identity check.
- info/backup send only: SCSI INQUIRY, ROM READ_MEMORY 0xFD07, ROM WRITE_MEMORY 0xFB06 + JUMP 0xFB08 (loader into RAM), loader READ_KEY 0xFC09, GET_ONLINE_DEVICE 0xFC0A, GET_USB_BUFF_SIZE 0xFC14, READ_FLASH 0xFD05. Flash erase/write (0xFB00/01/02/04) are only reachable from restore and repair-settings.
- Not verifiable from source: the firmware's soft-key handler and the RAM loader blobs (wl82loader.bin; m-flasher mflr-loader-a.bin sha256 ae0cdf4965cc2438a362e933ccf11d41159322d67f2dc630a61ab84cc58e04d7).
- 0xB36014: fm1_unbrick.py accepts it (FLASH_IDS_OK); m-flasher lists it as FM-1 "UCUN UC25HQ80, in units sold from late 2026"; an FM-1 with 0xB36014 did not start after installing m-bootloader (m-flasher warning). The unbricker README says 0xB36014 units get locked into firmwares they can't update out of.
- This laptop: /dev/sg0 = internal SSD (APPLE SSD SM0512), /dev/sg1 = SD card reader. sg driver is built in. No m-flasher udev rule installed yet.
Hardware touched: none (no device command run; find only read sysfs)
Questions for the planning chat:
- If the ID is 0xB36014: both tools explicitly support reading it. Approve the backup (steps 6–8) for that case?
- OK to install M-Flasher's udev rule (4c4a:8057 only, uaccess) so no third-party tool runs as root?
- songs/amazing-grace/ is untracked; will be included in the next commit as asked (previews committed, not ignored, unless you prefer otherwise).
Next: User runs docs/step-0.3-flash-backup.md steps 1–10 and pastes the output; then record chip key, flash ID and backup sha256 in docs/device.md and backups/SHA256SUMS.

2026-11-04 15:32 — Step 0.3 — Flash chip ID + full flash backup (device steps run by the user)
Status: done (off-laptop copies of the backup still to be made by the user; optional M-Flasher cross-check not done)
Did:
- User installed M-Flasher's udev rule (/etc/udev/rules.d/70-m-flasher.rules, identical to the reviewed file); /dev/sg2 then had an ACL for kjs, so no tool ran as root.
- Claude Code ran only the read-only checks (fm1_install.py --info, fm1_unbrick.py find, lsusb, sysfs, dmesg) and tools/verify_backup.sh. The user ran softkey (3x), info (1x) and backup (2x) in the FM-1 backup terminal tab, one step at a time.
- Recorded chip key, flash ID and backup sha256 in docs/device.md; added the dumps to backups/SHA256SUMS (sha256sum -c: all OK). Named copy: backups/flash/FM-1_015_full-flash_856014_2026-11-04.bin.
- Updated docs/step-0.3-flash-backup.md with what went wrong (below).
Learned / measured:
- Chip key 0x980F, flash ID 0x856014 (Puya), device type 0x03, loader USB buffer 32768 bytes. The 0xB36014 stop rule did not apply.
- Full flash sha256 794d72f01c293dc5d1c69ee4e54e4db956112bfe8734d0449fe4ea36e92f43f4 (1,048,576 B): 4 full reads in 2 separate ROM boot sessions, all identical.
- Boot head 0x0000–0x3FFF sha256 d9f43191…bd67 = official V15 head.
- Firmware region 0x4000–0x92FFF sha256 8c1d13a5934eb3c70a5fec644577618a31b2a96c8748a7b692c506a86505dbb1 is NOT the V15 package region (6edf3c37…): 0/143 sectors identical, 92.8 % of bytes differ, no constant XOR pattern. Cause unknown. Writing the package's region would not reproduce this unit's flash; the dump is the bit-exact way back.
- Running `backup` right after `info` in the same ROM boot session fails: re-uploading the loader gives OSError Errno 19 in mem_write (RAM only); the unit then reset itself and booted V15 normally. One loader command per ROM boot session.
- On this laptop (USB port 1-2) every connect has a first descriptor read timeout (error -110); the device appears ~15–20 s after plug-in or soft key, sometimes just past softkey's 15 s wait. Data was not affected.
- ROM boot mode USB descriptor strings read "WL80UBOOT1.00"; SCSI inquiry reads "WL82 UBOOT1.00".
- Each run also saves 512 B of retained RAM (sha256 cd1e8568…9163 both times, no Melodee crash record).
Hardware touched: read-only by Claude Code. By the user: soft key into ROM boot (MIDI SysEx F0 22 24 35 7D F7) x3, RAM loader uploads x3 (info x1, backup x2), flash reads only; no flash write or erase. Unit ends on FM-1_015 [running].
Questions for the planning chat:
- Why does the on-flash firmware region differ from the V15 package's flash.bin? Does this change the "restore to stock = backups/stock-firmware/FM-1_015.fwsc" plan (HANDOFF 15:02)? Suggest: the full dump becomes the primary restore target.
- Run the optional M-Flasher cross-check (independent loader; its analysis decrypts the firmware with the chip key and may explain the difference)?
- Backup copies: repo backups/flash done; another disk and cloud are up to the user (will be noted in device.md when they say so).
Next: Planning chat reviews Step 0.3. Pending on the user: two off-laptop copies of the backup.

2026-11-04 15:50 — Step 0.4 — Emulator on Mint
Status: partial (built and verified headless; live window plays but is choppy on this laptop; user's listening check pending)
Did:
- Before 0.4 (HANDOFF 15:06 and 15:45): CLAUDE.md compiler-input rule updated to D014 (song folder: song.mid + song.yaml). docs/device.md: added "Restore options" (D015, pending the user). Skipped the optional offline m-flasher analysis of our dump (not started; would need a new crate around m-flasher's internals).
- Cloned https://github.com/joakimeriksson/fm1-emu into third_party/fm1-emu (9de1ef053a3c4bf56f16f05b5f0de193f4608a1d, v0.4.0, 2026-11-02; submodule fm1-sid not fetched). Read README, recon.md, fm1-live/fm1-emu/fm1-soc sources.
- Built fm1-live and fm1-emu natively: `cargo build --release -p fm1-live -p fm1-emu`, 2 min 40 s, no extra system packages (libasound2-dev and pkg-config already present). Node/Chromium not installed; web version not needed.
- Downloaded Felucca 1.7.0 (latest stable; 1.8.2-beta exists) from GitHub releases into third_party/firmwares/ (git-ignored), sha256 bff7c617cfb2c88e66b22aa691cb798dfa0d9cb12ff9a73deab51d404e82817c, identity FM-1_917. Emulator only.
- Ran stock FM-1_015 and Felucca 1.7.0 headless (--bench 5000 with a scripted C4 key and a MIDI note): both boot, both make sound for key and MIDI input. Ran Felucca in the live window with --midi all and played songs/amazing-grace/song.mid into it via `aplaymidi -p 14:0` (Midi Through).
- Wrote docs/emulator.md (build, play, measurements, scripting, gaps). Added "Hardware model (from the fm1-emu emulator)" to docs/device.md.
Learned / measured:
- Display 240x240 ST7789-class, RGB565. 27 note keys F3–G5 (MIDI 53–79), 14 buttons with LEDs (OCTDN OCTUP FX SCL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC), 7 encoders (SELECT ALGO K1–K4 PRESET), MASTER pot. Audio I2S 44.1 kHz stereo, 24-bit in 32-bit words. CPU 2x pi32v2 at 240 MHz. RAM at 0x01C00000 (recon ">=512 KiB", emulator 1 MiB), SRAM 256 KiB at 0x01F00000, flash 1 MiB XIP at 0x02000000.
- Emulated flash JEDEC is fixed at 85 60 14; no option for B3 60 14. Testing our 0xB36014 / unknown-ID handling needs a small emulator patch.
- Headless: stock 0.9–1.0x real time, Felucca 1.3x. Live window: Felucca 0.25–0.5x at 240 MHz, 0.5–0.8x at --mhz 120, many audio underruns (i7-5557U, thermal throttling active).
- Scripting: fm1-live --bench/--shot with FM1_BENCH_KEYS (keys, velocity, pressure, encoders, pot, raw MIDI), FM1_BENCH_WAV (s16le 44.1 kHz stereo), FM1_BENCH_PANEL (PPM 1280x820), FM1_DUMPRAM, --flash, FM1_FLASH_CUT, --usb + FM1_USB_SEND. Deterministic on the emulated clock. fm1-emu batch runner is a slow interpreter; one attempt with --snap/--wav produced nothing in 3 min 50 s (not investigated).
- fm1-emu has --raw-flash ("the flash as stored on the chip, app area encrypted; the flash controller decrypts on the fly"): supports the planning chat's per-chip-encryption explanation for our dump.
- Prior art in the emulator's catalog close to our goal: fm1-chord (chords under the keys), CoralFlower (one key = chord + bass), Polly (QY70-style arranger: "play a chord and a band of eight follows"), CTL-1.
Hardware touched: none (FM-1 not connected)
Questions for the planning chat:
- Live play is choppy on this laptop. Is headless-only acceptable for development and tests (plus live play on the real unit or a faster computer)?
- Add a JEDEC-ID option to our copy of the emulator (local patch in third_party, or upstream PR), to meet the CLAUDE.md flash-ID test rule?
- Worth studying Polly / fm1-chord / CoralFlower sources before designing our firmware (licences permitting)?
Next: User confirms sound and keyboard input in the live window; then Step 0.5 per HANDOFF.

2026-11-04 15:51 — Step 0.4 (addendum) — live check by the user
Status: done
Did:
- User played Felucca 1.7.0 in the fm1-live window (--mhz 120, --midi all) with the computer keyboard (A, S, D).
Learned / measured:
- Keyboard input and audio output work in the live window; the sound is choppy, as the speed measurements predicted (0.5–0.8x real time, underruns). Live play on this laptop is usable only as a smoke test.
- MIDI input in the live window: Amazing Grace was sent via aplaymidi to Midi Through, which fm1-live subscribed to; not separately confirmed by ear. MIDI input is confirmed headless (stock ch 2, Felucca ch 1).
Hardware touched: none
Questions for the planning chat: as in the entry above
Next: Step 0.5 per HANDOFF.
