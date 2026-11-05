# Milestone M3 — instructions (one build)

From the planning chat, 2026-11-05 17:40. Scope: D024 (the user's M2 test notes + the
planned M3/M4 items, without chord checking and without the MIDI import tool) and D025 (simplified scrolling notation).
The user's M2 notes are quoted in the vault; the essentials are below.
Design reference: docs/m3-design/notation-mock.png (approved look for the notation view).

```text
Follow CLAUDE.md and docs/HANDOFF.md. Build Milestone M3 in the parts below, in order.
Commit after each part. TESTING (to save time and usage): after each part run the fast
checks (pytest, the C unit/store tests, and emulator tests for THAT part's features on
two songs) and keep that part's build in build/step-M3/; run the FULL emulator suite
only on builds offered for install: at the EARLY INSTALL POINT and at the end.
Emulator only; no hardware. At the EARLY INSTALL POINT write the early checklist and
continue; at the end write docs/step-M3-install.md and STOP for review. If the user is
away, don't stop for questions: make reasonable choices, record each in LOG with the
reason. For every new or changed screen, put a mock screenshot into docs/controls.md
first, then build it. Keep the on-screen legend on every screen.

PART 1 — Bugs and housekeeping
1. Greensleeves and Oh! Susanna: no drums at level 1 (user). Find out why and fix
   (song data or player).
2. Loops: several bars showed the "A" mark at once; with the loop OFF the bottom line
   still said "start bar". (Part 3 redesigns loops; fix the root cause there too.)
3. Bass muted: the key lights showed the bass, but the rest of the screen still showed
   the harmony, in every view. (Part 2 fixes this properly.)
4. No visual count-in in the keyboard and chart views after a pause.
5. Beat light: the user saw PLAY flash once per BAR, not every beat. The mid-level
   flash is probably invisible on the real LED: make every quarter-note beat a clear
   flash (beat 1 can be longer or brighter, but every beat must be visible).
6. Housekeeping: docs/step-M2-install.md step 2 → `songs/*/`; `fm1song send` skips
   non-song arguments with a one-line "skipped" note; CLAUDE.md: `fm1song send` /
   `delete` are user-run writes, `fm1song list` is read-only for Claude Code (HANDOFF
   2026-11-05 12:50).

PART 2 — "Follow your part" (all four layers) and blinking in rhythm
- The muted layer is "your part". The screen, rhythm cue, key lights and views follow
  it: harmony → chord; bass → bass note; melody → melody note; drums → drum view
  (Part 7). Several muted: propose a rule (or a control to pick "your part"), shown
  on screen. Harmony-wins from M2 is replaced by this.
- Key lights and the keyboard view's lit keys BLINK in the rhythm of your part at its
  level (flash at each attack, dim between). Melody muted → key lights show the
  melody note. Drums muted → no key lights (the beat light is enough).
- Rhythm cue: shown ALL the time (not only when muted), on the RIGHT of the chord,
  about HALF its current height, no level label (the level is on the bottom row).
- Setting to turn blinking off (Part 4).

PART 3 — Loops redesigned (user: "a little wonky")
- One loop type: BARS A–B (drop the SECTION loop; SONG loop may stay as "whole song"
  if simple, propose).
- A clear way to set and CONFIRM A and B (e.g. move A, press to set, move B, press to
  set; shown step by step in the legend), and to clear the loop.
- Highlight the selected bars in the bar strip/chart, keep the small A and B marks;
  exactly one A and one B can exist.
- Start from a bar (and from a section if it stays simple). Pickup rules as before.

PART 4 — Screen polish, count-in, inversion label, settings
- Legend: button names and actions in different colours, a divider between pairs
  (see the mock's bottom row).
- Remove the row showing the current section (more screen space).
- Keyboard view zoomed in: fewer keys on screen (e.g. ~1.5 octaves around the
  voicing, following it).
- Visual count-in in EVERY view (it may replace the view while counting).
- Count-in: much louder and a sound that cuts through (a beep, not a click); length
  1 or 2 bars (setting).
- Inversion label: show the voicing's inversion (e.g. "C/E · 1st inversion") in the
  chord and keyboard views; higher levels use more inversions / voice leading already:
  check that the levels make this visible.
- Settings screen (small): key-light/keyboard blinking on/off, count-in length (1/2),
  count-in volume, note names under notation on/off, guide volume (Part 8). Stored with
  fm1-base's own settings store; propose the button to open it; legend on screen.

PART 5 — Happy Birthday (the MIDI import tool moved to M4: the user wants to think it through)
- Happy Birthday to You (melody: "Good Morning to All", 1893, public domain): our own
  arrangement with songs/make_songs.py, 3/4 with pickup, all four layers, :1/:2.
- EARLY INSTALL POINT: after Part 5, write docs/step-M3-early-install.md (install over
  M2, send updated/new songs, on-unit checks for parts 1–5, fallback M2; full suite
  on that build), log it, and
  CONTINUE.

PART 6 — Instrument views (harmony, bass)
- Guitar chord diagrams: a chord box (6 strings, frets, fingers/open/muted strings)
  for the current chord, next chord small; standard open/barre shapes from a table
  (our own table), following key changes. Unknown shapes: say so.
- Bass tab: 4-string tab of the bass part at its level (scrolling, same engine as
  Part 7 if practical), with the note names.

PART 7 — Scrolling views (one shared scrolling engine, "now" line)
- Melody display: note bars sliding towards the now line (piano-roll strip).
- Simplified notation (D025), as in docs/m3-design/notation-mock.png: one staff, treble
  clef for melody, bass clef for bass; noteheads (hollow ≥ half note), stems, beams for
  eighth pairs, dots, bar lines, key signature; ledger lines where needed; chord names
  above; note names under the staff (setting); played notes dim, current amber,
  upcoming white. No ties across bar lines, no rests, no complex rhythms: notes that
  don't fit are drawn as the nearest simple value. Follows key changes.
- Drum grid: rows kick, snare, hi-hat/ride, other; hits as blocks sliding to the now
  line; follows the drum level.
- About 1.5 bars visible at 240x240: propose a zoom (bars visible) setting if needed.

PART 8 — Song folders and guide volume
- Song folders/categories: song.yaml gets an optional `category` (e.g. "Hymns",
  "Blues", "Practice forms"); the song list groups by category (folders you open), with
  "All songs". Format change → bump the format minor version; the device keeps reading
  older files.
- Guide volume: your part can play quietly instead of fully muted (e.g. 0 / 15 / 30 %,
  a setting or a control), so you can lean on it while learning.

PART 9 — Tests, budget, install checklist
- Emulator tests for every item and every song (9 songs), flash IDs 856014 / b36014 /
  unknown, the store tests, settings saved across a restart.
- Flash / RAM / CPU budget. Version 0.4.0-m3, release build, new identity.
- docs/step-M3-install.md: install over M2; resend songs if the format changed (or if
  new); on-unit checks per part, ordered so the user can test view by view per layer;
  fallback = M2. STOP for review.
```
