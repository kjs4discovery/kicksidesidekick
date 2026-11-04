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
