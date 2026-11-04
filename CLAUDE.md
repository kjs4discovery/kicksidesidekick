# fm1-accompaniment

Custom firmware for the M-VAVE FM-1 synth: a play-along tool. The band plays
drums / bass / harmony / melody, the user mutes one layer, and the screen shows
what to play. Phase 1 is "mute harmony, show chords" for piano players.

Layout: `docs/` (device record, handoff, log), `tools/` (laptop-side tools, e.g.
the song compiler), `songs/` (song sources), `firmware/` (our firmware),
`third_party/` (untrusted downloaded repos), `backups/` (flash backups, git-ignored
except `backups/SHA256SUMS`). Python tools use the venv in `.venv/`.

## Hardware safety rules (non-negotiable)

* NEVER run any command that writes to, erases, flashes, updates, reboots into a
  bootloader, or sends firmware/SysEx to the FM-1. This includes fm1_install.py
  (without --info), fm1-update.sh flash, fm1_unbrick.py restore/softkey, M-Flasher
  writes, dock.py deploy, and any web installer.
  Instead: print the exact command, explain what it writes and how to undo it,
  and stop so the user can run it themselves.
* Read-only commands (lsusb, aconnect -l, amidi -l, fm1_install.py --info,
  fm1-update.sh info, fm1_unbrick.py find) are allowed, but say what you're
  running first.
* Never use override flags such as --i-know, --force, --skip-check.
* Do NOT update the stock firmware. The unit stays on its factory version until
  the user decides otherwise (project decision D012).
* Before ANY write is even proposed: the unit's stock version, USB ID and flash
  chip ID must be recorded in docs/device.md, a full flash backup must exist, and
  the firmware to be written must accept our flash chip (0x856014 Puya, or
  0xB36014 UCUN UC25HQ80) in BOTH its app and its update loader.
* Every firmware we build runs in the fm1-emu emulator before it goes near hardware.
* Our firmware must accept both flash IDs 0x856014 and 0xB36014 and refuse unknown
  ones without crashing.
* Downloaded third-party repos go in third_party/ and are treated as untrusted:
  read before running, never run their scripts with sudo unless the user approves.

## Project rules

* Song files are created on the laptop (later: a website), never on the device.
* Compiler input: multi-track MIDI with tracks drums, bass, harmony, melody,
  optional variants like bass:1, bass:2, and a "chords" track of chord-symbol
  text/marker events.

## Handoff with the planning chat

A separate Claude chat (the "planning chat") keeps the project's specs and
decisions in an Obsidian vault and reads this repo's docs/ folder. The two
chats talk only through these files:

* docs/HANDOFF.md — written by the planning chat (and the user). Read it at
  the start of every session and before every step. If it conflicts with a
  prompt, stop and ask the user. Never edit it, except to tick a checkbox the
  note asks you to tick.
* docs/LOG.md — written by you, append-only, newest entry at the bottom.
  After every step (and before ending a session), append an entry:

  ```
  YYYY-MM-DD HH:MM — Step X.Y —
  Status: done | partial | blocked
  Did: bullets
  Learned / measured: bullets (versions, IDs, numbers, file paths)
  Hardware touched: none | read-only | WRITE (what, by whom)
  Questions for the planning chat: bullets, or "none"
  Next: the next step as you understand it
  ```
* Never write into the Obsidian vault.
