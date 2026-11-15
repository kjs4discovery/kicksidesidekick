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
