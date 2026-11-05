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

2026-11-04 16:03 — Step 0.5 — Pick the base (comparison prepared)
Status: partial (comparison written; Felucca built; the other builds blocked for Claude Code by the auto-mode safety check on external code, handed to the user as tools/build_bases.sh)
Did:
- Read HANDOFF 16:00: added Polly as candidate D and the fm1-chord / CoralFlower reuse questions. The JEDEC patch for fm1-emu is for Step 0.6, not started.
- Cloned into third_party/: fm1-dock 94f8087, Felucca a4f007c (+ felucca-bsp 5942e68, Felucca-Engines d026007), felucca-bsp 9670b5d, fm1-base 2d8ed74 (+ bsp 773b0b9), fm1_open 4c7496d, foma 69a2375, fm1-polly 853ebd7 (+ base 02f422c, bsp cd76d43), fm1-trio 7c92b7a, fm1-chord 5117c27, CoralFlower 272cad9, awesome-fm-1 (shallow).
- JieLi toolchain: jieli-linux-toolchains-20260730.1 (pkgman.jieliapp.com -> jl-update.oss-cn-shenzhen.aliyuncs.com), sha256 a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd, in third_party/jieli/toolchain; clang 4.0.1 runs natively. AC79 SDK AC79NN_SDK_V1.2.1_2023-12-13 (d179b44, 1.1 GB) in third_party/jieli/. Pillow 13.0.0 + fontTools 4.68.0 in .venv.
- Built Felucca natively: 31 s, felucca.fwsc 610,286 B (FM-1_900); it boots and plays in fm1-emu.
- Wrote docs/base-comparison.md and tools/build_bases.sh (logs to build-logs/, git-ignored).
Learned / measured:
- Flash ID: Felucca 1.8.2-beta app accepts 0x856014 and 0xB36014 (beta, untested on hardware, issue #320). fm1-base, Polly, TRIO, foma, fm1-dock, fm1-chord, CoralFlower apps: 0x856014 only (unknown -> RAM-only, no crash). felucca-bsp's update loader has no flash-ID check (checks chip key + CRC, writes only 0x4000-0x92FFF). fm1-dock's and foma's loaders refuse anything but 0x856014. FM1 Open: binary loader/engine, no ID constant found; not verifiable.
- FM1 Open builds on Windows only (JieLi Windows toolchain 2.5.3); its engine is a binary with an FM1-devices-only grant. Its sdk/firmware/FM-1_015.fwsc is byte-identical to our backups/stock-firmware/FM-1_015.fwsc.
- foma needs Docker (not installed).
- Polly: Sonivox EAS GM (Apache-2.0), 32 voices max, 22.05 kHz doubled; arranger arr.c: 480 PPQ, sample-accurate in the render, MIDI clock in, arr_track_mute, live chord. Plays styles, not songs.
- fm1-base: MPL-2.0, plat.h (68 functions), canvas gfx with Terminus/Barlow fonts, host simulator + browser emulator, app slot 581 KB flash / 96 KB RAM / 336 KB pool, flash data areas, APP_USB_CMD, resource region, beta.
- Felucca build: RAM .data+.bss 62,324 of 73,728 B; pool 326,236 of 368,640 B.
- Whole ecosystem 2-5 weeks old; activity table in docs/base-comparison.md. TRIO (MPL-2.0, fm1-base) is close prior art: chord recognition, walking bass, drums.
Hardware touched: none
Questions for the planning chat:
- Decision: B' fm1-base (+ EAS, Polly as reference), fork Polly (GPL, fastest), or an fm1-dock module (second mode first)? The recommendation leans to fm1-base; the user decides.
- Licence: is GPL-3.0 for our firmware acceptable? (Decides whether we may copy Polly's arranger or fm1-chord/CoralFlower code.)
- Our "refuse unknown flash IDs" rule vs felucca-bsp's loader, which checks none: add a check in our loader build (and upstream?).
Next: User runs tools/build_bases.sh; Claude Code fills in the build rows; then the decision (user + planning chat); Step 0.6 (fm1-emu JEDEC patch) per HANDOFF.

2026-11-04 16:10 — Step 0.5 (addendum) — all candidate builds
Status: done (comparison complete; decision pending with the user and the planning chat)
Did:
- At the user's request, Claude Code ran tools/build_bases.sh: Felucca, fm1-base (template), Polly, TRIO, fm1-dock, fm1-chord, CoralFlower all built (exit 0, 16–56 s each). Logs in build-logs/ (git-ignored).
- Ran each package headless in fm1-emu (--bench 4000, C4 at 2.0–2.6 s): all boot and make sound.
- Filled in the build rows and risks of docs/base-comparison.md.
Learned / measured:
- Image / RAM (.data+.bss of 98,304 B unless noted) / pool (of 344,064 B):
  fm1-base template 221,896 B / 76,388 / 93,960. Polly 574,704 B / 95,840 / 289,540. TRIO 377,584 B / 84,824 / 283,792. fm1-chord 536,396 B / 94,996 / 335,704. CoralFlower 366,452 B / 92,268 / 322,828. Felucca 517,616 B / 62,324 of 73,728 / 326,236 of 368,640. fm1-dock host 61,400 of 196,320 B / 45,508 / 46,080 of 65,536 (+ module RAM).
- Polly is ~99 % of the ~581 KB app slot and 97 % of RAM: forking it means cutting features before adding a song player.
- Emulator speed (headless): fm1-base 2.5x, Polly 3.3x, TRIO 2.4x, fm1-dock 1.15x, fm1-chord 1.0x, CoralFlower 0.95x real time.
Hardware touched: none
Questions for the planning chat: as in the entry above
Next: The decision on the base (user + planning chat), then Step 0.6.

2026-11-04 16:38 — Step 0.6 — pre-code checks (HANDOFF 16:30)
Status: done
Did:
- Updated the D015 heading in docs/device.md to "accepted 2026-11-04" (15:58 note).
- Asked the user about a prompt/HANDOFF difference (prompt: a held key plays middle C; HANDOFF: holding a key plays that note). User chose: each key plays its own note (C4 = middle C).
- Cloned upstream Sonivox EAS https://github.com/EmbeddedSynth/sonivox into third_party/sonivox (HEAD 86bd07341078084cb9f224b9e7d508fbf4751af0, 2026-10-02; tags up to v4.0.2).
Learned / measured:
1. EAS provenance: Polly credits https://github.com/EmbeddedSynth/sonivox. Compared file by file with upstream HEAD: 69 of Polly's files identical; 9 modified (eas_chorus.c, eas_filter.c, eas_filter.h, eas_midictrl.h, eas_reverb.c, eas_synth.h, eas_voicemgt.c, eas_wtengine.h, eas_wtsynth.c; changes marked "Polly:"). Headers: 7 keep the Apache-2.0 header; eas_filter.c and eas_filter.h have none (unclear). Polly's own: eas_host_fm1.c (SPDX Apache-2.0), eas_options.h, eas_version.h, eas_visibility.h, cutils/log.h. Polly's LICENSING.md lists the whole folder as Apache-2.0. Per the HANDOFF we use upstream only and copy none of Polly's changes or files.
   What we redo ourselves: the host layer (memory pool, no file I/O; upstream has host_src/eas_hostmm.c as a reference), the build configuration (voices, features, 22.05 kHz -> 44.1 kHz output), and optionally: sample-accurate note starts within an EAS block (without it a note lands on a block boundary: 128 frames at 22.05 kHz = 5.8 ms), CC74/CC71 filter, effect-send clamping and tails.
2. GM bank: wt_200k_G.c + wt_200k_samples.c in upstream arm-wt-22k/lib_src/ ("Source: wt_200k_G.dls", "Wavetable sound libary"), Copyright (c) 2009 Sonic Network Inc., Apache-2.0 header in both files; upstream LICENSE-2.0.txt, NOTICE, MODULE_LICENSE_APACHE2. Clearly redistributable under Apache-2.0 (keep the licence and NOTICE).
3. Flash budget (measured from Polly's -O2 objects): EAS code 28,846 B text + GM bank 227,160 B rodata (wt_200k_G) + ~3 KB other data = ~259 KB. fm1-base template image 221,896 B with Bluetooth (README: 77 KB without). Our firmware without Bluetooth: ~77 + ~259 = ~336 KB of the ~581 KB app slot (~245 KB left for our code). Song data outside the app slot: fm1-base's data area 0x97000-0xDFFFF (292 KiB), XDATA0 0x93000-0x96FFF (16 KiB), XDATA1 0xE5000-0xFBFFF (92 KiB), or a resource region written by the installer (Polly's: 43,032 B).
Hardware touched: none
Questions for the planning chat: none (all three checks pass)
Next: Step 0.6 code.

2026-11-04 16:53 — Step 0.6 — First firmware on fm1-base + EAS (emulator only)
Status: done
Did:
- firmware/vendor/: fm1-base 2d8ed748 + felucca-bsp 773b0b99 (the commits evaluated in 0.5) and upstream Sonivox EAS 86bd0734 (subset: arm-wt-22k lib_src + host_src + licence files), copied with git archive; provenance, licences and patches in firmware/vendor/README.md. Vendored (not submodules) so builds are offline and our patch is visible.
- Flash id: patched fm1-base (firmware/vendor/patches/fm1-base-flash-id.patch): app accepts 0x856014 and 0xB36014, else RAM-only (no crash); felucca-bsp's update loader now refuses unknown ids (-14) after the chip-key check and before any erase (small: one hook in loader.c, one check in ldr_core.c, a stub in the host test).
- fm1-emu JEDEC patch (HANDOFF 16:00): FM1_FLASH_ID=<6 hex> in SpiFlash::new; tools/patches/fm1-emu-jedec-id.patch + tools/apply_patches.sh (idempotent, rebuilds).
- Our app firmware/app/ (Licence: TBD, D017; no licence header): app.c (title, version, flash status, held key's name large, mini keyboard, keys -> GM Electric Piano 1 at velocity 100), synth.c (SPSC queue main loop -> audio interrupt, EAS at 22.05 kHz, x2 linear interpolation to 44.1 kHz, MASTER), eas/ (our eas_options.h, eas_visibility.h, eas_version.h, cutils/log.h, log/log.h, fm1inc/string.h, eas_host_fw.c: 40 KB bump pool, no files, strcmp, no-op reports). Polly's EAS changes not copied.
- firmware/build.sh, tools/emu.sh, tools/ppm2png.py, Makefile (make firmware / emu / shot / patches). docs/firmware.md (build, run, flash ids, rescue), docs/step-0.6-emulator.png.
Learned / measured:
- Build: ~10-16 s. App 340,236 B; RAM .data+.bss 66,604 of 98,304 B; pool 69,384 of 344,064 B; loader 8,371 B; package fm1acc.fwsc 611,527 B, identity FM-1_7100. All fm1-base checks pass (register access, no soft-double, no 64-bit mul beside loads).
- _16_BIT_SAMPLES made the GM set 440 KB (image 552 KB); _8_BIT_SAMPLES (the set as made, 0x33CBD B) gives 340 KB.
- Upstream EAS does not build without _REVERB_ENABLED/_CHORUS_ENABLED (eas_public.c uses them unguarded): both on.
- fm1-emu headless (--bench 4000, C4 2.0-2.6 s): 1.2-1.4x real time; silence before, C4 rms 1271, decay after. FM1_FLASH_ID 856014 and b36014: "FLASH OK" (identical screens); 123456: "FLASH: RAM ONLY", still plays, no crash.
- Rescue (fm1-base, unchanged): REC at power-on -> ROM update mode; 2 failed starts (first 30 s) -> SAFE MODE (no sound, USB on, PLAY retry, REC update mode); 4 -> ROM update mode; USB soft key; cut-off update resumes; OCT-+OCT+ at power-on = panel set-up. Documented in docs/firmware.md.
- fm1-base's words.tsv needs a "# url:" line: placeholder https://example.invalid/fm1-accompaniment/words/ until we have a site.
- Package identity FM-1_71xx chosen; not yet checked against the Firmware Hub's list.
Hardware touched: none
Questions for the planning chat:
- Loader refusal (-14) leaves the unit in update mode on an unknown chip (nothing erased). OK, or should it also drop the update record so the old app boots (needs a flash write on that unknown chip)?
- Velocity: fixed 100 now; turn on fm1-base's APP_TOUCH (key velocity/pressure) in the next step?
- Identity FM-1_71xx and a URL for the screen-reader words: what should they be?
- Not done: sample-accurate note starts inside an EAS block (notes land on 5.8 ms block boundaries), EAS CPU load on the device.
Next: Planning chat reviews 0.6; Step 0.7 per HANDOFF.

2026-11-04 19:59 — Step 0.7 — Song prototype (Part A, emulator) + first-writes checklist (Part B, prepared only)
Status: partial (Part A done; Part B prepared and WAITING FOR REVIEW: pre-flight P1 and P2 are ❌, both need the user; no device command run)
Did:
- A0: upstream Sonivox EAS (86bd0734) has the SMF player (eas_smf.c, already compiled in 0.6). It plays from a buffer: EAS_FILE is a locator with readAt/size callbacks (host_src/eas_types.h:147-151); our host layer (eas/eas_host_fw.c) now implements the file calls over it, with duplicate handles per track. Per-track mute exists upstream behind JET_INTERFACE (eas_smf.c:302-305, 642-671: PARSER_DATA_MUTE_FLAGS / SET_MUTE; a muted track drops note-ons, keeps note-offs and controllers, eas_midi.c:240-300). JET_INTERFACE changes a struct (eas_miditypes.h jetData), so it is set globally in eas_options.h and upstream jet.c is linked (never opened). Per channel: no public API (CHANNEL_FLAG_MUTE is SP-MIDI's only). Nothing of Polly's used.
- A1: tools/proto_song_to_c.py (fm1.toml gen, venv pyyaml + mido) -> build/gen/proto_song.h: song.mid's 8,890 B as is, track->layer table (variant :1; :2 tracks muted for good, old chords track ignored), chord chart from song.yaml (pickup, form sections, ending) as 30 (tick, chord) changes. Cross-check: the chart equals song.mid's old chords track. Refuses tempo/metre changes (prototype limit).
- A2: PLAY play/stop (green LED); FX/SEL/ENV/LFO = drums/bass/harmony/melody mute (top row, left four; lit = plays); MASTER = volume (platform gain); keys = EP1 on ch 16. REC, OCT-, OCT+ untouched (rescues). Mute = track mute + CC123 on the layer's channel (a held chord stops at once).
- A3: screen: current chord at 2x FONT_L (64 px), next chord, bar:beat + beat dots, four layer boxes (amber MUTE), CPU % and voices, version on the stop screen. font_full L, M (lower case: Em).
- A4/A5: tools/proto_test.py (make proto-test, ~10 min, 80+ headless fm1-live runs) -> docs/step-0.7-emulator-report.md, screenshots in docs/step-0.7/: ALL PASS on the committed package.
- Prototype code marked: firmware/app/proto_player.[ch], proto_ui.c (app.c only includes it), tools/proto_*.py. Hello world kept: tools/build_hello.sh (make hello) builds commit 4629478's firmware -> build/hello/fm1acc-hello.fwsc, 340,236 B app as in 0.6, sha256 e191c005…1a1c.
- HANDOFF 17:10 item 3 (identity): checked awesome-fm-1 mooring/catalog.json (c7cc615, 2026-11-01, 87 identities), fm1-emu's catalogue and the id_prefix of every fm1.toml in third_party/. FM-1_71 is taken (Bunker Evolved FM-1_7100002, Skiff FM-1_712). Moved to FM-1_46 (none used): dev builds FM-1_4600. Words URL placeholder kept.
- Part B: docs/step-0.7-first-writes.md (pre-flight, packages, 5 steps with commands / expected output / time / what if it fails, restore paths A, B1, B2). Read-only only: lsusb, kernel log.
- Downloaded third_party/firmwares/felucca-1.8.1.fwsc (Felucca's web installer file, gh-pages 1db38f8). Booted it in fm1-emu (4 s: screen + sound). Nothing sent anywhere.
- Committed 8bc977e (code). This entry's commit: docs, report, screenshots, and the planning chat's HANDOFF additions (17:10, 17:15) as found.
Learned / measured:
- Our package: commit 8bc977e, firmware/app/build/fm1acc.fwsc sha256 58be4537be314bf463dc828c919b18675c2626d01be5a01724560b3cc760d54f, 611,527 B, identity FM-1_4600, v0.1.0-proto; app 370,672 B of ~581 KB (+30 KB over 0.6: fonts with lower case, jet.c, song 8.9 KB, player); RAM .data+.bss 67,340 of 98,304 B. Two builds give the same sha256. Copy: build/step-0.7/fm1acc-proto-0.1.0.fwsc (+ .sha256).
- Emulator (fm1-live, emulated clock): busiest bar 17 (the turnaround into verse 2): 14 voices of 24, platform audio load 14 %, EAS render 16.5 % of a 5.8 ms block. Harmony muted: at most 7 voices, load 8-9 %. Typical bar 10 voices / 11 %. Headless speed ~2x real time.
- Harmony muted: fewer voices in all 32 bars; audio identical to the all-layers run until the song starts; build/emu/proto/harmony-muted.wav (75 s, local).
- Chord vs audio: with a 30 ms look-ahead the chord was up to ~20 ms LATE (2 of 4 changes): the LCD is slow (SPI, ~0.32 ms a row; a full frame ~77 ms). Fix: the big chord gets its own 84-row blit, sent first, and the screen runs 80 ms ahead of the parser. Now 6 of 6 changes on screen 40-80 ms before their first audio frame (±~20 ms for the WAV/clock alignment).
- Flash ids: 856014 and b36014 identical screens, sound; 123456: "RAM ONLY" in amber, the song plays on, no crash.
- Bug found and fixed in my own measurement: 256*24*10^6 overflowed 32 bits (render % was 3.3x too high).
- fm1-live: FM1_BENCH_NOKEYS also clears FM1_BENCH_KEYS; our app takes buttons only after the splash (~1.2 s; PLAY at 1.0 s was ignored).
- Felucca 1.8.1 has a git tag (f4df296, 2026-11-02) but NO GitHub release; latest GitHub stable is 1.7.0, 1.8.2-beta a prerelease. 1.8.1 ships through the web installer: felucca-1.8.1.fwsc, 610,286 B, sha256 187d4a9bc5b555866848d8e2c9964bea134d764cdeb62712a30da1825c5caf15, identity FM-1_918 (git blob a85155fb = GitHub's).
- Felucca 1.8.1 and 0x856014: app project.c:1195 accepts only 0x856014 (else RAM-only); its own loader (firmware/loader/ldr_core.c) has no flash-ID check, writes only 0x4000-0x92FFF after board, chip-key and CRC checks. Same for 1.7.0 (project.c:1162).
- Each package is written by its OWN update loader (bsp/chip/ota.c:3-13); the running firmware only checks and stages it. So "stock V15 from our firmware" depends on our app's step 1: fm1-base's host test (tests/ota_test.c on our vendored ota.c) stages FM-1_015.fwsc ("commits ok") and refuses it with a damaged boot area (-5).
- Installer: the vendored firmware/vendor/fm1-base/bsp/tools/fm1_install.py accepts Felucca 1.8.1, stock V15 (by its sha256 db1642b2…, no --force), our prototype and the hello world; it refuses FM-1_016. The Felucca fm1_install.py in fm1-linux-update/.cache and fm1-update.sh need --force for stock: not used.
- Last resort: m-flasher 0.8.2 `restore BACKUP.bin` (writes the differing sectors after its own backup and a typed yes); middle path: fm1_unbrick.py restore FM-1_015.fwsc --verify-v15. Neither run yet.
- USB today: the FM-1 is on port 1-2 again; dmesg shows `device descriptor read/64, error -110` and ~15 s enumeration (327.3 s -> 343.2 s).
Hardware touched: read-only (lsusb, kernel log, by Claude Code). No write.
Questions for the planning chat:
- Felucca 1.8.1 (web installer's file, no GitHub release) or 1.7.0 (GitHub release, emulator-tested in 0.4) for the round trip? The checklist uses 1.8.1.
- Step 1's "a setting survives a power cycle": I did not verify which Felucca setting saves by itself or its key presses (Felucca saves preferences such as the palette: editor_preferences.c settings_save); the checklist points to Felucca's manual. Name one?
- OK to keep the screen 80 ms ahead (chord appears 40-80 ms before the sound in the emulator)? Device LCD timing may differ; the user checks by eye in step 3.
- Notes start on 5.8 ms block boundaries still (Phase 1 item from 0.6); the user listens for loose drums in step 3.
Next: the user makes the off-laptop backup copies (P1) and finds a clean USB port + cable (P2, read-only; Claude Code can read dmesg/--info with them); the planning chat and the user review docs/step-0.7-first-writes.md; then the user runs steps 1-5; Claude Code records the results and the playing-along impressions here.

2026-11-04 20:10 — Step 0.7 (addendum) — pre-flight P1
Status: partial (P1 recorded; P2 still ❌; checklist still waiting for review)
Did:
- Recorded the off-laptop backup in docs/device.md and marked P1 ✅ (one copy) in docs/step-0.7-first-writes.md, on the user's word.
Learned / measured:
- Off-laptop copy: the user's USB thumb drive (kept unplugged), checked by the user; sha256 not re-read by Claude Code. No cloud copy. The user chose to go ahead with this one copy.
Hardware touched: none
Questions for the planning chat:
- device.md asks for three copies (repo, another disk, cloud); there are two. Require the cloud copy before step 1?
Next: P2 (a clean USB port + cable, read-only), then the review of docs/step-0.7-first-writes.md, then the user runs steps 1-5.

2026-11-04 20:15 — Step 0.7 (addendum) — P2 USB port test 1
Status: partial (P2 still ❌)
Did:
- Read-only, by Claude Code: kernel log, lsusb, fm1_install.py --info (vendored), after the user moved the FM-1 from port 1-2 to port 1-1 (same cable).
Learned / measured:
- Port 1-1: `new full-speed USB device` at 3769.48 s, `device descriptor read/64, error -110` at 3785.52 s, device found at 3785.74 s: ~16 s, the same -110 as on port 1-2. lsusb `4c4a:c755 FM-1`; --info `FM-1_015 [running]`.
- Same failure on two ports with the same cable: the cable (or the FM-1's own USB start-up) is the likelier cause than the port.
Hardware touched: read-only
Questions for the planning chat: none yet
Next: the user tries another data cable (on 1-1, then 1-2); Claude Code reads the same three checks.

2026-11-04 20:25 — Step 0.7 (addendum) — P2 USB test 2 (another cable)
Status: blocked (P2 cannot pass as written; decision for the planning chat)
Did:
- Read-only, by Claude Code: kernel log, lsusb, fm1_install.py --info, after the user swapped to another data cable (the FM-1 came up on port 1-2).
Learned / measured:
- New cable, port 1-2: `new full-speed USB device` at 4329.08 s, `device descriptor read/64, error -110` at 4345.10 s, found at 4345.33 s. --info `FM-1_015 [running]`.
- All three tries so far (1-2 old cable 327->343 s, 1-1 old cable 3769->3785 s, 1-2 new cable 4329->4345 s): the same -110 and the same ~16 s, every time. Two ports, two cables: the FM-1's own USB start-up (stock V15; Step 0.3 saw it in ROM boot mode too) is the likely cause, not the port or cable. After the -110 the device always enumerates and answers normally; Step 0.3's 4 full flash reads were identical.
- During an install the FM-1 re-enumerates twice (into the update loader, back into the new firmware). The vendored installer waits up to 30 s for the loader and 40 s for the restart (fm1_install.py:42-44), more than the ~16 s seen.
Hardware touched: read-only
Questions for the planning chat:
- P2 asks for no -110. With this unit that seems impossible on this laptop (two ports, two cables, always ~16 s). Options: (a) accept it as this unit's behaviour, since it always recovers and the installer's 30/40 s waits cover it; (b) try a powered USB hub or another computer first. Recommendation: (b) once if a hub is at hand, else (a), recorded in device.md.
Next: the planning chat decides P2; then the review of docs/step-0.7-first-writes.md; then the user runs steps 1-5.

2026-11-04 20:30 — Step 0.7 (addendum) — P2 decided
Status: partial (pre-flight complete; checklist WAITING FOR REVIEW by the planning chat and the user; no device write)
Did:
- The user chose option (a): accept the single -110 and ~16 s enumeration as this unit's behaviour. Recorded in docs/device.md ("USB for writes") and as P2 ✅ (accepted by the user) in docs/step-0.7-first-writes.md: port 1-2, the new cable; a second -110 or more than ~25 s to appear during a step = stop.
Learned / measured: none new
Hardware touched: none
Questions for the planning chat:
- Please confirm P2 (a) and P1 (one off-laptop copy, no cloud) in the review, plus the open questions of the 19:59 entry (Felucca 1.8.1 vs 1.7.0, which Felucca setting to test, the 80 ms chord lead).
Next: the planning chat and the user review docs/step-0.7-first-writes.md; then the user runs steps 1-5.

2026-11-04 20:48 — Step 0.7 — Part B: first writes, run by the user
Status: done (steps 1-5 passed; our prototype left installed). Pending from the user: the device's CPU % / voices reading and listening / playing-along notes; step 5's quick on-unit check (PLAY + one mute) not yet confirmed in words.
Did:
- HANDOFF 20:40: checklist approved with additions; added its rules (suspend off, MIDI apps closed, USB power, user's own terminal, stop on unexpected output) and marked the P2 procedure historical. Pre-check (read-only, Claude Code): FM-1_015 [running], no emulator/MIDI apps.
- The user ran every write in their own terminal with the vendored installer firmware/vendor/fm1-base/bsp/tools/fm1_install.py (no override flag), on port 1-2 with the new cable; Claude Code read the kernel log and --info after each step.
- Step 1, Felucca 1.8.1 (sha256 187d4a9b… checked): "checking the package (22 reads) … done: the FM-1 runs FM-1_918". Boots, plays (user). A setting saved and survived a power cycle (user; which setting not recorded). --info after the power cycle: FM-1_918 [running].
- Step 2, stock V15 from Felucca (sha256 db1642b2… checked): 73 reads, "done: the FM-1 runs FM-1_015". --info FM-1_015 [running].
- Step 3, our prototype (sha256 58be4537… checked): 25 reads, "done: the FM-1 runs FM-1_4600". --info FM-1_4600 [running]. The user checked items 1-9 (screen, PLAY, all four mutes, chord timing by eye, MASTER, keys, CPU/voices display, speaker and headphones, playing along, end of song): all as expected. Hello world not needed.
- Step 4, stock V15 from OUR firmware: 73 reads, writing 100 %, restarting; --info FM-1_015 [running]. We can leave our firmware by the normal path.
- Step 5, our prototype again: 25 reads, "done: the FM-1 runs FM-1_4600"; --info FM-1_4600 [running]. Left installed. docs/device.md: new "Currently installed" section.
Learned / measured (kernel log, port 1-2):
- Felucca's and our update loader enumerate as 1209:0002 "Felucca Update" ~0.4-0.7 s after the disconnect; writing the app area took ~54-55 s (Felucca 4992->5047 s; ours 5706->5760 s, 6234->6289 s). Felucca boots as 1209:0001 "Felucca" (~5 s after the write, and ~5 s after a power cycle); ours as 1209:0001 "FM-1 Accompaniment" 0.13 s after the loader leaves. No -110 with Felucca's, our loader's or our app's USB.
- M-VAVE's update loader enumerates as 4d4a:4155 "USB Composite Device", writes ~24 s (5543->5567 s, 6136->6160 s); stock then comes up with the single -110 after ~16 s every time (5568.9->5584.8 s, 6162.1->6178.2 s). So the -110 / ~16 s is stock V15's USB start-up only.
- Stock V15's install output ended at "restarting" in the paste of steps 2 and 4; for step 2 the user later pasted "done: the FM-1 runs FM-1_015". --info confirmed FM-1_015 both times.
- Device CPU load / voices at bar 16-17 and listening notes: not reported yet (emulator: 14 %, 14 voices).
Hardware touched: WRITE, by the user: app area 0x4000-0x92FFF five times (Felucca 1.8.1, stock V15, our prototype, stock V15, our prototype) through each package's own update loader, plus the update staging area 0xE0000-0xE4FFF. No ROM boot session, boot head untouched. Read-only by Claude Code: kernel log, lsusb, --info.
Questions for the planning chat:
- Phase 1 can start from a unit running our firmware; the user's device CPU / sound notes will follow in a LOG addendum.
- Cloud copy of the backup still recommended (HANDOFF 20:40 item 3).
Next: the user sends the CPU / voices reading and playing-along impressions (addendum); the planning chat reviews Step 0.7 and sets Phase 1.

2026-11-04 23:16 — Milestone M1a (steps 1.1-1.3) — song format v1, fm1song, starter songs
Status: done (format summary below, for review by the planning chat; per HANDOFF 23:02 M1b goes on without waiting)
Did:
- docs/song-format.md: source format `fm1-song/1` (song.yaml + song.mid) and the compiled format `.fm1s` v1.0, device behaviour for count-in and loops, and a "Deferred" list.
- tools/fm1song/ (Python; core modules never print or exit, the CLI is cli.py; wrapper tools/fm1song.sh): `validate`, `compile`, `info`, `dump --json`, plus `preview` (mp3/wav). Own SMF reader (smf.py) so malformed files give clear errors. Deterministic output (tested).
- tests: 62 pytest tests (`make song-test`, ~5 s): every starter song compiles with no warnings; 15 song.yaml errors and 7 song.mid errors each checked for their message and bar:beat; compiled-file truncation / checksum / header-field tests; fuzzing: 3000 mutated .fm1s (half with the CRC fixed up so the structure checks run), 400 mutated song.mid, 400 mutated song.yaml: only SongError ever raised.
- songs: amazing-grace moved to v1 (song.yaml only; song.mid and previews unchanged, "keep the music"). New, generated by songs/make_songs.py: twelve-bar-blues (C, 4/4 shuffle, 36 bars), saints (F, 4/4, 3-beat pickup, 32 bars), four-chords (G, I-vi-IV-V, straight 8ths, 32 bars), scarborough-fair (D dorian, 3/4, 32 bars). Each: drums/bass/harmony with :1 and :2, melody, chart, sections, two mp3 previews. songs/README.md says they are generated (D004).
- Makefile: `songs` (regenerate + compile), `song-test`.
Format summary (for the review):
- Source: song.yaml keeps v0.1's shape. Changes: `format: fm1-song/1`; `form` entries may name their `chart` part (default "Verse 2" -> `verse`) and a section `pickup_beats` (lead-in, default = the song's); layers use `program:` (GM 1..128) instead of free-text `sound:`; optional `mode:` (D dorian), `volume`, `pan`; `rights.generated` names the generating script. Chart syntax unchanged (`|`, `.`, N.C., even split), plus a fixed list of chord qualities (docs/song-format.md 1.1). song.mid: tick 0 = start of the first bar in the file (the full pickup bar if there is one), as the sample does.
- Compiled: 32-byte header (magic FM1S, version 1.0, size, CRC-32, ppq, metre, tempo, end tick, bars, pickup, flags) + a chunk directory; chunks META, SECT, CHRD, CEVT (chord changes with level-1 and level-2 voicings, 5 notes each), BARS (per bar: the chord at beat 1, section, flags: gives "this bar + next 3"), LAYR, and one event stream per layer variant (EVD1, EVB2, ...: 8-byte events with absolute ticks, so the player can binary-search on loops and variant changes). Unknown chunks are skipped; every count, offset and value has a bound (docs/song-format.md 2.2, 2.4).
- Sizes: 15-32 KiB per song (8-byte events; compression deferred).
Judgement calls (with reasons):
- D009 and D004 are not in the repo (vault only); I used HANDOFF, plan-M1 and the amazing-grace sample as the reference. D004 read as "say the songs are generated", from plan-M1's wording.
- Repeated chords merge into one chord event (`| G | G |` = one change): this is what fixes the user's "NEXT a bar too early" feedback on the data side; the bar table still has one entry per bar.
- Voicings computed by the compiler (2 levels; harmony variant 3 would show level 2), not read from the harmony track: one rule for every song; the generated songs' harmony uses the same voicing code, so what you hear is what the screen will show. Amazing Grace's hand-made level-1 voicings differ slightly (e.g. D: D3 F#3 A3 in the MIDI, the compiler places D the same; C: C4 E4 G4 in both).
- Loops with pickups: loop = [start - lead, end - lead] (a printed repeat with a pickup cuts the same beats from the last bar). Whole-song loop uses the first section's lead.
- Program changes in a track are ignored when song.yaml names a program (warning only if they differ).
- 3/4 and 4/4 family only (2/4..7/4); 6/8 deferred.
- Scarborough Fair: the traditional tune is public domain, but the well-known versions are copyrighted arrangements, so the melody is a simplified version of my own and rights says "check before publishing".
- Saints melody and the blues/four-chords tunes are typed from memory / written by me; the melodies sit on chord tones on 70-85 % of on-beat notes (Amazing Grace's: 82 %), a quick check that keys and chords line up. I could not listen to the previews (no audio here).
- Installed into .venv from PyPI: pytest 9.2.0, numpy 2.5.4, lameenc 1.9.0 (mp3 without ffmpeg, which is not installed). Previews are 64 kbps mono mp3 (amazing-grace's are 96 kbps from ffmpeg; left as they are).
Hardware touched: none
Questions for the planning chat:
- Format review: anything to change before more songs exist? In particular (a) the loop rule with pickups, (b) compiler-computed voicings vs taking them from harmony:1, (c) 8-byte events (simple; ~2x a delta-coded stream).
- The vault's Song Format Draft may have fields I did not see (D009). Please list any that v1 should carry.
- Arrangement licence (TBD) for all starter songs; proposal from the sample: CC0-1.0.
Next: Milestone M1b (device parser, player, screens, emulator tests, install checklist), without stopping (HANDOFF 23:02).

2026-11-04 23:44 — Milestone M1b (steps 1.4-1.5 + hardware checkpoint, prepared) — device parser, player, screens
Status: done; install checklist docs/step-M1-install.md WAITING FOR REVIEW by the planning chat and the user. Nothing installed.
Did:
- firmware/app/song.c/.h: the .fm1s parser in standalone C (no platform calls): song_peek (header + META, for the list), song_open (CRC and every bound), accessors that read in place. tools/song_fuzz.c + song_fuzz.sh: host gcc, ASan + UBSan, -fno-sanitize-recover; 10-minute run: 2,304,000 mutated files (75 % with the CRC fixed up), 43,182 accepted and every accessor walked, no sanitizer report (build/fuzz/fuzz-600s.log). `song_fuzz --diff` + pytest: the C parser and the Python reader accept/refuse exactly the same 1,500 mutated files.
- firmware/app/player.c/.h: our scheduler (D017), in the audio interrupt before each EAS render: per-layer streams, note tracking (128-bit sets) so mutes / variant changes / loop jumps / stops release exactly the sounding notes; tempo 50-120 % with an exact 64-bit position (no drift); count-in (one bar + the pickup's empty beats, GM wood blocks); loops whole song / current section (docs/song-format.md 3.3). RAM stats for the tests ("PLST").
- firmware/app/ui.c: song list (title, key, metre, tempo) and play screen: big chord, NEXT + countdown ("IN 3") only in the last bar before a change, bar strip (this bar + 3; held chords dimmed; beat cursor), layers with level and MUTE, tempo / loop / count-in flags, section, CPU/voices debug line. docs/controls.md: the control map proposal (REC, OCT-, OCT+ left free).
- Songs bundled in the app image as const data (tools/songs_to_c.py, fm1.toml gen): 5 songs, 113,208 bytes; image 483,808 B of the ~581 KB slot.
- Removed the Step 0.7 prototype (proto_*.c/h, proto_song_to_c.py, proto_test.py; in git at 98fc08c), JET_INTERFACE and jet.c. Version 0.2.0-m1.
- tools/m1_test.py (`make m1-test`): per song a full run (count-in, bass L2, harmony and drums muted mid-song), a tempo run (50 % then 120 %), a section-loop run; flash ids 856014 / b36014 / unknown. ALL PASS on the dev build and again on the release package itself: docs/step-M1-emulator-report.md (+ docs/step-M1/*.png).
- Release package for the install: build/step-M1/fm1acc-0.2.0-m1.fwsc, sha256 bf8f640f8137efc278fa17f6bcd7c21791eb1adbd68a7b7299c22f851804ff25, identity FM-1_46000200, from commit 018e5d7. The vendored installer's load_package (offline, file only) accepts it without overrides; V16 still refused.
- docs/step-M1-install.md: pre-flight P1-P7, rules from Step 0.7 (20:40), step 1 install over the prototype, step 2 on-unit checks (incl. the user's feedback #1 in bars 4-5 and 15-16), optional step 3 (back to stock and again), fallback = reinstall the prototype, restore paths unchanged.
Learned / measured (emulator):
- Chord changes reached the LCD 81-99 ms before the block that plays them (every change, every song; lead 100 ms).
- Busiest: 14 voices of 24, CPU 14 %, longest EAS render 17.0 % of a block (Blues); Amazing Grace 14 / 14 % / 16.2 %.
- Song length from PLAY to the end within one 256-frame block of the prediction for all five; tempo run: position within 1 tick of the exact value after 14 tempo steps; frames played = emulated time (20004 vs 20000 ms).
- Mutes: muted note-ons held back; 0 EAS voices 1 s after the end; no block hit the 128-events cap.
- felucca-bsp docs/CPU.md: inlined 32x32->64-bit multiplies beside loads corrupt registers on this CPU; the first build inlined some in player_block. All are now in a noinline helper; the build's check says none is inlined.
Judgement calls (with reasons):
- Songs as const data in the app image (not the resource region or data area): no new flash layout or write path, covered by the package's checks; songs change only with a rebuild until `fm1song send` (1.7).
- Release build identity FM-1_46000200 instead of the dev FM-1_4600: the prototype on the unit is FM-1_4600, so `--info` could not show that the install happened. fm1-base's own `--release` scheme (prefix + XXYYZZ).
- Fallback in the checklist is the prototype (known good on this unit), not the hello world.
- Events are sent at the start of the block they fall in (up to 5.8 ms early, like EAS's file player); sample-accurate starts stay deferred (HANDOFF 17:10 item 4).
- Chord lead raised from 80 to 100 ms (more drawing per frame than the prototype; the beat cursor and countdown use 30 ms). The UI waits for the chord's DMA before logging it, so the test is strict.
- Opening a song resets mutes, levels (all L1) and the loop; tempo and count-in are kept.
- Count-in on by default (ARP toggles).
- Section loop picks the section by the heard position (with lead-ins), and from the top when started with PLAY.
- Not re-proven: back to stock from this build (step 3 is optional; the update code is byte-identical to Step 0.7's).
Hardware touched: none (FM-1 unplugged)
Questions for the planning chat:
- Review docs/step-M1-install.md; decide optional step 3.
- Review docs/controls.md (proposal) and the format questions of the 23:16 entry.
- Cosmetic for step 1.6: the current bar's box dims a held-over chord like the following boxes do (Saints screenshot); the current box should probably stay bright.
- Flash: 484 KB of ~581 KB used with 5 songs; more songs before step 1.7 would need smaller events (delta coding, deferred) or the data area.
- I could not listen to anything (no audio here): the previews and the emulator audio are only checked by numbers.
Next: the planning chat and the user review docs/step-M1-install.md; then the user runs it (steps 1-2), pasting output.

2026-11-05 06:53 — Step M1 install (review received) —
Status: done (review applied); install waiting for the user
Did:
- Read HANDOFF 2026-11-05 07:00: M1 approved; install checklist approved as written; optional step 3 skipped.
- docs/step-M1-install.md: status set to APPROVED, step 3 marked SKIPPED. No other change.
- Arrangement licence: all five songs (and make_songs.py) already said `arrangement_license: TBD`; removed the
  "proposal: CC0-1.0" comment from songs/amazing-grace/song.yaml. Recompiled: byte-identical to
  build/songs/amazing-grace.fm1s (sha256 a1d7659e…cf901), so the reviewed package is unchanged.
Learned / measured:
- Package still build/step-M1/fm1acc-0.2.0-m1.fwsc, sha256 bf8f640f8137efc278fa17f6bcd7c21791eb1adbd68a7b7299c22f851804ff25.
- Noted for later: step 1.6 keeps the current bar's box bright for held chords; step 1.7 (songs in the data area,
  fm1song send, delta coding) is next priority after M1 feedback; v1.x option: voicings from harmony:1 (step 1.10).
Hardware touched: none
Questions for the planning chat: none
Next: the user runs step-M1-install.md steps 1–2, pasting each step's output; Claude Code reads (--info, lsusb,
kernel log) between steps and records.

2026-11-05 06:58 — Step M1 install, step 1 —
Status: done
Did:
- The user ran step-M1-install steps 1.1–1.4 (--info, sha256sum, install, --info) and reported FM-1_46000200.
- Claude Code read back (read-only): --info, lsusb, kernel log. docs/device.md "Currently installed" updated.
Learned / measured:
- --info: `FM-1_46000200  [running]  port: FM-1 Accompaniment:FM-1 Accompaniment MIDI 1 24:0`; lsusb 1209:0001.
- Kernel log, port 1-2, no -110 anywhere: prototype (1209:0001) disconnect 06:55:09 → update loader "Felucca
  Update" 1209:0002 at 06:55:10 → disconnect 06:56:03 (~53 s of loader, Step 0.7: ~55 s) → M1 enumerated 06:56:04
  (< 1 s). bcdDevice 3.d0 for both the prototype and M1.
Hardware touched: WRITE (M1 package to the app area via our update loader, run by the user); then read-only by Claude Code
Questions for the planning chat: none
Next: step 2, the on-unit checks (no command), user reports observations; Claude Code records them.

2026-11-05 — Step M1 install, step 2 (user's on-unit test) — review of the user's notes
Status: done (review); M2 list proposed, not started
Did:
- Read the user's M1 test notes (vault Inbox.md, read only; nothing written to the vault).
Learned / measured (user's results on the unit):
- Works: boot, song list, mutes, levels (K1–K4), loop song, CPU fine, sound fine.
- Crackle in Four Chords, which the user puts down to the melody's instrument.
- Not found: how to loop a section (SEQ twice → LOOP SECT is not discoverable).
- L1 vs L2 drums sound too alike in most songs: song design, later (D004 / step 1.10).
- Wants: tempo in BPM with a much wider range; tempo reset to the song's own on each song; 3 s splash with a temp
  logo (vault tmpLogo-sm.jpg, 400x453); louder count-in; no dimming of held chords in the bar strip (the note's
  sentence is cut off; read as "drop the dimming, the rolling strip is enough"); change key; OCT−/OCT+ shift the
  keys' octave; a rhythm cue next to the chord showing when to play at the chosen level.
Hardware touched: none (the user ran the unit)
Questions for the planning chat:
- OCT−/OCT+ for the keys: the rule "REC, OCT−, OCT+ stay free" (fm1-base's rescues) is about holding them at
  power-on. Using them after boot looks compatible, but changes the rule in docs/controls.md: please decide.
- Held-chord dimming: the user now wants it removed rather than "current box bright" (07:00 note). Confirm.
- Change key was Phase 2 ("transpose"); the user wants it in M2.
- Splash logo costs flash (~90 KB as raw RGB565 at ~200x226); with 484/581 KB used it needs compression or step 1.7.
Next: the planning chat and the user settle the M2 scope; then the M2 prompt.

2026-11-05 09:53 — M2 scope: the user's decisions (for the planning chat) —
Status: done (decisions recorded); M2 not started, the planning chat and the user write the M2 prompt
Did:
- Proposed an M2 list from the user's M1 notes (entry above); the user decided in the Claude Code chat (they took
  it for the planning chat). Decisions, in the user's words where given:
Decided by the user — IN M2:
  1. Tempo in BPM, not %, with a much wider range (proposal: about 30–250 BPM, 1 BPM a detent, faster when
     turned quickly).
  2. Tempo starts at the song's own BPM on every song (not kept from the previous song).
  3. Boot splash visible at least 3 s with the temporary logo (vault tmpLogo-sm.jpg). User: "feel free to really
     compress the logo image. its just a tmp placeholder." Keep it easy to replace.
  4. Louder count-in click.
  5. Section looping that can be found on screen, with a clear way to choose the section; plus start from a section.
  6. No dimming of held-over chords in the bar strip (confirms the reading of the user's note: the rolling strip is
     enough). Replaces the 07:00 note's "keep the current box bright".
  7. Four Chords crackle: check (instrument vs clipping) and fix, e.g. another melody instrument.
  8. Change key (transpose) live: whole band except drums, chord names re-spelled for the new key, ranges kept
     comfortable. Moved from Phase 2 into M2.
  9. OCT− / OCT+ shift the octave of the keys the user plays. The user approves using them after boot; the
     power-on rescue (holding them at start-up) must keep working. docs/controls.md's "REC, OCT−, OCT+ stay free"
     rule changes accordingly (REC stays free).
 10. Rhythm cue next to the chord: a note symbol showing when / how long to play at the chosen level (e.g. whole
     note at L1, blinking quarter at L2), shown also when the layer is muted.
 11. Songs stored outside the firmware, sent from the laptop (step 1.7). User: if this second song location works,
     ALL songs move out of the firmware, saving flash.
 12. Keyboard view (the current chord's keys) and the optional chart view (step 1.6).
 13. Pause / resume (continue from the same place instead of always from the top).
 15. Loop any bars (A–B loop), not only sections; designed together with item 5.
 18. Smaller song files (delta coding), with item 11.
Decided by the user — NOT in M2: 14 metronome click during playback; 16 speed trainer (tempo up a little each
  loop); 17 remember settings on SAVE; 19 HOME asks before stopping.
Later (not M2): L1 vs L2 drums too alike in most songs: song design, with the D004 experiment (step 1.10).
Learned / measured:
- Flash now 484 of ~581 KB; moving all songs out (~113 KB) leaves room for the compressed logo and new code.
Hardware touched: none
Questions for the planning chat:
- Proposal from Claude Code (not decided): split M2 like M1: M2a = items 1–7, 13, 15 + an on-unit test;
  M2b = items 8–12, 18 + an on-unit test. Twelve-plus items in one run is a lot of new screens and controls.
- Item 11 changes what is written to the unit (songs in the data area): the hardware rules and the install
  checklist for that write need your review as before.
Next: the user takes this entry to the planning chat; they write the M2 prompt.

2026-11-05 11:10 — Milestone M2, Part 1 (songs outside the firmware; items 11, 18) —
Status: done (emulator); committed; tested release build kept
Did:
- Read HANDOFF 10:10–10:40 and docs/plan-M2.md (one build, parts 1–5, early install point after part 2).
- Format 1.1: event streams delta-coded in `EZ` chunks (docs/song-format.md 2.2); `fm1song compile` writes 1.1
  (`--format 1.0` the M1 layout); the Python reader and the device parser (song.c) read both. The five songs: 113 KB
  -> 45 KB (40 %).
- The song store, firmware/app/store.c: 0x97000–0xDBFFF (69 sectors), one record per song (32-byte head written
  last, its own CRC; older copy erased after the new one is whole; FULL refused before anything is erased; every
  erase / write checked against the area). Laptop test tools/store_test.sh: ASan/UBSan, a simulated NOR flash, a
  power cut at every operation of add / replace / replace-with-no-room / delete: ALL PASS.
- firmware/app/library.c: the store on fm1-base's plat_data*, the list (by title), opening a song (record CRC +
  song_open, events decoded into a 64 KiB RAM buffer, checked at first use, never at boot), and USB commands
  LIST / SEND / DELETE (fm1-base APP_USB_CMD, the update's framing; the FM-1 pulls the file into RAM, checks CRC and
  the whole song, then writes). Songs no longer in the image (tools/songs_to_c.py removed).
- Laptop side: `tools/fm1song.sh list | send | delete` (tools/fm1song/device.py; finding the FM-1 and the framing
  from fm1-base's vendored tools/fm1_install.py). It talks only to identity FM-1_46...
- Emulator: tools/patches/fm1-emu-usb-host.patch (FM1_USB_HOST: a program plays the USB host in --bench runs).
  tools/m1_test.py became tools/m2_test.py: songs are sent over emulated USB first, every run starts from that flash.
- Screens: empty store -> "NO SONGS YET" + the send command; flash refused -> "NO SONG STORAGE"; a song that fails
  its full check stays listed as "SONG ERROR <reason> (send it again)".
Learned / measured:
- Stock firmware never uses 0x97000–0xDFFFF (our dump: all 0xFF there; its data at 0x93000 and 0xE9000–0xFBFFF), so
  the song area cannot disturb a return to stock.
- Image 376,480 B (M1 484 KB); RAM .data+.bss 73,620 of 98,304; pool 215,048 of 344,064.
- Emulator, release build build/step-M2/part1/fm1acc-0.2.1-m2p1.fwsc (identity FM-1_46000201, sha256
  6e883b90adbd918358effb6640bf2c577b28e110cceeecffb13b6bd4a11a8a32): 96 checks ALL PASS
  (build/step-M2/part1/report.md): all 5 songs sent in 2.7 s emulated (203 flash ops), stored byte for byte; list
  writes nothing; empty store, junk in the area, a damaged stored song (SONG ERROR, the others play), two damaged files
  sent unchecked (refused, nothing written), a stalled transfer (gave up, nothing written), power cuts at 6 points of
  a send (the other 4 songs intact every time; sent again: stored); flash ids 856014 / b36014 same; unknown: NO SONG
  STORAGE, keys sound, a send refused. Playback of every song from the store (v1.1 decoded): lengths within one
  block, chords on screen 81–99 ms before the sound, at most 14 voices / 14 % CPU.
- Fuzz (tools/song_fuzz.sh 600, seeds in 1.1 and 1.0): 10 minutes, 3,148,000 mutated files, 125,248 accepted, no ASan/UBSan report
  (build/fuzz/fuzz-m2-600s.log).
- Python tests: 65 pass (incl. the C/Python agreement on 3,000 mutated files of both versions).
Choices (with reasons):
- One record per song on whole sectors, head written last (not a FAT or a log of small blocks): the simplest layout
  that survives a cut at any point; 69 sectors hold 28+ songs of our size.
- The whole file into RAM before any write (64 KiB limit): the device checks the complete song, exactly as it will
  play it, before touching the flash. Our songs are 6–12 KB.
- A send stops the song playing and goes to the list: the file being played may be the one replaced.
- Decoding 1.1 events into RAM at open (not on the fly): the player stays unchanged (binary search, loops, variant
  switches). 8192 events at most, a format rule both readers check.
- The test host is a patch to fm1-live (FM1_USB_HOST), kept in tools/patches like the flash-id patch.
Hardware touched: none
Questions for the planning chat:
- `fm1song send` and `delete` write the FM-1's flash (the song area only). Add them to CLAUDE.md's list of commands
  only the user runs? (I treat them so already: run only against the emulator.)
Next: Part 2 (practice controls and fixes), then the early install checklist.

2026-11-05 12:00 — Milestone M2, Part 2 (practice controls and fixes; items 1–7, 9, 13, 15, B) —
Status: done (emulator); code committed (d13122a); tested release build kept; early install checklist ready
Did:
- docs/controls.md rewritten for M2 with mock-ups first (tools/mock_screens.py, docs/step-M2/mock-*.png), then built:
  - tempo in BPM, 30–250 (1 a click, 5 when turned quickly), amber when not the song's own; each song at its own;
  - start point by section (PRESETS) or bar (ALGORITHM); loops SECTION / BARS A–B / SONG (SEQ); a loop moved while
    playing (or one the music is outside of) is jumped to at the next bar line; pickup rules kept (song-format 3.3);
  - PLAY pauses / goes on (count-in on: a bar of clicks, then the paused bar from beat 1; off: from right there);
    HOME stops (back to the start point), HOME again: the list;
  - splash with the user's placeholder logo for >= 3 s (tools/logo_to_c.py: 150x170, 8 colours, run-length, 4.4 KB;
    the one file to replace: firmware/assets/tmp-logo.jpg);
  - count-in louder (127 on beat 1, 104 others, percussion channel at full volume during it);
  - beat light: PLAY's green LED flashes each beat (bright on beat 1, mid level otherwise), count-in too;
  - OCT- / OCT+ shift the keys' octave (-3..+3, lit when shifted; a held key is released at its own note) (D021);
  - GLO: the help screen (every control and what it does here; CPU / voices moved there); a legend on every screen;
  - no dimming of held-over chords in the bar strip.
- Item 7 (Four Chords crackle) measured: not clipping. Every song at L2, MASTER 100 %: peaks 41–59 % of full scale, 0
  samples at full scale (before and after). The melody alone on 8 GM instruments (tools/melody_check.py, emulator):
  GM 81 "Lead 1 (square)" put 3.80 % of its energy above 8 kHz and 1.40 % above 11 kHz (aliasing: EAS synthesises at
  22.05 kHz, our 2x upsampling is linear); flute 74, clarinet 72, oboe 69: 0.00 %; alto sax 66: 0.17 %; vibes 12:
  1.98 %. Fix: Four Chords' melody is now the clarinet (72). After: Four Chords peak 44 % (was 51 %).
- Player: position in units of 1/2,646,000 tick (a frame adds ppq x BPM: exact); player_start(tick, count-in),
  player_pause(); stats version 2/3 (start point, pauses, BPM, clicks, keys' octave, screen, beat-light flashes).
- Tests (tools/m2_test.py): boot (splash, the list at 3.3 s, PLAY ignored during the splash; OCT- + OCT+ held at
  power-on: fm1-base's panel set-up, our screens never start; REC held: the update mode), practice (start points,
  loop bars, a moved section loop, pause / go on both ways, HOME twice, own BPM per song, OCT up / down / clamp, beat
  light count, count-in vs M1), levels (every song), and every song at 30 and 250 BPM (no block hit the 128-events
  cap; CPU at most 15 %; position exact within 0.5 tick after 45 tempo changes).
- Early install point: docs/step-M2-part2-install.md (install over M1, the first song send explained: the area,
  a cut-off, how to undo; on-unit checks for parts 1–2; fallback M1).
Learned / measured:
- Image 386,144 B; RAM .data+.bss 74,100 of 98,304; pool 215,048 of 344,064.
- Release build build/step-M2/part2/fm1acc-0.2.2-m2p2.fwsc (identity FM-1_46000202, from d13122a, sha256
  b7d44547a463433a85fabfcd2faec3690bf12d781147461be0810ef5f1bd4683): 117 checks ALL PASS (build/step-M2/part2/report.md),
  run in its own folder (an earlier run was spoiled by a parallel dev run sharing the work folder: m2_test --out since).
- The splash's 3 s are counted from fm1-base's clock (starts after boot): the list appears at 3.0–3.2 s emulated.
- Count-in peak 2.1x M1's (362 vs 175, Amazing Grace, MASTER 100 %).
- Pausing or stopping mid-song leaves up to 2 EAS voices in their release for ~2 s (silent: rms < 30 after 0.5 s,
  0 after 3 s); no hanging notes. At the song's end: 0 voices after 1 s, as in M1.
Choices (with reasons):
- PLAY = pause, HOME = stop (then the list): one press each; "stop and back to the top" stays one press.
- PRESETS (left) = start / A, ALGORITHM (right) = bar / B: the two knobs side by side, left = start.
- Loop BARS starts 4 bars long at the start point; A..B have no pickup (a section's does).
- Resume with count-in from the paused bar's beat 1 (musicians re-enter at a bar line); without, exactly where it was.
- The help is GLO, a press (any button closes it): reachable from every screen.
- Part builds carry their own version string (0.2.N-m2pN): the splash and the list show what is installed.
Hardware touched: none
Questions for the planning chat:
- The early checklist (docs/step-M2-part2-install.md) is ready for review; the user may install it before parts 3–5.
- `fm1song list` sends a SysEx question (writes nothing). CLAUDE.md forbids Claude Code sending SysEx, so the
  checklist has the user run it too. Allow it as read-only, like `--info`?
Next: Part 3 (key, views, rhythm cue, bass names, key lights).

2026-11-05 12:30 — Milestone M2, Parts 3 and 4 (views and guidance: items 8, 10, 12, F, G; three songs: E) —
Status: done (emulator); committed; tested release build kept
Did (part 3; docs/controls.md "Part 3", mock-ups first):
- Key (item 8): SAVE starts the KEY mode, SELECT changes the key through all twelve, the music moving the nearest way
  (-5..+6); the player transposes every layer but the percussion channel's as each note is sent, and a change while
  playing releases exactly the sounding notes. Chord names, the key name and bass names are spelled for the new key's
  signature (its mode's parent major: flats for F Bb Eb Ab Db, sharps otherwise, C mixed). Each song opens in its key.
- Views (item 12, EDIT): chord view; keyboard view (the harmony level's voicing on C3–B5, the next chord outlined in
  the last bar before it, the bass note amber when the bass is muted); chart view (the section's bars, the bar now
  framed). The chord is still drawn ahead of its sound in every view.
- Rhythm cue (item 10): a note symbol left of the big chord, from the part's own events at the chosen level (how long
  its attack is held: whole .. sixteenth), bright at each attack.
- Bass names (F): the bass muted, the status line says `BASS C3  then G2`.
- Key lights (G): the FM-1's keys light for the part played (harmony: the chord's voicing, the next chord dim in the
  last bar before it; bass: its note), folded into F3–G5 and following OCT-/OCT+; pressed keys light as ever.
  PROPOSAL: both harmony and bass muted -> harmony wins (one rule for the cue and the lights, nothing to set); the
  bass stays named on the status line.
Did (part 4): songs/make_songs.py: Greensleeves (A minor 3/4, 88 BPM), Londonderry Air (Danny Boy) (C, 66 BPM, slash
  chords C/E C/G D7/F# G/F), Oh! Susanna (D, 150 BPM); our own simplified melodies of public-domain tunes, no lyrics,
  marked "check before publishing" (typed from memory); previews; songs/README.md. Their drums: level 1 just the
  pulse, level 2 a full groove (drum hits per bar x2.2–3.6, 7 sounds instead of 3; the M1 songs: x1.5–1.8).
Learned / measured: Release build build/step-M2/part3/fm1acc-0.2.3-m2p3.fwsc (FM-1_46000203, from 6d7893d + songs c6b121d, sha256
  bf504efb543cfb8ae5cfd38fb3e19c829476dc556edea3f844dbc742da495324): 175 checks with all 8 songs; one test-side failure
  fixed (Danny Boy's 6.4 s count-in: the sound check now listens after it), then ALL PASS. Image 395,232 B; RAM
  74,276 B; pool unchanged. The new songs: 10.4 / 8.6 / 13.8 KB; at most 13 voices, 13 % CPU.
Hardware touched: none
Questions for the planning chat:
- Both harmony and bass muted: harmony wins (proposal above). Or a control to choose?
- The three new melodies are from memory: worth a check against a public-domain score before anything is published.
Next: Part 5 (tests of everything, budget, 0.3.0-m2, docs/step-M2-install.md, STOP).

2026-11-05 12:40 — Milestone M2, Part 5 (tests, budget, install checklist) —
Status: done; STOPPED for review (nothing installed, nothing sent; the FM-1 was not touched)
Did:
- Version 0.3.0-m2, release build build/step-M2/final/fm1acc-0.3.0-m2.fwsc (identity FM-1_46000300, from commit
  2e470ed, sha256 0294089be99c6e07aeaecba8aedc749097642aa8522b48ff597ff1c6177ac52d).
- tools/m2_test.py on that exact package: 175 checks, ALL PASS (build/step-M2/final/report.md): every item and all
  8 songs, flash ids 856014 / b36014 / unknown, a corrupt stored song, damaged files sent, an empty store, junk in
  the area, a stalled transfer, power cuts during a send. tools/store_test.sh: ALL PASS. pytest: 71 pass.
- docs/step-M2-emulator-report.md: results per item and per song, the budget (image 395,232 B of ~581 KB; RAM 74,276
  of 98,304; pool 215,048 of 344,064; the 8 songs 77,888 B in 22 of the store's 69 sectors; CPU <= 15 %, 14 voices),
  screens in docs/step-M2/.
- docs/step-M2-install.md: install M2 over M1 (or over 0.2.2-m2p2 if the early checklist was run), the first send of
  all 8 songs (the new kind of write, explained: area, cut-off, undo), on-unit checks for every item, fallback M1.
Hardware touched: none
Questions for the planning chat:
- Review docs/step-M2-install.md (and the early docs/step-M2-part2-install.md: one of the two is enough).
- Both harmony and bass muted: harmony wins for the cue and key lights (proposal), or a control?
- fm1song send / delete (write the song area) and list (a SysEx question): add to CLAUDE.md's rules (user-run /
  read-only)?
- The three new melodies are from memory: check them against public-domain scores before publishing.
Next: STOP. The user and the planning chat review; then the user runs the checklist.

2026-11-05 17:55 — Milestone M3, Part 1 (bugs and housekeeping) —
Status: done (emulator), except the CLAUDE.md rule text (blocked, below); committed; tested release build kept
Did:
- 1. **No L1 drums on Greensleeves and Oh! Susanna.** The player was fine: the emulator played their level 1 (drums
  alone: rms 419 / 600, as loud as Amazing Grace's 454). The cause was the song data. M2's new level 1 for these
  songs was a kick and a soft hi-hat, with no snare (Greensleeves: pedal hi-hat, velocity 44–56), so it was lost under
  the band. Fix in songs/make_songs.py: level 1 is now the plain beat with a backbeat (4/4: kick on 1 and 3, snare on
  2 and 4; 3/4: kick on 1, light snare on 2 and 3; closed hi-hat on every beat). Level 2 is unchanged (busier).
  Danny Boy had the same level 1 and gets the same fix.
- 2. **Loops.**
  - Several A marks: the strip repeats a loop's bars after its end and drew A on every copy. Marks are now drawn only
    on the bars in song order.
  - "START BAR" left on with the loop off: setting A (PRESETS) and starting LOOP BARS moved the start point. They
    no longer do, so with the loop off the start point is the one set before.
  - Part 3 redesigns loops.
- 4. **Count-in in the keyboard and chart views:** the count (big, amber) and the first chord replace the view
  while it counts in.
- 5. **Beat light:** a full flash on every beat, 220 ms on beat 1 and 110 ms on the others. M2's mid level for
  beats 2.. was not visible on the real LED.
- 6. **Housekeeping:**
  - docs/step-M2-install.md step 2: `songs/*/`, with a dated note. Also songs/README.md, controls.md and the
    empty-list screen.
  - docs/step-M2-part2-install.md marked SUPERSEDED (HANDOFF 12:50).
  - `fm1song send` skips anything that is not a song folder (song.yaml + song.mid) or a .fm1s file, with
    "PATH: skipped (...)". With nothing left to send it exits 2 before looking for an FM-1. New pytest test.
  - 3 (bass muted shows the harmony): in Part 2, as planned.
- New tools/m3_test.py, grown from m2_test.py:
  - `--only` takes a list;
  - each part's checks are a group (part1, ...).
  - The full suite runs at the early install point and at the end, as planned.
Learned / measured:
- Part 1 checks (tools/m3_test.py --only part1): ALL PASS.
  - Drums alone at L1: Greensleeves rms 691, Oh! Susanna 1056 (Amazing Grace 454).
  - Beat light: 4 beat-1 and 9 other flashes in 8 s at 96 BPM.
  - Loop off after LOOP BARS: PLAY at the earlier start (bar 3).
  - Count-in drawn over the keyboard view.
- pytest 72 pass; tools/store_test.sh ALL PASS.
- Release build build/step-M3/part1/fm1acc-0.3.1-m3p1.fwsc: identity FM-1_46000301, sha256
  ddb63c4f1ba89f53a70eddfca81a7454425310be6ec171f0333c13be5003d877, Part 1 checks ALL PASS on that file. Image
  395,472 B.
- `firmware/build.sh --release X.Y.Z` writes `fm1acc-X.Y.Z.fwsc`; plain `fm1acc.fwsc` stays the dev build
  (FM-1_4600). Copy the versioned file.
Choices (with reasons):
- Part builds 0.3.N-m3pN (identity FM-1_460003NN); the whole M3 will be 0.4.0-m3. Same scheme as M2.
Hardware touched: none
Questions for the planning chat:
- **CLAUDE.md not updated:** Claude Code's permission check refused the edit (self-modification). Please make the
  change or allow it. Proposed text:
  - In the "NEVER run" list, after "any web installer": "and `tools/fm1song.sh send` and `delete` (they write the
    FM-1's song area; the user runs them)".
  - In the read-only list: "`tools/fm1song.sh list` (a SysEx question that writes nothing)".
  Until then Claude Code keeps treating all three as user-run.
Next: Part 2 ("follow your part" for all four layers, blinking in rhythm, the cue on the right).

2026-11-05 18:35 — Milestone M3, Part 2 ("follow your part", blinking in rhythm) —
Status: done (emulator); committed; tested release build kept
Did (docs/controls.md "Part 2"; mock-ups first: tools/mock_m3.py → docs/step-M3/mock-p2-*.png):
- **Your part** is the muted layer. The top line (`BASS - YOURS`), the layers row (YOURS, double frame), the big name,
  the next name, the strip, the keyboard and chart views, the rhythm cue and the key lights all follow it:
  - harmony: chords, as before;
  - bass: the note with its octave, the next note, the chord; the bars' bass notes;
  - melody: the same for the melody;
  - drums: the hit now and next (KICK, SNARE, + HAT); each bar as a small grid (kick, snare, cymbals, others). No
    keyboard view for drums (EDIT skips it) and no key lights.
  This fixes Part 1 item 3 (bass muted, the screen showed the harmony).
- **Several muted:** the one muted last is followed; unmute it and the one muted before takes over. M2's
  harmony-wins rule is gone. Each part remembers its last view.
- **Key lights blink in your part's rhythm, at its level:** bright for 140 ms at each attack, middle brightness
  between (fm1-base plat_lights_mid). The harmony's next chord stays dim in the last bar before it. The keyboard
  view does the same in colour (bright amber, then darker amber).
- **Rhythm cue** always shown, right of the big name, half M2's height (note head 11 x 9, stem 24), no level label.
- Player stats version 4 (part, key_flashes, mid_keys) for the tests.
Learned / measured:
- tools/m3_test.py --only part1,part2 on the release build: ALL PASS (13 checks).
  - Harmony muted: Amazing Grace L1 3 flashes for 3 attacks, L2 7 for 7.
  - Bass muted (Danny Boy): one key lit, the bass note now.
  - Melody muted: 8 flashes for 8 notes.
  - Drums muted: no key lights, EDIT goes to the chart.
  - Harmony then bass muted: bass followed; bass on again: harmony.
  - None muted: keys dark, the cue shown.
- The full per-song run on Amazing Grace and Danny Boy: every chord on screen 81–99 ms before its sound (the new
  drawing code keeps the timing).
- Release build build/step-M3/part2/fm1acc-0.3.2-m3p2.fwsc: FM-1_46000302, sha256
  cdb0c7f09c5333d22fa1810787a1778b64c7dab9abed6fe88f8a1f4b816301f6. Image 398,172 B (+2.7 KB).
Choices (with reasons):
- Several muted: the last muted is your part. It needs no new control, it is visible (YOURS), and it is easy to
  change (mute the one you want last). Recorded for the planning chat.
- "Darker between attacks" uses fm1-base's middle LED level for the keys. M2's middle level was not visible as a
  110 ms PLAY flash; held between attacks it may read better. If not, the user can turn blinking off (Part 4
  setting) for steady keys.
- Bass notes below C3 are drawn an octave (or more) up in the keyboard view, and it says so. Part 4's zoomed
  keyboard follows the notes instead.
Hardware touched: none
Questions for the planning chat:
- "Last muted is your part": OK, or do you want an explicit control?
Next: Part 3 (loops redesigned: bars A–B only, set and confirm, highlighted bars).

2026-11-05 19:05 — Milestone M3, Part 3 (loops redesigned) —
Status: done (emulator); committed; tested release build kept
Did (docs/controls.md "Part 3"; mock-ups first: docs/step-M3/mock-p3-*.png):
- One kind of loop: bars A–B. The section and whole-song loops are gone.
- Set and confirm with SEQ: set A, set B, then the loop is on.
  - ALGORITHM moves the mark by a bar; PRESETS jumps it to a section's edge.
  - HOME steps back (B → A → cancelled).
  - SEQ with the loop on turns it off.
  - The top-line badge "LOOP: SET A bar 5", the status line and the legend show each step.
- The loop's bars are filled on the strip and the chart, with one A and one B; the bar being moved is framed white.
  This fixes the root cause of Part 1 item 2, as Part 1 started: the marks belong to bars in song order.
- PRESETS / ALGORITHM still pick the start point with the loop off. With the loop on, PLAY starts at A.
Learned / measured:
- tools/m3_test.py --only part1,part2,part3 on the release build: ALL PASS (19 checks).
  - Part 3: set stopped (A bar 3, B bar 5; starts at A; jumps 9600 → 3840).
  - Not active while being set; HOME back and cancel; SEQ off.
  - PRESETS to section edges (Saints: A 17, B 32).
  - Set while playing: bars 2–5, 2 jumps.
- Release build build/step-M3/part3/fm1acc-0.3.3-m3p3.fwsc: FM-1_46000303, sha256
  859752445f1095736804e046bb5f46d79238cd1e4fc82008b4e30264e33cfff9. Image 399,664 B.
Choices (with reasons):
- No separate whole-song loop: A at the first bar and B at the last is the same thing, with two PRESETS turns.
  One loop type is what the user asked for.
- No pickup on A: a loop starts on a bar line, as M2's A–B.
- With the loop on, PRESETS / ALGORITHM do not edit it (they say "SEQ: OFF FIRST"). Editing means clear and set
  again, so the confirm steps are the only way to change it.
Hardware touched: none
Questions for the planning chat: none
Next: Part 4 (legend colours, no section row, zoomed keyboard, count-in in every view, beep, inversion label, settings).
