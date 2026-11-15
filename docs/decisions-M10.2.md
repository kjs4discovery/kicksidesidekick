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
