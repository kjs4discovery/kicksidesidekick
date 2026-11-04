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
