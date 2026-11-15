# Decisions of the M10.2 run (Sonnet coder; for the planning chat's review)

Format (docs/plan-M10.2.md, "Read this first"): one entry per judgment call the plan does not spell out.
No test cycle in this run (the user's choice): build, pytest and the make_songs diff only.

## M10.2-01 — Part 1: pop-up fonts, sizes
- vpop.title grows 24 -> 32 bytes ("Harmony Complexity Level" is 24 characters), vpop.sub 32 -> 40 (as the plan says). player_stats.vp_title / vp_sub grow the same (player.h) so the
  tests' mirror of the text is not cut; tools/m10_test.py's M10_STRINGS is updated to ("vp_title", 32), ("vp_sub", 40) (m101_test computes its offsets from it).
- draw_vpop: title and sub line are FONT_XS when they fit 208 px, else FONT_T (2 px lower so the baseline matches); never cut.
- "Track Selected" sub line: "(press again to control volume level)" everywhere a layer button selects a layer, except BACKTRACK's melody (bt.on && L_MELODY: no volume there): no sub line.
  The third place in that block (a press on your own part that has no volume: BACKTRACK's melody) shows the same pop-up without sub.

## M10.2-02 — Part 1: stopped play screen
- PRESS PLAY (FONT_M, 24 px high) at y 6 where the title was; the start line at y 38 (8 px under PRESS PLAY), FONT_B in C_GRAY when it fits W_BIG - 4 (172 px), else FONT_XS in C_GRAY.
  The start line is at most "from bar 99 (Chorus 1)"-like; the choice is made at run time per line.

## M10.2-03 — Part 1: the layer-buttons screen
- V_HINT's code, draw_hint, HINT_TIMES, the opening in play_open and player_stats.hints' only writer are removed. The enum slot stays, renamed V_HINT_UNUSED, so V_MODE, V_SYNTH, ... keep their numbers (the tests and
  player_stats.ui_view use them). hint_count stays in the settings record (unused byte; Part 3 uses it).
- Tests: m5_test.py's m5p1-hint check is removed and its "hint-off" set-up no longer turns S_HINT off (S_HINT is now Pop-ups; turning it OFF would silence every pop-up in the later tests); m101_test.py and
  m10_test.py strings: "Drums Complexity Level" etc., "Track Selected". m7_test.py's V_HINT mention is harmless (the view is never reached). Not run.

## M10.2-04 — Part 2: start, key, paused start
- keymode_until is removed from `ui` (field, timers, banner, legend, status hash, synthui.c reset). KEYMODE_MS stays as the name of the TEMPO mode's timeout (renaming it was not needed). The status hash bit that
  carried the key mode (bit 31) now carries `paused_start`, so the PAUSED screen is redrawn when the flag changes.
- PRESETS = the key whenever it is not setting a loop (also with a loop A-B on, also paused). Pop-up "Key" / "E (+2)", or "D (song key)" at 0; app_say("KEY", same text).
- ALGO start pop-up: the section's name as the sub line when the bar is a section's first bar (say_from); app_say("START", "BAR n") unchanged. With a loop A-B on, ALGO still says "LOOP ON / SEQ: OFF FIRST" as before.
- Paused start: `ui.paused_start` (uint8) set by ALGO while PL_PAUSED; PLAY then does player_start(start_point(), count-in setting); cleared at the top of input_play whenever the state is not PAUSED (any start or stop), and by the PLAY branch.
  PAUSED screen: "PLAY: from bar N" in place of "PLAY: on (...)". The key changing while paused does not set the flag.
- SAVE: removed from learn_jobs() on the play screen for both SONG LEARN and BACKTRACK (the free-button pop-up opens on it). The loop-setting SEQ/PRESETS jobs are unchanged.
- Help: PRESETS "key", ALGORITHM "start: bar; loop A/B"; SAVE's row removed (HOME "held: mode" moved up, the last row is empty). BACKTRACK's and the list's help tables are unchanged.
- Legend, stopped, SONG LEARN: the full line "PLAY|ALGO start|PRESETS key|SEQ loop|GLO help" is wider than one row at FONT_XS, so it is split on three pages (HINT_PAGE_MS, rotating):
  1 "PLAY|ALGO start|PRESETS key|GLO help", 2 "FX-LFO: pick part|again: volume" (the existing one), 3 "SEQ loop|SELECT tempo|EDIT view|GLO help" (LOOP_SONG: "SEQ set A-B|..."). Loop-setting legends are unchanged.
- Tests (not run): m5_test.py's key sequences (view-key2, view-key-next-song, view-key-live, p6-guitar-key, p7-notation-flats, m5p1-fresh) use PRESETS (e6) instead of SAVE + SELECT; m7_test.py's "every button has a job" list drops SAVE.
  tools/m2_test.py and tools/m4_test.py still contain the old SAVE + SELECT key sequences (older milestones' copies of the same checks); they were not rewritten and will fail there if ever run again.

## M10.2-05 — Part 3: the Pop-ups setting
- The row is S_HINT (same index 9, same record), name "Pop-ups", values OFF / ON (SET_VAL unchanged), default ON (SET_DEFAULT[9] = 1). The enum name S_HINT is kept (no renames).
- Marker: FNV-1a (32 bit) of FW_VERSION (never 0), stored little-endian in hint_count + pad[0..2] (nothing else uses them: hint_count's only writers were the removed hint screen and the M4-record
  default, which now just gets a mismatch). settings_marker() runs once in app_ui_init after settings_load(): different marker (an update, or an older record, whose hint_count is a 0..10 counter): Pop-ups ON, marker
  stored, settings_apply(), settings_save() once. Equal: the user's choice stays. A factory reset zeroes the record; settings_load's defaults are ON; the next boot stores the marker (a second, harmless save).
- Gate: `vpop_show` (gated on SV(S_HINT)) and `vpop_keep` (the old body, always). popup_open() returns at once with Pop-ups OFF (a button with no job does nothing; popup_input still says "consumed").
- Every vpop_show call, classified:
  - gated (a value shown because a knob or button changed a setting): ui.c say_from "Start"; lm_step "Melody Sound" (incl. its "no room: keeps SONG" sub line); "Track Selected" (both places); the volume pop-up ("Drums Volume"...);
    the level pops "... Complexity Level"; SONG LEARN "Tempo"; SONG LEARN "Key"; bt.c bt_vp_select's pop-up (BACKTRACK's knobs: progression, style, complexity, melody sound, key, structure, tempo, band kit);
    synthui.c "Preset", "Tempo", the quick-knob parameter pop-ups.
  - kept (vpop_keep): synthui.c "SAVED U05"-style confirmation and "NOT SAVED" (an error).
  - Other confirmations (the factory reset, WIPE, yes/no questions) are screens of their own, not vpop calls: untouched. The v-pop and its "SAVED U05" test are unaffected.
- SYNTH with OFF: draw_synth_main shows the turned quick knob's "NAME value" in the bottom row for QUICK_MS (the code of M10 before 9fd953a, restored under `!SV(S_HINT)`); synth_hash includes the value so it redraws on each click.

## M10.2-06 — Part 4: small fixes
- Mode screen: mode_input drops every encoder but SELECT each frame (`for e < NE`, `(void)plat_enc(e)`); mode_choose drops all of them once before the mode starts (so a turn that arrived in the same frame is not applied by SYNTH).
- SETUP: lights() sets PLAY lit when `ui.view == V_SETUP && bt.n` (the same "lit" as the mode screen's PLAY); with no backtracks PLAY stays dim as before.
- EDS / NIGHT DRIVE (vsynth.c PRESETS; both ANALOG, so the engine field is unchanged; names and positions stay). Values after the engine, before:
  - EDS:         1, 60, 90, 14, 40, 50, 70, 0, 6, 12, 110, 0, 30, 20, 0, 0, ARP_UP, RATE_8, 0, 64 | 64, 0, 0, 0, 0, 0, 0, 0, 3, 64, 0, 100, 64, 64, 8, 94, 0
  - NIGHT DRIVE: 0, 50, 90, 20, 48, 30, 82, 0, 6, 10, 80, 0, 30, 64, 0, 0, ARP_UP, RATE_8, 0, 64 | 64, 0, 0, 0, 0, 0, 0, 0, 3, 64, 0, 0, 64, 64, 6, 100, 0
  After: the two rows exchanged (EDS has NIGHT DRIVE's values, NIGHT DRIVE has EDS's, LEVEL 100 / 94 included). No loudness run.
- Test: m101_test.py part 4's "changed vs M10" list is now E-GUITAR, EDS, NIGHT DRIVE, ZUNJET (26 unchanged rows). Not run.

## M10.2-07 — Part 5: Oh! Susanna
- The chorus chart typed exactly as the plan gives it (bars 2 and 6 "D A7"); only songs/make_songs.py's oh-susanna entry changed. song.mid is byte-identical (the melody did not change); song.yaml's chorus chart changed; the two previews
  (preview-full-band-level1.mp3, preview-harmony-muted-level2.mp3) were regenerated with `make_songs.py --preview oh-susanna` (the harmony in them follows the chart).
- `make_songs.py` for ALL slugs: `git status` shows only oh-susanna's files (and make_songs.py) changed; validate: songs/oh-susanna ok. No other song touched.

## M10.2-08 — Part 6: the count-in number
- ui.c frame_play: `count` (used by draw_big and draw_count_view alike, and ui.sh_count) is now the beat in the bar at the lead tick tc: ((tc mod bar_ticks) + bar_ticks) mod bar_ticks / ppq + 1. The bar lines are at multiples
  of bar_ticks, the same as the clicks, so a song without a pickup counts 1 2 3 4 (3/4: 1 2 3), one with a 1-beat pickup in 4/4 counts 1 2 3 4 | 1 2 3 and the pickup comes in on the 4th beat. The number changes at the same instants as the old one (the old
  beats-left number changed when ceil((from - tc) / ppq) did). No change to player.c, the label "COUNT-IN" or the layout. Tests that read the shown count (not run) are not updated: none was found by grep for "COUNT-IN" numbers.
