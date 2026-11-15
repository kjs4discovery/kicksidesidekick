# Milestone M10.2: instructions. Your M10.1 notes: text, controls, pop-ups setting, small fixes, Oh! Susanna

From the planning chat, 2026-11-14. Scope: the user's M10.1 notes (vault Brainstorms/2026-11-04 Prototype feedback,
"2026-11-14 — M10.1 build"), vault decision D069.

State at the start: the user installed 0.10.2-m10.1 (FM-1_46001002) and used it. Record that in LOG and device.md
first (install details: not given; write "installed and used; notes in the vault").

Version: **0.10.3** (`--release 0.10.3-m102`; a dot is not allowed in the suffix, M10.1-15). Package
`build/step-M10.2/fm1acc-0.10.3-m10.2.fwsc`; identity FM-1_46001003 if the scheme holds. Record what you get.

## Read this first (the coder is Sonnet)

This plan is deliberately prescriptive. Follow it literally.
- **MUST / MUST NOT** are hard rules. Everything else is the intended design; if it turns out impossible or clearly
  worse in the code, choose the SMALLEST change that does the job, then record it.
- **docs/decisions-M10.2.md (new; you write it)**, same format as docs/decisions-M10.1.md, numbered M10.2-01, ...
  Every judgment call this plan does not spell out goes there.
- **MUST NOT** change: the song store's flash code, the update loader, the rescue combos, the factory reset, the
  hardware safety rules in CLAUDE.md, the settings record's layout and size (Part 3 reuses an existing slot), the
  user presets already saved on the unit, the compiled file of any song other than oh-susanna.
- **MUST NOT** refactor code a part does not need. No renames of existing functions or files.
- **No test cycle (the user's choice, D069).** No full suite, no fast lane, no emulator runs, no new tests, no
  screenshots. What stays, because it costs seconds and guards the songs: the firmware builds with no new warnings;
  `pytest tools/fm1song` passes; `songs/make_songs.py` for ALL slugs changes only oh-susanna's files. Where a changed
  text or behaviour is named in an existing test's expectations (grep tools/m*_test.py for the old strings, e.g.
  "Your Part", "Bass Level", "THE LAYER BUTTONS", "Controls hint", "SAVE"), update the expectation to the new text so a
  later suite does not fail on stale text; do not run those tests; list each change in decisions-M10.2.md.
- **Never run a device command.** End by writing docs/step-M10.2-install.md and STOP.

## PART 0 — Housekeeping
- LOG + device.md: the M10.1 install (above). Create docs/decisions-M10.2.md.

## PART 1 — Text and screens (SONG LEARN)
1. **Complexity in the level pop-ups.** ui.c's TITLE table for K1-K4: "Drums Complexity Level", "Bass Complexity
   Level", "Harmony Complexity Level", "Melody Complexity Level". Value unchanged ("2 of 3"). The title must fit the
   pop-up's 216 px in FONT_XS; if one does not, use the next smaller font for that title (FONT_T); never cut it.
2. **"Track Selected".** Where a layer button makes a layer your part (today `vpop_show("Your Part", LT[l], "", 0)`,
   both places in input_play), the pop-up is: title **"Track Selected"**, value the layer name ("Drums", "Bass",
   "Harmony", "Melody"), and under it, smaller, **"(press again to control volume level)"**. Use vpop's `sub` line;
   grow `vpop.sub` from 32 to 40 bytes (the text is 38 characters + the terminator). If it does not fit 216 px in
   FONT_XS, use FONT_T; never cut it. BACKTRACK's melody button (no volume there): "Track Selected" / "Melody", no
   sub line. The volume pop-up ("Bass Volume" / "QUIET") is unchanged. app_say's "YOUR PART" becomes "TRACK SELECTED".
3. **The stopped play screen** (draw_big, `st == PL_STOPPED`, not setting a loop): remove the song title line above
   PRESS PLAY (the title is at the top of the screen already). Move "PRESS PLAY" up to where the title was (about
   y 6). The start line under it ("from bar 9 (Chorus 1)") becomes more readable: FONT_B instead of FONT_XS, colour
   C_GRAY or brighter (not C_DIM), about 8 px below PRESS PLAY. If the longest start line of the public songs does not
   fit W_BIG in FONT_B, use FONT_XS for that line but keep the brighter colour. Record the fonts and positions.
