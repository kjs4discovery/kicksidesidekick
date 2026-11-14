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

## M10.1-11 — Part 3: FACTORY RESET
- Command: SysEx 0x78 (SC_FACTORY; 0x76 / 0x77 were not used either, 0x78 is the plan's choice and was free: the list is 0x70..0x75, 0x7F), confirmation word "RESET"
  (5 bytes, as WIPE's is "WIPE"); laptop: `tools/fm1song.sh factory-reset --yes` (without --yes it only explains and sends nothing). Result codes as a
  send's (0 ok, 9 SYNTH / BACKTRACK, 1 no flash, 6 a flash error with its code).
- What it does, in this order (library.c lib_factory_reset): (1) the song store erased by the existing lib_wipe (M5.1; unchanged; a blank sector is left),
  (2) the preset object's two sectors (0xDE000, 0xDF000) erased by plat_data_erase (a blank sector is left), the RAM slots emptied, (3) the settings:
  there is NO existing command or rescue combination that resets the settings, so a record that is not ours (all zero) is saved with the normal store
  save and settings_load() reads it back as "none yet" = the code's own defaults (no second copy of the defaults), applied at once. The panel
  calibration (0xDC000) and the firmware are never touched. If the unit is in SONG LEARN with a melody synth on, that is switched off first, as a send does.
- Afterwards the screen shows "FACTORY RESET DONE" / "SWITCH OFF AND ON" (the new view V_RESET): the buttons and encoders are no longer read, and the
  frame draws nothing else, until the power is cycled. (The two lines are one centred title and one subtitle; the plan's text with " - " between them.)
- Power cut: every part is either old or empty at the next boot (each already boots from junk or an empty area); proved on the timed emulator
  (tools/m101_flash_test.py, `m101f`): the image has two songs (one sector), two saved presets and a changed setting: the reset is 5 flash operations
  (1 song-sector erase, 2 preset-sector erases, the settings' save: erase + page write); a cut at each of the 5, then a boot in SONG LEARN and in SYNTH: always
  boots, no watchdog reset, no XIP read with a flash command running; after the complete reset nothing is left. A fuller store has more erases of the
  same existing kind (M5.1's WIPE, already cut-tested).

## M10.1-12 — Part 4: the preset list, E-GUITAR, EDS, ZUNJET
- Order (vsynth.c): E-PIANO, E-GUITAR, E-SITAR, PARKER, POTATO, EDS, STAB 84, then the others in today's order (NEON PAD ... VHS STRINGS, 30 in all).
  Positions that named a stock preset: SYNTH starts on position 0 and BACKTRACK's default melody sound is position 0 (both E-PIANO, still first),
  `vsynth_load(&PRESETS[0])` at start-up is E-PIANO too; the kits name no preset; tools/vsynth_test.c named two presets by position (NEON PAD at 3, CHIP
  BLIP at 12): now by name (a small `named()` helper); tools/m7_test.py's OLD list (M7's order, used to find the sounds it sampled) says ZUNJET
  where HAZE PAD was. The user's saved presets (U slots) are records, not positions, and are untouched.
- E-GUITAR: attack 0 -> 12, fuzz 90 -> 35; the screen shows both as raw numbers (ATTACK and FUZZ are plain 0..127 parameters, SHOW_NUM), so the table
  value IS the screen value. Nothing else changed, LEVEL 100 too. vsynth_test LEVELS=1: E-GUITAR now 806,299 against the median 970,535 (-1.6 dB;
  within the 3 dB window, so the test passes). Its LEVEL would be 103 to match: NOT changed (the user's words); for the user.
- EDS (new fuzz bass; a record in the table, ANALOG, ARP off): ATTACK 1, DECAY 60, SUSTAIN 90, RELEASE 14 | CUTOFF 40, RESONANCE 50, ENV AMOUNT +6 (70),
  TYPE LP | WAVE SUB (a saw plus a square an octave below), DTN 12 cents (the warble: the sub drifts against the saw's octave), SUB level 110 (strong),
  NOISE 0, DRIVE 30, KEY TRACK 20 (low: the filter stays dark up the keyboard), LFO RATE 0, PWM 0 | ARP defaults (off) | CHORUS 0 (depth 64, mix 0), TREM 0,
  PHASER 0, CRUSH 0 | DELAY mix 0 (time 1/8D, feedback 64) | FUZZ 100 | REVERB size 64, damping 64, mix 8 | LEVEL 94. There is no pitch-LFO depth in the
  engine, so the warble is the detune (the plan's first suggestion). It cannot play an octave lower than the key; the SUB oscillator is the low end
  (the OCT- buttons take it further). Judged by the numbers, not by ear (I cannot listen): the loudest 100 ms of the test chord is 985,424 against the
  median 970,535 (+0.1 dB), peak 6.1 M of 8.39 M (no clipping), every voice ends. LEVEL 94 is what vsynth_test LEVELS=1 names for it.
- ZUNJET: the row of the plan, typed as given (checked by tools/m101_test.py against the plan's text), in HAZE PAD's place. Its LEVEL 97 is the user's:
  vsynth_test LEVELS=1: loudest 100 ms 1,167,054 against the median 970,535 (+1.6 dB, inside the 3 dB window); its LEVEL would be 94 to match; NOT changed.
  Nothing in the firmware, the tests or docs/controls.md named HAZE PAD by position; the name is in docs/controls.md's preset list (Part 7).
- The stats: player_stats.cells_now (an edit page's four values as drawn) lets the test read E-GUITAR's and ZUNJET's pages.
