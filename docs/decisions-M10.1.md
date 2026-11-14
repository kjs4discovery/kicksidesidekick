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

## M10.1-07 — Part 2: the layer buttons (D066)
- input_play (ui.c) has no hold logic any more: a press (the button-down edge, at once) on a layer that is not your part makes it your part (the
  view follows, as the old hold did; its volume does not change); on your part it cycles FULL -> QUIET -> MUTED -> FULL. Every layer keeps its own
  volume (ui.vol[]). Pop-ups: "Your Part" + "Drums" (the layer name in capitals-and-lower case), or "Drums Volume" + "QUIET".
  The old hold fields (ui.hold_t, hold_done, HOLD_MS) stay in the code unused (the plan: no refactors); popup_input still writes hold_done.
- Text: the instruction row is "FX-LFO: pick part|again: volume" (M10.1-05); the controls hint screen (draw_hint) now says "PRESS: make it YOUR PART"
  / "PRESS AGAIN: its volume"; the help screen's button list says "your part, volume".
- BACKTRACK: its layer buttons are the same code (input_play), so they got the same rule. The melody is the keys there: its button only selects it
  as your part (a second press shows the same pop-up; there is no melody volume). Drums, bass and harmony: first press = part, again = volume.

## M10.1-08 — Part 2: HOME acts on RELEASE
- A HOME press acts when HOME is released after under 2 s (input() in ui.c turns the press edge into a release "tap": ui.home_tap, set only if HOME was
  seen up since power-on, i.e. never for the WIPE power-on combination or a HOME held since boot). A hold of 2 s fires the straight-to-mode-screen
  jump (unchanged) and clears the tap; so a hold never acts as a single press first. Released after 1.5 s it counts as one single press.
  Consequence for every screen: the other buttons' behaviour is unchanged; HOME's "no" in the REPLACE question, "back" in SAVE / Settings / help all act at
  the release now.
- HOME no longer opens the "NOT IMPLEMENTED" pop-up anywhere: its only two "free" screens (SONG LEARN's folder list and BACKTRACK's SETUP) now go to the
  mode screen (the pop-up code for a free HOME stays but never triggers). SYNTH's main screen: HOME goes to the mode screen (was a hint message).
  SONG LEARN's song list with no songs on the unit (nothing to go up to): HOME goes to the mode screen too.
- The pop-up (value pop-up) closes on a button press as before; for HOME that is at its release (the same moment HOME acts).

## M10.1-09 — Part 2: K4 = Melody Sound in SONG LEARN
- Check: every public and private song has exactly one melody level (and 3 / 4 / 5 levels for drums / bass / harmony), so K4 is free everywhere. A song with
  more than one melody level would keep the level job for itself (code path kept in input_play: variants > 1).
- vsynth plays the melody: player.c send() routes layer 3 to vsynth_key (note on / off; controllers and programs are dropped) while player_melody_synth(1);
  QUIET becomes vsynth_gain (the melody's velocity squared, as EAS's gain does). synth_band(1) keeps EAS under it; vsynth lives in the top of the song
  buffer (as in BACKTRACK; VS_MEM_BT 83,712 B, 4 voices). Changing the routing holds the melody layer (player_hold) as lib_level does.
- RAM check: public songs need at most 24,672 B of level buffers (oh-susanna), the song room beside the synth is 112,896 B. Private zombie needs 172,384 B
  (and let-it-be 30,672, true-colors 54,160, creep 59,840 fit): a song that does not fit keeps its own instrument: K4 shows "SONG" with
  "no room: keeps SONG". A pre-2.0 file (not fmt2) is treated the same.
- The choice is a reference (pl_ref: a stock preset's position, or 0x100 + user slot; lm.ref in ui.c), not a list position. It resets to SONG whenever a song is
  opened (open_song, also the same song again), when the song store is touched (send, delete, wipe: lib_release) and on leaving the mode.
  The preset's arpeggiator is forced off (the melody is played by the song; BACKTRACK does the same for the keys) and its delay follows the song's tempo.
- BACKTRACK's K4 (closes M10-14): bt.sound_ref (pl_ref) is the truth; bt.sound (the list index) is recomputed from it (bt_snd) wherever it is used, so a preset
  saved in SYNTH (which can shift the list) cannot change the sound BACKTRACK keeps. SONG LEARN's and BACKTRACK's K4 use the same list.

## M10.1-10 — Part 2: measurements and small choices
- CPU, SONG LEARN with the melody on vsynth (tools/m101_test.py m101p2): MIAMI PAD (M9 / M10's heaviest) on every public song at its busiest levels
  (drums 3, bass 4, harmony 5), 20 s from PLAY: the worst block (EAS render + vsynth render, each at its own maximum, so a little pessimistic) is
  **35 %** of a block (limit 60 %); the same song with its own instrument 14 %. A sweep of all 30 stock presets on one song is in the LOG.
- RAM: vsynth's memory (83,712 B) sits in the top of the song buffer as in BACKTRACK; the public songs' level buffers are at most 24,672 B
  of the 112,896 B left (M10.1-09); .data + .bss grew by about 0.1 KB (see the budget in the install note).
- Stats for the tests: player_stats.mel_sound and .vols (the two spare words m10_pad[2], same offsets); the part and volume stats are set at the press
  because the frame (which normally sets them) does not run under the value pop-up.
- The pop-up's value for K4 when the song has no room for the synth: "SONG" with the second line "no room: keeps SONG".