4. **Remove the "THE LAYER BUTTONS" screen** (V_HINT, draw_hint, the HINT_TIMES counter and the code that opens it
   before a song's play screen). The view goes; nothing replaces it. `hint_count` in the settings record stays as an
   unused byte (MUST NOT change the record's layout). The legend line "FX-LFO: pick part|again: volume" on the
   stopped screen stays (it is the bottom-row hint, not the screen).

## PART 2 — Start point and key (SONG LEARN)
1. **One start control: ALGORITHM, by bar.** Turning ALGO moves the start one bar a click (as today). The pop-up:
   title "Start", value "BAR 9", and when that bar is a section's first bar, the section's name as the sub line
   ("Chorus 1"). (set_from_bar already starts a section's first bar at the section's pickup; keep that.)
2. **PRESETS = the key**, as in BACKTRACK: a semitone a click, the nearest way (-5..+6), with today's KEY-mode code
   (ui.key, player_transpose). Pop-up: title "Key", value the key now sounding and the shift, e.g. "E (+2)"; at 0 the
   song's key and "(song key)". The KEY mode (SAVE on, SELECT turns the key, "SELECT key|SAVE done", KEYMODE_MS for the
   key) is removed; SAVE has no job in SONG LEARN's play screen any more (it opens the controls pop-up like any button
   without a job). SELECT is tempo / zoom only. REC's tempo mode is unchanged.
