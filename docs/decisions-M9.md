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

## M9-02 — The count-in rule: a `counting` flag; K4 is not a second path (Part 1)
- Situation: the plan suspected a second path for the K4 report. The failing checks (m9p1) showed K4 / K1-K3 turned while playing
  never count in (count-ins 1, before and after); only the loop wrap after a pause / resume or a later start point did.
- Options: A the plan's suggested flag `counting` (set by start() when a count-in is scheduled, cleared when the position reaches
  music_from; the click test and the PL_COUNT_IN test need it); B set music_from back to the loop's start at every wrap.
- Chosen: A (the plan's suggestion; B would also move the "from here" shown on the screen).
- Undo: player.c `counting` (3 places); player_stats.countins can stay.

## M9-03 — The importer's key signature decides the spelling only (Part 2)
- Situation: smf.py's key signature (a = sharps / flats, b = minor) was read as bytes (never found) and in any case never used;
  the plan says "fix both to read a and b".
- Options: A the signature decides sharps / flats of the key name and the chords, the key itself stays by the notes; B the
  signature decides the key too.
- Chosen: A: a signature is often wrong or generic in game / pop MIDIs, and detect_key's docstring already said "for the
  sharps / flats only". Test: a file in F# major with 6 flats imports as Gb.
- Undo: importer.py import_file (`flats = kmeta[0] < 0`), key_name's `flats` parameter.

## M9-04 — A 6/8 file's report says what it was read as (Part 2)
- Situation: with the time signature read, a 6/8 file imports as 2/4 (the importer's own compound rule); its report line said
  "read as 6/4".
- Options: A print the metre used; B leave the text.
- Chosen: A (the text was wrong once the signature was read). Hallelujah (import/test) is the one file whose song changes
  (4/4 -> 2/4); no repo or private song changes.
- Undo: importer.py, the `rep.append(f"time signature ...")` line.

## M9-05 — The mode screen's "modes:" line (Part 2)
- Situation: the plan: FONT_XS, x 10, the subtitle colour, just above the first choice, move the rows down if needed.
- Options: A the line at y 158 (the mascot ends about y 155), the rows unchanged (they end at y 240: there is no room to move);
  B shrink the rows.
- Chosen: A; the colour RGB(120, 108, 94) is the unselected subtitle's.
- Undo: synthui.c draw_mode (the `cv_begin(120, 14 ...)` block).

## M9-06 — The LFO light in BACKTRACK (Part 2)
- Situation: the melody has no layer; LFO held makes it yours. "ON steadily" and "shows as SONG LEARN shows 'your part'".
- Options: A lit (and dim base) like a layer that is on, in lights(); the layers row already shows MELODY / YOU (M8);
  B a blinking light.
- Chosen: A (SONG LEARN's layer lights do not blink).
- Undo: ui.c lights(), the `if (bt.on)` block.

## M9-07 — A section's crash also puts a kick on beat 1 (Part 3)
- Situation: "a crash (49, vel 92) with the kick on beat 1 of the first bar of every section except INTRO". Some tier tables have no
  kick on beat 1 (REGGAE's one-drop).
- Options: A crash and a kick (92) both added at the section's first bar (the literal reading); B only the crash, with whatever
  the pattern has.
- Chosen: A. Where the pattern has a kick there already, canonical keeps the louder one.
- Undo: gen.c bt_drums and arrange.band_drums, the `lv >= 2 && b == first` block (drop the KICK line).

## M9-08 — The fill silences kick, snare (replaced) and both hats (Part 3)
- Situation: "the bar's last beat is replaced by a snare fill ...; kick and hats silent in that beat". Rim, clap, ride and tom lo
  rows are not named.
- Options: A silence K, S, H, O in that beat (S because the fill is the snare), keep R C Y T; B silence everything but the fill.
- Chosen: A (the plan names exactly kick and hats; the fill is the snare).
- Undo: the `r < 4u` / `row in "KSHO"` tests in bt_drums / band_drums.

## M9-09 — Which bars get the fill (Part 3)
- Situation: "the last bar of every section that is followed by another section (not ENDING)".
- Options: A the section is not the last and not an ENDING itself; B the following section must not be the ENDING.
- Chosen: A (an ENDING section is the last anyway; the bar before it gets its fill, as a drummer would).
- Undo: `lastbar` in bt_drums / band_drums.

## M9-10 — The tier grid is not swung (Part 3)
- Situation: SHUFFLE is `steps 3 swing`; the old generators swing a straight grid when the song's swing flag is set.
- Options: A the table's steps are the swing (triplet grid), no extra swing; B swing_tick on top.
- Chosen: A (B would swing a swing). song.swing stays set for SHUFFLE (song_restyle) but gen.c bt_* ignore it.
- Undo: gen.c make() `o->swing = 0` in the backtrack branch; arrange.py _Out(False).

## M9-11 — Voicings, holds, note lengths (Part 3)
- Situation: details the plan leaves: UPPER with fewer than 2 notes; what l / m / h are with 3 notes; what a '-' at the start of a
  bar does; how long a drum note is; whether the 15-tick shortening applies.
- Options / chosen: UPPER = the WIDE voicing without its lowest note when it has 2 or more notes; l / m / h = sorted WIDE
  voicing [0], [n/2], [n-1] (as the old broken chords); a '-' holds the previous hit of the same bar (a leading '-' is silence);
  a hit is cut at its chord's end; harmony and bass notes go through note() (ended 15 ticks early, as every generated note), drum
  notes are one step long exactly (raw), the final bar's kick and crash 60 ticks as in M8.
- Undo: gen.c / arrange.py, the three bt_* / band_* functions.

## M9-12 — A backtrack has 4 levels in each generated layer; no format change (Part 3)
- Situation: the plan says "no format change", but LAYR's level count was checked against 3 / 4 / 5.
- Options: A kind-aware check (song.c layer2 `gen`, binformat.py, source.py): 4 for a song of kind backtrack, 3 / 4 / 5 for a song;
  B store 5 / 4 / 3 and play 4.
- Chosen: A. The format version and chunks are the same (2.2). Consequence: the 30 backtracks M8 put on the unit (3 / 4 / 5
  levels) are refused by the M9 firmware when opened ("BACKTRACK ERROR LAYERS") until they are sent again; the install
  checklist (step-M9-install.md) sends all 36 (the same slugs are replaced) straight after the install.
- Undo: those three places.

## M9-13 — Progression names (Part 5)
- Situation: "No progression is named after a song or a trademark".
- Doubts: CANON is the name of a piece (Pachelbel's Canon, in the public domain; the progression is generally called that);
  FIESTA is a common word (also a car's name); ROYAL ROAD is a Japanese music-theory term, not a song. Nothing else.
- Options: A keep all three for the user's ear to decide; B rename CANON.
- Chosen: A (the plan told me to record doubts, not to rename).
- Undo: tools/backtracks.tsv (and `backtracks.py`; git rm the old folder).

## M9-14 — The two M8 tests changed (Part 3)
- Situation: M8's test_m8_restyle_on_the_fm1_is_a_recompile compared the FM-1's restyle of a POP *backtrack* with a *song*
  compiled in that style: by design they differ now. M8's energy tests used a song with energy: refused now.
- Options: A the restyle test on a song (the old code path), energy tests on backtracks, the backtrack restyle in
  test_band_styles.py (C = Python for 11 styles); B delete them.
- Chosen: A.
- Undo: tools/fm1song/tests/test_gen_golden.py.

## M9-15 — The switch: one mechanism for every change (Part 4)
- Situation: the plan's table has a bar class (complexity, key) and a phrase class (progression, style, structure).
- Options: A every change goes through the same prepare-in-the-other-half and player_swap at a bar line (a key-only change swaps the
  same song with another transposition; a structure-only change likewise); B complexity through lib_level (a held layer) and key
  through player_transpose.
- Chosen: A: B would lose the notes at the bar line while a layer is held, and release notes late.
- Undo: bt.c bt_plan / bt_live; player.c player_swap.

## M9-16 — When a change is made (Part 4)
- Situation: "the next phrase boundary" / "the next bar line", "several waiting changes are applied together".
- Options / chosen: the knobs still for 300 ms, then the boundary is chosen at least 250 ms ahead (the build takes about 60 ms) and,
  after the build, asked of the player for the first line at least 100 ms ahead; changes of both classes due at the same line go
  together; if the bar class is due first it goes alone and the phrase class waits (a second switch). If no boundary is left (the
  song ends first) nothing is built. A request the position passed is dropped by the player and planned again.
- Undo: BT_IDLE_MS, BT_PLAN_MS, BT_GUARD_MS and bt_plan.

## M9-17 — Changes while stopped or paused (Part 4)
- Situation: the plan describes live changes while playing only.
- Options: A made at once (the knobs still 300 ms): the band is loaded again in the other half and the player stopped (a pause is
  lost; PLAY starts from the loop's start or the start point); B wait until PLAY.
- Chosen: A (what the screen shows is what the next PLAY plays).
- Undo: bt_live's `st == PL_STOPPED || PL_PAUSED` branch.

## M9-18 — "The same position in the form" (Part 4)
- Options / chosen: the section index and the bar in the section (the new file has it if the section exists and is long enough);
  then the structure's rules: inside the new loop play on, else the loop's start; FULL without the bar: the first VERSE. At a
  loop wrap the bar that carries on is the loop's first.
- Undo: bt_map_bar.

## M9-19 — input_play must not read BACKTRACK's knobs (Part 4)
- Situation: my first version consumed K1-K4, PRESETS and ALGO in bt_play_input but input_play still read them after it: a turn that
  arrived between the two reads (during bt_live's build, for one) went to the harmony level / the start point. m9p4 found it:
  about 6 % of knob turns lost, the same script passing or failing with the build. The plan's hint (a flag input_play checks)
  was the cure.
- Chosen: input_play does not read LAYER_ENC, PRESETS, ALGO when bt.on; SEQ and SAVE are taken out of `down` and are not "jobs" (the
  pop-up). 30 of 30 turns arrived afterwards.
- Undo: ui.c input_play (`bt.on ? 0 : plat_enc(...)`), learn_jobs, the call site.

## M9-20 — Screens (Part 4)
- Situation: the plan gives the rows and says to record the layout.
- Chosen: SETUP: header; K1 PROGRESSION label and badge (y 27), the name FONT_B (41), the numerals FONT_XS (62); then FONT_XS rows
  at y 80 STYLE, 100 COMPLEXITY, 120 SOUND, 140 KEY (+ its chords at 154), 172 STRUCTURE, 192 TEMPO. Labels of the encoders PRE / ALG /
  SEL. Nothing is cut. MINOR badge RGB(96, 156, 240) (QUIET is dark amber in this firmware, not blue), MAJOR the amber highlight,
  both with their word. Play top line: line 1 "<number> <name>" and the badge; line 2 NEXT: ... (as many items as fit, then "+n"),
  else the message, else "STYLE  STRUCTURE  Cn" (no tempo or key there: the width; the tempo is SELECT's message). The layers row
  shows L = the level + the section's energy (a chorus one more) and redraws when the energy changes. The play legend has no SEQ;
  help screens: ENC_SETUP, BTN_SETUP, ENC_BT, BTN_BT.
- Undo: bt.c draw_setup / bt_top, ui.c draw_layers, legend_play, draw_help.
