# Decisions of the M10.3 run (Sonnet coder; for the planning chat's review)

Format (docs/plan-M10.3.md, "Read this first"): one entry per judgment call the plan does not spell out.
No test cycle in this run (plan, D072): build, pytest tools/fm1song and the make_songs diff only.

## M10.3-01 — Scope: the plan file against the prompt and the HANDOFF
- The user's prompt asks for "parts 0-10", builtin_test after Part 6, and a trimmed run of "Part 9" with the test-runner subagent. The HANDOFF's
  M10.3 note (written before the plan's revision) names the built-in starter library and a trimmed test run too. docs/plan-M10.3.md (modified after
  the HANDOFF note, "revised the same day") has Parts 0-7, says "Not in M10.3: the built-in starter library (M10.4)" and "No test cycle (the user's choice, D072)";
  there is no Part 8, 9 or 10, and no builtin_test (the tool does not exist).
- Decision: follow the plan file literally (CLAUDE.md: a conflict means stop and ask; the user is away and said to decide and record). The library and the
  emulator test runs are NOT done. If the planning chat wants them, they are a follow-up (M10.4). The test-runner subagent is not used.

## M10.3-02 — Part 1: BACKTRACK
- Indicator: bt.ind_loop = (structure == LOOP && w_structure == LOOP); the redraw number returned by bt_ind_update carries ind_loop (bit 10), so the label repaints the frame ALGO is turned.
  (The two warnings in the build, bt.c:234 '^' within '|' and vendor eas_voicemgt.c, are older than this milestone.)
- HOME on BACKTRACK's play screen: in any state but stopped, player_play(0) first, then the stopped branch's path (V_SETUP, lib_release which waits for idle). Held HOME is unchanged (other code).
- SEQ: learn_jobs no longer removes SEQ for bt.on; bt_play_input steps the kit (no phrase boundary); input()'s `down &= ~(SEQ|SAVE)` stays so input_play's loop code never sees SEQ.
- Programs: player.c `kit_programs_due` (set by player_kit; cleared by channel_setup, which sends the programs itself); dispatch() calls kit_programs_send() at its next event: 0xC0 on layers 1 and 2 with the kit's program, or ly->program for 0xFF, skipping a layer with no variants.
  Swap/phrase code untouched.
- Help: BACKTRACK's BTN table gets a "SEQ band kit" row. The table had 9 rows and no room, so ALL button tables became 10 rows 15 px apart (was 9 at 17 px; the last row ends at y 165, "GLO again" stays at 184). Legend: "PLAY pause|HOME stop|SEQ band kit|GLO help" playing (the EDIT view is in the help); paused: "PLAY go on|HOME stop|SEQ band kit|GLO help".
- Title "Band Drum Kit" (bt_vp default case). Test strings naming "Band Kit": see M10.3-tests (end of this file).