3. **While setting a loop** (SEQ, loop_step 1 or 2) PRESETS keeps its job of snapping A or B to a section, as today
   (the planning chat's call; the key is not needed there). Outside loop setting PRESETS is always the key.
4. **Paused, then a new start.** If ALGO moves the start while PAUSED, PLAY resumes from the NEW start point
   (start_point(), with the count-in setting as for any start), not from the paused position. If the start was not
   moved, PLAY resumes as today. The PAUSED screen then says "PLAY: from bar 9" (the new start) instead of
   "PLAY: on (...)". A flag set by the ALGO start change while paused, cleared by any start or stop, is enough.
5. **Help and hints.** The GLO help screen (draw_help, SONG LEARN's ENC and BTN tables): PRESETS "key",
   ALGORITHM "start: bar; loop A/B", SAVE removed from the list, SELECT "tempo / zoom". The bottom-row legends
   (legend_play): the stopped line becomes "PLAY|ALGO start|PRESETS key|SEQ loop|GLO help" (LOOP_SONG: "...|SEQ set
   A-B|..."); if it does not fit, split it over the two alternating pages (HINT_PAGE_MS) rather than cutting words.
   Remove "SELECT key|SAVE done". Loop-setting legends unchanged.

## PART 3 — A setting: Pop-ups ON / OFF
- The Settings row "Controls hint" (S_HINT, freed by Part 1.4) becomes **"Pop-ups"**, values OFF / ON, default ON.
  Same slot, same index (MUST NOT change the record).
- **Pop-ups is ON after every firmware update (the user's wish).** At start-up, compare a marker of this firmware's
  version (e.g. a 32-bit hash of FW_VERSION) with one kept in the record's spare bytes (`hint_count` and `pad[3]`,
  unused after Part 1.4; check nothing else uses them). If they differ: set Pop-ups ON, store the marker, save the
  settings once with the existing settings-save code. If they match: keep the user's choice. So a choice of OFF lasts
  until the next update, and a new user or a factory reset gets ON as the default. Record the marker's form.
- OFF suppresses, in all three modes: the value pop-ups shown when a knob or button changes a setting (SONG LEARN,
  SYNTH, BACKTRACK), the "Track Selected" pop-up, and the controls pop-up for a button with no job. Gate it in one
  place (vpop_show and popup_open) with a parameter or a second function for the calls that must stay.
- OFF does NOT suppress: confirmations and results (e.g. "SAVED U05"), yes/no questions, warnings and errors, the
  factory reset and WIPE screens. List every vpop_show call you classified, and how, in decisions-M10.2.md.
- With OFF, SYNTH's bottom row shows the turned value again (as before M10.1). app_say's message line is unchanged.

## PART 4 — Small fixes
1. **Mode screen ignores the knobs.** synthui.c mode_input reads only SELECT; the other encoders keep their detents
   and SYNTH then applies them (K3 turned on the mode screen changed SYNTH's reverb). In mode_input, drain every
   encoder it does not use each frame (as SYNTH's menu does: `(void)plat_enc(...)`), and drain all encoders once in
   mode_choose before the chosen mode starts.
2. **BACKTRACK's SETUP page: PLAY lit.** On V_SETUP, PLAY's light is ON (full, as on the mode screen) when there are
   backtracks to play; otherwise as today.
3. **EDS and NIGHT DRIVE: swap their sounds.** In vsynth.c's PRESETS table, the two rows swap their VALUES (every
   value after the engine field, LEVEL included); the names and the list positions stay (EDS stays 6th, NIGHT DRIVE
   where it is). So EDS now sounds as NIGHT DRIVE did and NIGHT DRIVE as EDS did. Record both rows before and after.
   (No loudness run: each LEVEL moves with its own sound.)

## PART 5 — Oh! Susanna: the chorus harmony (the planning chat's chart; type it exactly)
The chorus's 2nd and 6th bars (song bars 10, 14, 26, 30) have a G chord under a melody of D-chord notes (A A F# D)
and a held E: that is the clash the user hears. In songs/make_songs.py, oh-susanna's chart, the chorus becomes:
```
| G | D A7 | D | A7 |
| G | D A7 | D A7 | D |
```
(only bars 2 and 6 change, "G" -> "D A7"). The verse chart, the melody, the tempo (125) and everything else stay.
Run make_songs.py for oh-susanna; validate; then for ALL slugs: only oh-susanna's files change. The song must be sent
again after the install (Part 7's install doc).
Source: the chord placement of the published chorus "C, G, A7, D7" in G (IV, I, then V) at
celticguitarmusic.com/folk_susanna.htm, transposed to D.

## PART 6 (**) — The count-in: show the beat in the bar (the planning chat's recommendation; see D069)
Today the count-in number is the beats LEFT (ui.c, `count = (from - tc + ppq - 1) / ppq`), and a song with a pickup
counts one full bar plus the pickup bar's beats before the pickup: Auld Lang Syne (4/4, 1-beat pickup) shows 7 6 5 ...,
Saints (3-beat pickup) 5, Michael Row 6, the 3/4 songs with a pickup 5, songs without one 4 or 3. The CLICKS are right
(a full bar, then the pickup bar up to the pickup; the high beep on each bar's beat 1). Change only the NUMBER: show
the beat within the bar, 1 2 3 4 | 1 2 3 (then the pickup comes in on 4), in draw_big and draw_count_view alike, so
every song counts like a bandleader's "one, two, three, four, one, two, three". Above it "COUNT-IN" as today. No
change to player.c. (To skip this part, delete the sentence marked (**) in the prompt.)

## PART 7 — Docs, build, install note
- docs/controls.md: an "M10.2" section in text (no screenshots): the start / key controls, the pop-up texts, the
  Pop-ups setting, the layer-buttons screen gone, the paused start, the mode-screen knobs, PLAY lit on SETUP, EDS /
  NIGHT DRIVE, the count-in numbers (if Part 6 ran).
- The release build (two builds, same sha). Then the checks in "Read this first" (build, pytest, make_songs diff).
- docs/step-M10.2-install.md: install over M10.1 (no factory reset needed; saved presets and settings stay), then
  send one song: `tools/fm1song.sh send songs/oh-susanna/`. A line saying Pop-ups is ON after this (and every) update.
  A short by-hand list for the user: one line per item of their M10.1 notes, what to check on the unit.
- LOG, commit after each part. STOP.
