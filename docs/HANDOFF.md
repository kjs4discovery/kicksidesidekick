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

---

## 2026-11-14 — from the planning chat — M10 review: the song files re-sent (M10-01), camptown-races fixed (M10-03)

- **M10-01:** the planning chat's original songs/m10_songs.py is back (with DROPPED_SONGS), songs/make_songs.py's hook
  imports NEW_STYLE_OF from it, and the six song.yaml files were regenerated with make_songs.py: only their texts changed
  (credits, rights, info, chart layout), plus camptown-races below. Not committed. Delete tools/recover_m10_songs.py.
- **M10-03, camptown-races (the song changed, not a check):** (1) the 4-note attacks were its last bar: two chords
  (A7, D), and the held first chord of the final bar is counted by m4p4 / m5p1 (only the last chord is left out). Its
  ending is now "| D |". (2) The harmony loudness 1.64: the banjo (GM 106) decays fast, so the held L1 was too quiet; the
  harmony is now accordion (GM 22), as Clementine and Drunken Sailor (both pass). song.mid unchanged.
- To do: commit; HELD_BACK = {}; on the final package (no firmware change) run m5 --only m4p4,m5p1,part5 (part5's count
  becomes 15) and camptown-races' own playback group; if they pass, put camptown-races back in docs/step-M10-install.md's
  send list (six new songs) and in docs/step-M10-early-install.md; record in LOG. If a check still fails: record, keep it
  held back, do not relax.
- Reviewed and accepted: M10-02, M10-04 .. M10-14. M10-14's list-index shift in BACKTRACK K4 after a new save goes to the
  backlog. M10-15: m5p1's hint count (9 of 10) also fails on M9: not an M10 regression; it stays open for a later milestone.

---

## 2026-11-14 (evening) — from the planning chat — Milestone M10.1 planned: docs/plan-M10.1.md

The user installed 0.10.0-m10 and used it (saved a preset ZUNJET in SYNTH). M10.1 = their M10 notes, in parts:
screens and text (name "KickSide - Your Song SideKick", BACKTRACK first on the mode screen, 6 s hint pages, LOOP in
BACKTRACK's loop structure, the value pop-up in SONG LEARN and SYNTH); buttons (layer button: first press = your
part, next presses = volume, no hold; HOME single presses climb to the mode screen; SONG LEARN K4 = melody sound);
a READ-ONLY preset read-out over USB plus a factory reset; preset order, E-GUITAR, a new EDS (fuzz bass); five
public song fixes plus Creep (exact notes in the plan, type them literally); a clean-install script and
songs/library.txt. HAZE PAD -> ZUNJET waits for M10.2 (from the read-out). The coder is Sonnet: judgment calls go
into docs/decisions-M10.1.md. Light testing by the user's choice: per-part checks and one fast lane (Part 8), no
full suite. `fm1song.sh presets` (and `--save`) are approved as read-only from Part 3 on; factory-reset and
tools/fresh_install.sh write and are run by the user only.

Correction (the user, 2026-11-14 16:56; docs/plan-M10.1.md is updated): the mode screen order stays as it is (SONG
LEARN first); the tagline is "KickSide - Your Musical SideKick"; new: SYNTH's edit page titles name the preset
("FX 2 - POTATO"); Creep: only bars 8, 16, 40 change, bars 24 and 48 stay "Csus4 Cm7".

Correction 2 (the user, 2026-11-14 17:04; docs/plan-M10.1.md is updated): NO preset read-out in M10.1. ZUNJET replaces
HAZE PAD in this build, from the values the user read off the unit (the exact table row is in the plan, Part 4).
Part 3 is now the factory reset only; the clean-install script warns before erasing presets instead of saving them.

---

## 2026-11-14 (late) — from the planning chat — Milestone M10.2 planned: docs/plan-M10.2.md

The user installed 0.10.2-m10.1 and used it; their notes are in the vault. M10.2, mostly text and controls:
"Complexity" in SONG LEARN's level pop-ups; "Track Selected" + "(press again to control volume level)"; the stopped
play screen without the song title, a more readable start line; the "THE LAYER BUTTONS" screen removed; ALGO alone
sets the start (the pop-up names the section), PRESETS = the key (the SAVE/SELECT key mode goes); a paused start
change resumes from the new start; help screen and legends; a Pop-ups ON/OFF setting in the old "Controls hint"
slot, set ON again after every firmware update (the user's wish); the mode screen drains the knobs (K3 changed SYNTH's reverb); PLAY lit on BACKTRACK's SETUP; EDS and NIGHT
DRIVE swap their sounds; Oh! Susanna's chorus bars 2 and 6 become "D A7". Optional Part 6 (**): the count-in shows
the beat in the bar instead of the beats left. **No test cycle (the user's choice):** build, pytest and the
make_songs diff only; update stale test strings without running the tests. Coder Sonnet; judgment calls in
docs/decisions-M10.2.md. Never run a device command.

---

## 2026-11-15 — from the planning chat — Milestone M10.3 planned: docs/plan-M10.3.md (revised the same day)

The user installed 0.10.3-m10.2 and used it; their notes are in the vault. M10.3: BACKTRACK's indicator leaves LOOP
as soon as ALGO picks another structure; HOME from playing or paused goes straight to SETUP; SEQ steps the band kit
while playing (bass and harmony programs re-sent by the player); the kit pop-up keeps "Band Kit". SONG LEARN: a footer
at the end of the song list; the empty song / backtrack screens point to the website; the keys play the melody's
sound; the Start pop-up's section name bigger. POTATO's CRUSH 127. The new splash logo (no name line, the version
lower right) and mode-screen mascot; "Kick" bold in the mode screen's title. Open source (vault D071): LICENSE,
LICENSES/, SPDX lines, REUSE.toml, NOTICE, one contact comment (no joke comment). **The built-in starter library**
(songs/library.txt inside the firmware, compressed, the store wins on the same slug; USB commands and store code
unchanged). Docs: docs/reference.md (current controls, from the code) and a staleness check. **Testing: one trimmed
run at the end** (Part 9: pytest, host tools, a new builtin_test and m103_test, trimmed m5/m10/m101; no m51, no
every-song play-through). Coder Sonnet; judgment calls in docs/decisions-M10.3.md. Never run a device command.
