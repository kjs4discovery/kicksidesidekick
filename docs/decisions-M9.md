# Decisions of the M9 run (Sonnet coder; for the planning chat's review)

Format (docs/plan-M9.md, "Read this first"): one entry per judgment call the plan does not spell out.

## M9-01 — The switch method for progression / style / structure changes: (A) SEAMLESS (Part 0)
- Situation: Part 0 had to measure and choose. Numbers (emulator, M5.1 flash timing, the largest backtrack in its
  busiest style, bt28 in FUNK; docs/step-M9-part0-measure.md; the measuring patch is tools/patches/m9-part0-measure.patch,
  not in the firmware): song_open 11 ms + song_restyle 12 ms + building the 3 layers' level 1 38 ms = 61 ms (about 10.5
  audio blocks of 256 frames); rebuilding only the three layers' levels (a complexity change) 23-62 ms (62 in FUNK,
  COUNTRY). The player kept making its blocks while this ran from the main loop (168 blocks in the 972 ms the 11
  styles took, 167 expected), the longest EAS render 17,841 ticks of a 139,296 budget (13 %): no gap, no xrun.
  Two copies: the largest build need is 54,032 B (bt28 FUNK; M8's figure confirmed) x 2 = 108,064 B <= BACKTRACK's song
  room 112,896 B (each copy <= 56,448 B, half of it).
- Options: A seamless (build into the other half of the song room, swap at the change point); B one-bar break.
- Chosen: A, because both of the plan's conditions held on M8's generators. The condition "two copies fit" MUST be
  re-checked on Part 3's tier-table generators (their levels are bigger or smaller than M8's); if it fails then, the
  fallback to B is recorded here (see the later entry).
- Undo: bt.c's switch code (bt_prepare / bt_apply) and player.c's player_swap; B needs only lib_open + lib_restyle at the
  boundary with the player stopped.
