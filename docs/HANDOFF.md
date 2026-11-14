# HANDOFF

Written by the planning chat and the user. Claude Code reads this at the start
of every session and before every step, and never edits it except to tick a
checkbox the note asks it to tick.

---

*(Notes before Milestone M5: docs/archive/HANDOFF-before-M5.md.)*

---

## 2026-11-06 14:15 — from the planning chat — Milestone M5 planned: docs/plan-M5.md

The user tested M4 (notes in the vault). M5, in PRIORITY order with two early install points:
Part 1 fixes (level loudness, Let It Be L3, arpeggio key colours, bass tab labels + fingering,
layout overlap, track box spacing, fresh start per song, longer splash + start-up melody,
controls hint pop-up); Part 2 storage (D029: generate levels on the FM-1, play only the chosen
level, compact format 2.0, all songs back, private songs whole); Part 3 mode select + SYNTH
mode (D030, Felucca-Engines MIT only); Part 4 views (ukulele option, melody guitar/ukulele tab,
harmony bar view, zoom, rhythm icon on chord boxes); Part 5 tests + checklist. The user's
weekly usage is limited: respect "finish the current part, then stop".

---

## 2026-11-06 14:30 — from the planning chat — added to M5 Part 1: bass register

The generated bass is an octave too high (E2..D#3 roots); write it in the real octave, with a
setting "Bass sound: REAL / +1 OCTAVE (speaker)" (default +1 OCTAVE), tab = real notes,
bass-clef notation written an octave up (bass guitar convention). Details in docs/plan-M5.md.

---

## 2026-11-06 17:15 — from the planning chat — usage is nearly spent: trim Part 3, stop after it

The user is at ~90 % of their weekly usage and wants synth mode in. So:
1. Finish Part 2 and write docs/step-M5-early-install-1.md as planned.
2. Part 3, TRIMMED to a simple test: the mode screen (LEARN / SYNTH, mascot) and SYNTH mode
   with the ANALOG engine only (PHASE later), 8 voices, our own chorus + delay (reverb later),
   a simple arpeggiator (up / down / up-down, tempo rate), OCT, knobs (cutoff, resonance,
   attack, release, chorus, delay), SELECT for presets, **20 of our own synthwave presets**.
3. Write docs/step-M5-early-install-2.md (full suite on that build) and **STOP**. Do NOT start
   Part 4 (views) or Part 5; they move to the next milestone.
Keep LOG entries short to save usage.

---

## 2026-11-06 21:45 — from the planning chat — docs/step-M5-early-install-2.md APPROVED

Approved as written (supersedes -1). Superb result: 9 songs 168 KB -> 11 KB, all 14 fit,
private songs whole, golden tests identical. Parts 4-5 go to the next milestone.

---

## 2026-11-06 21:50 — from the planning chat — INCIDENT: M5 build crashed during song sends -> SAFE MODE

The user installed 0.5.3-m5p3 (FM-1_46000503). Sending songs failed ("the FM-1 was
disconnected" twice, then "stopped answering"); the unit then failed to start and is in
fm1-base's SAFE MODE. Likely: receive-to-flash / compaction in the new 512-byte block store,
then a crash at start-up on the half-written store. The emulator suite did not catch it.
1. READ-ONLY now: --info, kernel log, the crash record (plat_last_crash) if readable in
   safe mode. No install, send or delete. Record everything in LOG (time, outputs).
2. Give the user the exact M4 reinstall command (P7 fallback) from safe mode; record its result.
3. NEXT milestone (not now): find and fix the root cause; add a hardware-like test (the real
   USB timing / flash erase timing differ from the emulator); make start-up never depend on the
   store's state (scan lazily, and recover from any store content); consider a "store wipe"
   recovery action. Don't reinstall M5 until then.

---

## 2026-11-09 — from the planning chat — M5.1: fix the song-send crash (docs/plan-M5.1.md)

The user will start M5.1 with a short prompt. New: a Sonnet test-runner subagent at
.claude/agents/test-runner.md (written by the planning chat; commit it). Use it for full-suite
runs and long test output; keep design/debugging in the main session. Token saving: at the start
of each milestone the user may /clear; everything you need is in CLAUDE.md, this file, the plan
and LOG. Please also move HANDOFF notes and LOG entries older than M5 into docs/archive/ (keep
the live files short), as part of M5.1.

---

## 2026-11-13 — from the planning chat — Milestone M9 planned: docs/plan-M9.md

The user installed M8 (0.8.0-m8): all fine; the M8 notes are in the vault. M9, in priority order: count-in bugs
(test first), small fixes (the "All songs" count, mode screen text, LFO light, importer time/key signature), band
complexity tiers for backtracks (a table file, C = Python by generation), one knob map for BACKTRACK SETUP and PLAY
with changes on the phrase, 3 renamed + 6 new minor progressions. Saving presets and sound kits are M9.1 (not now).
**The coder for M9 is Sonnet:** the plan is prescriptive; every judgment call it does not spell out goes into
docs/decisions-M9.md (format in the plan) for the planning chat's review.


---

## 2026-11-13 — from the planning chat — Milestone M10 planned: docs/plan-M10.md

The user installed M9 (0.9.0-m9) and ran docs/step-M9-install.md steps 1-5: all fine (3 renamed backtracks deleted,
36 sent). M10, in priority order: M9-review fixes (M9-07 reversed: crash only; M9-17 changed: a change while paused
keeps the pause), BACKTRACK screens (value pop-up with titles, SIMPLE/MEDIUM/BUSY everywhere, NEXT spelled out,
"MELODY SOUND", the section indicator that blinks in the bar before a change), the mode screen title "Notami - Song
Sidekick", 6 progressions (42), a song info line on the Settings screen (INFO chunk), BACKTRACK band kits (SEQ on
SETUP), then saving user presets (D040, own store object at 0xDE000), docs + NOTICE, the full suite once.
**The 12 new songs were written by the planning chat** (songs/m10_songs.py + the 12 folders, uncommitted in the
working tree): commit them as they are; never change their notes. songs/hallelujah/ moves to songs/private/.
The coder is Sonnet again: every judgment call goes into docs/decisions-M10.md. One early install point after Part 6
(do not stop there). The full suite runs once on the final package (a beta candidate; M9-21 is suspended for M10 unless the user says otherwise in the prompt).

## 2026-11-13 (later) — from the planning chat — M10 amended before the run

The product name is now **Tuneka** (it replaces Notami; plan-M10.md Part 3). The user dropped 6 of the planning chat's
12 songs after listening: M10 has **6 new songs** (camptown-races, clementine, auld-lang-syne, drunken-sailor,
blow-the-man-down, michael-row); the other 6 stay in songs/m10_songs.py's DROPPED_SONGS, never generated or sent.
The full suite in Part 9 is trimmed and runs in three lanes (plan-M10.md Part 9).

Correction (the user, later): "Tuneka - Song Sidekick" (two words, Song and Sidekick) on every screen, as plan-M10.md says.
