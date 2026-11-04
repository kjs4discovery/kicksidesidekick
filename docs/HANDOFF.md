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
