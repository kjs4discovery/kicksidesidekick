# HANDOFF

Written by the planning chat and the user. Claude Code reads this at the start
of every session and before every step, and never edits it except to tick a
checkbox the note asks it to tick.

---

## 2026-11-04 14:45 — from the planning chat — after Step 0.1

Step 0.1 reviewed: looks good. Thanks for the clear log.

### Answers to your questions
1. **Rust:** only used to build laptop-side tools (the fm1-emu emulator's native
   build, and possibly M-Flasher in step 0.3). Our firmware will be **C** on the
   FM-1's JieLi pi32v2 CPU, built with whatever toolchain the chosen base uses
   (decided in step 0.5). **Do not add any rustup embedded targets** — none apply.
2. **Stock version and flash chip:**
   - Stock firmware version → **Step 0.2** (read-only identity query; the user
     will paste the prompt next).
   - Flash chip ID + full flash backup → **Step 0.3**, and in 0.3 you only
     *prepare* the procedure; the user runs every device command.

### Notes for Step 0.2
- The unit is on factory firmware; version unknown (could be V14, V15 or V16).
  If the identity tool doesn't recognise the version, record exactly what it
  printed and stop — don't try another tool that sends anything else.
- If any tool's "info"/setup path would download firmware, explain first. Saving
  stock packages to disk for safekeeping is fine; sending them anywhere is not.
- Small fix: your first LOG entry says "nothing committed yet", but there is a
  commit (c071c5b). No action needed beyond being accurate in future entries.

### Current step
- [ ] Step 0.2 — Identify the unit (read-only)

---

## 2026-11-04 14:58 — from the planning chat — FYI, no action needed during Step 0.2

The planning chat added a **draft test song** at `songs/amazing-grace/`:
`song.yaml` (lead sheet + metadata, draft source format v0.1), `song.mid`
(9 tracks: conductor, chords, melody, harmony:1/2, bass:1/2, drums:1/2),
`make_amazing_grace.py` (regenerates song.mid with the standard library only;
`--preview` needs numpy + ffmpeg), and `previews/*.mp3`.

- Don't modify these files; they're a fixture for Steps 1.1–1.3. The format is a
  draft and may change.
- When you next commit, include them (the mp3 previews are ~0.9 MB each; fine to
  commit, or add `songs/*/previews/` to .gitignore if you prefer — note which in LOG).
- Finish Step 0.2 first; this doesn't change it.

---

## 2026-11-04 15:02 — from the planning chat — after Step 0.2

Step 0.2 reviewed: excellent work, especially reading the whole --info path and
recovering the pinned V15 package from the Wayback Machine.

### Answers
1. **V16 on M-VAVE's site:** nothing in the plan assumes V16. The unit stays on
   **FM-1_015** (D012). V15 is actually the best starting point: most community
   tooling and return-to-stock steps are built around it. Keep FM-1_016.fwsc on
   disk for reference only; **never propose installing it** unless the user asks.
   Our "restore to stock" target is `backups/stock-firmware/FM-1_015.fwsc`.
2. **V14:** not needed. Unit is V15.

### Notes for Step 0.3
- Version is settled; the flash chip is still unknown. The unit was bought new
  in late October / early November 2026, so treat 0xB36014 as quite likely.
- If the chip is 0xB36014: record it, do NOT propose any further step (no backup
  attempt with a tool that doesn't explicitly support it), and stop for the
  planning chat.
- Allow rules like the one in .claude/settings.local.json must stay limited to
  exact read-only commands. Never add a rule that matches a write.

### Current step
- [x] Step 0.2 — Identify the unit (read-only) — done, FM-1_015
- [ ] Step 0.3 — Flash chip ID + full flash backup (prepare only; user runs device commands)

---

## 2026-11-04 15:06 — from the planning chat — decision D014 (no action during Step 0.3)

The user accepted **D014**: chord symbols live in `song.yaml` (the chord chart),
next to `song.mid`, not in a MIDI "chords" track. A song is a folder:
`song.yaml` (metadata, rights, key, tempo, pickup_beats, form, layers, chart) +
`song.mid` (notes only). The sample in `songs/amazing-grace/` follows this; its
`chords` MIDI track is now just an optional cross-check.

- After Step 0.3 is finished (not during it), update the Project rules in
  CLAUDE.md to: "Compiler input: a song folder with song.mid (tracks drums, bass,
  harmony, melody, optional variants like bass:1, bass:2) and song.yaml (metadata
  and the chord chart; D014). See songs/amazing-grace/." Note it in LOG.

---

## 2026-11-04 15:45 — from the planning chat — after Step 0.3

Step 0.3 reviewed: very good. Flash ID 0x856014 (Puya), chip key 0x980F, full
backup confirmed by 4 identical reads in 2 sessions. The B36014 stop rule no
longer applies.

### Answers
1. **Why the firmware region differs from the V15 package:** almost certainly
   per-chip encryption, not a problem. m-flasher's docs/safety.md says a package's
   firmware "must decrypt with this unit's chip key", and its Analysis decrypts
   flash with CHIP_KEY 0x980F. The on-flash firmware is encrypted with the chip
   key, so it was never going to match the package bytes. (Planning chat's reading
   of the source; not yet confirmed by decrypting our dump.)
2. **Restore plan (proposed as D015, pending the user):**
   - *Normal return to stock* = install `backups/stock-firmware/FM-1_015.fwsc`
     through the normal update path. That's what the community and m-flasher do
     with official .fwsc files; the loader handles the encryption.
   - *Last resort / bit-exact* = the full dump, written via ROM boot mode
     (restores calibration, Bluetooth address and settings exactly).
   Keep both. Update the 15:02 note's "restore target" wording accordingly in
   docs/device.md (add a short "Restore options" section).
3. **M-Flasher hardware cross-check:** not needed; skip it. It would mean another
   ROM boot session for no new information. Optional, offline only (zero hardware):
   if it's easy, build a tiny program against m-flasher's `boards::Analysis` that
   reads our dump file and reports what it identifies (board, firmware version,
   regions). Only if it takes under ~30 minutes; otherwise skip and note it.
4. **udev rule:** fine as installed (4c4a:8057 only).
5. **Previews:** committing them is fine.

### Notes to carry into Step 0.7 (first writes)
- "One loader command per ROM boot session": write it into the 0.7 checklist.
- The repeated -110 descriptor timeouts and 15–20 s enumeration on USB port 1-2:
  before any write, test other USB ports (and another data cable) with read-only
  `lsusb`/`--info` until we find a port that enumerates cleanly. Record it in
  docs/device.md. Don't do any write on a port that shows -110 errors.

### Next
- After this: do the CLAUDE.md D014 update (15:06 note), then **Step 0.4** (emulator).
- During 0.4–0.6 the FM-1 is not needed: the user will unplug it.

### Current step
- [x] Step 0.3 — done (0x856014, backup OK). Off-laptop copies: pending on the user.
- [ ] Step 0.4 — Emulator on Mint
