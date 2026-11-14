# Decisions of the M10.1 run (Sonnet coder; for the planning chat's review)

Format (docs/plan-M10.1.md, "Read this first"): one entry per judgment call the plan does not spell out.

## M10.1-01 — HANDOFF's evening note vs. the plan on ZUNJET (Part 0)
- Situation: the evening note says "HAZE PAD -> ZUNJET waits for M10.2"; its Correction 2 and docs/plan-M10.1.md Part 4.4 put ZUNJET in this build.
- Chosen: the plan and Correction 2 (newer): ZUNJET replaces HAZE PAD in this build.

## M10.1-02 — Part 1: stale M10 expectations updated (not relaxed)
- tools/m10_test.py m10p3 checked the old name and milestone "m10"; m10p2 checked that the LOOP structure's indicator says VERSE. Both are what
  Part 1 changes on purpose, so the expectation follows the plan (new name, milestone "m10.1", indicator "LOOP"). Nothing else in m10_test.py touched.

## M10.1-03 — Part 1: the name and the fonts
- The name was already drawn in FONT_B when it fits, else FONT_XS (never cut): "KickSide - Your Musical SideKick" is about 222 px in FONT_B, so it fits
  in FONT_B on the mode screen (limit 232 px) and on the splash (236 px). The Settings screen's "name  RAM ONLY" line (FONT_XS, about 222 px) fits too.
  FW_MILESTONE is "m10.1" (shown under the name on the splash), FW_VERSION "0.10.2-m10.1".
- Source comments that said "Tuneka" now say "renamed" (the plan: every "Tuneka" is gone from the firmware).

## M10.1-04 — Part 1: edit page titles
- Title = page name + " - " + the list label of the preset ("FX 2 - POTATO", "ENVELOPE - U05 ZUNJET"; synthui.c draw_synth_page, via pl_label).
  synth_top picks FONT_B if the title fits beside the page number ("3/9"; room = 234 - width of the number - 6 - 8 px), else FONT_XS, else FONT_T
  (the smallest); never cut. With the longest name ("U30 WWWWWWWWWWWW", 12 W's) FONT_B does not fit; FONT_XS does: the test's screenshot
  (m101p1-t-user.png) shows it whole with a 24 px gap to the page number. A short title ("ENVELOPE - E-PIANO") is in FONT_B.

## M10.1-05 — Part 1: the hint pages and the legend text
- HINT_PAGE_MS 6000u is in app.h (one constant for ui.c's three places and synthui.c; bt.c and btview.c had no own timer: BACKTRACK's play screen uses
  ui.c's legend_play). The SYNTH screen's change detector (synth_hash) now includes the page so the row is redrawn when the page changes.
- The new layer-button hint is "FX-LFO: pick part|again: volume" ("|" is draw_legend's separator: it highlights the first word of each piece, so the
  button name is "FX-LFO:" and the second piece's first word "again:"). It fits one row at FONT_XS (draw_legend falls back to FONT_T, never cuts).
  (The buttons' new behaviour is Part 2; the text is final here.)
- For the tests: player_stats got two texts at its very end (title_now, legend_now): the SYNTH title and the bottom row as drawn last.

## M10.1-06 — Part 1: LOOP, pop-ups
- LOOP: bt.ind_loop = (structure == LOOP) in bt_ind_update; the indicator shows "LOOP" (amber, the steady colour) and blink is forced off. It follows the
  structure the band PLAYS (bt.structure), so a pending change to FULL shows LOOP until the swap. (ind_name in the stats says "LOOP".)
- Pop-ups in SONG LEARN: K1-K3 "Drums Level" / "Bass Level" / "Harmony Level" with "2 of 3" (the layer's own number of levels: 3, 4, 5); SELECT
  "Tempo" "123 BPM" (not while SELECT is the key / zoom / tempo mode: those keep their own messages); PRESETS "Start" with the section name; ALGO
  "Start" "BAR 3". The existing top-line messages (app_say, also the screen reader's) stay as they were.
- SYNTH: PRESET "Preset" + the list label ("NEON PAD", "U05 NAME"), SELECT "Tempo", K1-K4 on the main screen: the parameter's name as the page
  names it ("CUTOFF", "RESONANCE", "REVERB MIX", "DELAY MIX"; the plan's "Cutoff" is the page's capitals). PRESET and SELECT also pop up on the edit
  pages (they act there too); the K knobs do not (the page shows them). The old bottom-row value ("quick knob") is gone: the pop-up replaces it.
