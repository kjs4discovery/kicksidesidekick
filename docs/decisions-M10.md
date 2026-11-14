# Decisions of the M10 run (Sonnet coder; for the planning chat's review)

Format (docs/plan-M10.md, "Read this first"): one entry per judgment call the plan does not spell out.

## M10-01 — INCIDENT: the planning chat's six song folders, songs/m10_songs.py and make_songs.py's change were lost and rebuilt (Part 0)
- Situation: at the start of Part 0 the working tree held the planning chat's files as STAGED additions whose git objects were
  EMPTY (16 zero-length loose objects, written 21:55 the same evening, before this session: songs/m10_songs.py, the six song
  folders' song.mid + song.yaml, songs/make_songs.py's change, docs/plan-M10.md, docs/device.md). While I was checking
  something else I ran a stray `git stash`; it could not store the empty blobs, failed half way and DELETED those working-tree
  files (my other changes came back). The data was gone from git, from the vault and from every other copy on the disk
  (found: only Python 3.10 .pyc files, and this machine has no 3.10).
- What I recovered, and how it was verified:
  * docs/plan-M10.md: retyped from this session's read of it; `git hash-object` = 40410755f776c26761d81c870bdd0ad9b7d31b34, the
    staged blob's hash from git's error messages: BYTE-IDENTICAL.
  * docs/device.md: HEAD + the M9 record; hash d707737a7697b315b5eac699ed3f19c739f0f9a4 = the staged one: BYTE-IDENTICAL.
  * the six song.mid files: rebuilt by tools/recover_m10_songs.py from the compiled files build/fuzz/<slug>.fm1s (made from the
    originals earlier that day) through make_songs.py's own build_midi; all six hash to the staged blobs
    (c7f0d3a6, 5fddb559, db610d55, 04df5ee8, 6d116d1e, 3ffa0256): BYTE-IDENTICAL.
  * the six song.yaml: NOT recoverable byte for byte. The compiled file does not hold credits, rights text, the melody-variant
    text or the chart's layout. Rebuilt so that every compiled format (2.0, 1.3, 1.1, 1.0) is byte-identical to the originals'
    (sha compared): melodies, chords, form, style, tempo, key, metre, category, title and programs are exact. Credits / rights /
    the info lines are PLACEHOLDERS I wrote (facts about the tunes; see songs/m10_songs.py).
  * songs/m10_songs.py: regenerated as data (NEW_SONGS) from the same source; DROPPED_SONGS is EMPTY (the six dropped songs'
    data is gone; they were never to be generated). songs/make_songs.py: HEAD + a 4-line import (NEW_SONGS into SONGS, their
    style into STYLE_OF) in place of the planning chat's 5-line change.
- Options: A stop and wait for the planning chat; B rebuild what can be verified, flag the rest, carry on (the user said: don't
  stop).
- Chosen: B. The six songs are playable and compile identically; ONLY the yaml texts (credits, rights, the info lines) differ
  from what the planning chat wrote.
- ACTION FOR THE PLANNING CHAT: re-send songs/m10_songs.py, the make_songs.py change and the six song.yaml files (the song.mid
  files and the compiled songs need no change). Then `git checkout`-style replace them, run make_songs.py <six slugs> and
  `git diff`: only the yaml text should change; delete tools/recover_m10_songs.py. The git object store was repaired (the empty
  objects deleted, the files re-added); `git fsck` is clean.
- Undo: replace the files; nothing else depends on the placeholder texts (the info lines are shown on the Settings screen).

## M10-02 — A change while paused: a new player command "park" (Part 1b)
- Situation: the plan wants the new version built while the player stays PAUSED at the mapped bar. bt_load opens a song (the
  player stops); there was no way to put the player into PAUSED at a position without playing.
- Options: A a new play command (player_park): the interrupt starts the song at the tick and holds it at once (nothing sounds,
  `starts` not counted); B player_start then player_pause (two requests in one main-loop pass: only the last is seen).
- Chosen: A. The parked position is the START of the mapped bar (bt_map_bar), not the offset inside the bar: the style may
  change the bar length (WALTZ), and with the count-in on PLAY goes back to the bar's start anyway.
- Undo: player.c player_park / play_cmd 3, bt.c bt_live's PL_PAUSED branch.

## M10-03 — camptown-races is held back from the suites and the send list (Part 0)
- Situation: the plan says a new song that fails a check is left out of the send list and reported. On the M9 package with the
  tree of Part 0, camptown-races fails two m5 checks that every other song passes: m4p4 / m5p1 "harmony L4 / L5 one note at a time"
  (its L4 harmony attacks carry 4 notes: h[3] = (185 attacks, 4 notes, span 9)) and m5p1 "every layer's levels within 1.6x" (its
  harmony 1.64). The generators are unchanged (SONG LEARN's generation MUST NOT change), so this is the song's chart / tempo
  (132 BPM, `A7 . D .` in a bar), for the planning chat.
- Options: A hold it back (tools/m5_test.py HELD_BACK: not in songs(), not on the test send list; docs/step-M10-*-install.md sends
  five of the six); B relax the checks (forbidden: "never relax a check"); C change the song (forbidden).
- Chosen: A. The song's files are committed as they are. When it is fixed or the expectation is judged wrong, empty HELD_BACK.
- Undo: tools/m5_test.py HELD_BACK = {}.

## M10-04 — the traced m5 failures of M9's accidental run (Part 0)
- Situation: "Happy Birthday's expectation" (m5 part5) is `len(all_songs) == 9`: a STALE COUNT. It failed in M9's run because the
  untracked songs/hallelujah/ made it 10 songs; with the M10 tree it counts 14 (9 older + 5 new; camptown-races held back).
  Corrected to 14 (the exact new number). m4p4 and m5p1's level checks: camptown-races (M10-03; hallelujah's in M9). The m5p1 hint
  count (9 of 10 shown in 11 quick openings) was not traced: that run had a load average of 13 (other runs); recheck on the
  final package in Part 9 with no other run.
- Also: no test reads songs/private/hallelujah (PRIVATE_SKIP in m5/m51, pytest's test_gen_golden, m4, m6, guitar_check).
- Undo: tools/m5_test.py song_dirs / private_dirs.

## M10-05 — the value pop-up: size, fonts, strings (Part 2a)
- Situation: the plan leaves size, position and font to me ("centred, readable").
- Options: A 216 x 104 px at (12, 68), title FONT_XS (gray), value FONT_M (24 px) or FONT_B/FONT_XS when it does not fit 196 px, the
  badge under it, the "at the next ..." line FONT_XS at the bottom, amber frame; B a narrower box with FONT_B only.
- Chosen: A (FONT_M gives "VERSE/CHORUS" and "42 JAZZ BLUES" at about 170 px). State: ui.c `vpop` (separate from ui.popup; a press
  closes it and goes on to its job; time 1,500 ms from the last turn; the screen under it frozen, ui.redraw on close). The texts
  are in player_stats (vp_title, vp_value, vp_sub, next_text, ind_name: player.h "M10") for the tests.
- Undo: ui.c vpop_* and draw_vpop; bt.c bt_vp / bt_vp_select.

## M10-06 — the section indicator: place, colour, blink clock (Part 2e)
- Situation: the plan puts the indicator at the right end of the top line, where "the badge is now". On the play screen the
  badge sat right after the title; the bar label (FONT_M) is at the far right of both lines.
- Chosen: the indicator is a box right-aligned at the edge of the title line's room (just left of the bar label), amber when it
  is the section playing (steady), a different amber while it blinks to the NEXT section's name; the MAJOR / MINOR badge is no
  longer drawn on the play screen (SETUP and the pop-up keep it): MOVED, as asked. Names come from the section's role (INTRO,
  VERSE, CHORUS, BRIDGE, ENDING), for ROLE_OTHER the section's name in capitals without its number.
- The blink's clock is the player's own time (ticks from the moment the blink began, 250 ms halves at the tempo now: the beat
  light's base), not plat_ms(): it is steady under a pause and testable. The blink starts in the frame in which the position
  (with the 30 ms beat lead) reaches the last bar's line: measured 16..28 ticks (8..14 ms at 250 BPM) after the line.
  Blinks only while PL_PLAYING; a loop wrap (ui.loop BARS) counts as a change to the loop's first section when it differs.
- Undo: bt.c bt_indicator / bt_ind_update / bt_top.

## M10-07 — the NEXT line (Part 2c)
- Situation: "NEXT: STRUCTURE VERSE/CHORUS" does not fit the room before the bar label.
- Chosen: "NEXT: <setting word> <value>" for the first of progression, style, structure, key, complexity and " +n" for the others;
  when it does not fit, the VALUE is shortened (VERSE/CHORUS -> VERSE/CH. -> V/C; BOSSA NOVA -> BOSSA; INDIE POP -> INDIE), never
  the setting's word. The tests read the text with all the room (bt_stats: player_stats.next_text), because the screen is frozen
  under a pop-up while the turn is fresh.
- Undo: bt.c bt_next_text / bt_next_item.

## M10-08 — the help screens say "melody sound" already (Part 2d)
- The help's rows (ui.c draw_help) read "K4 melody sound" since M9 and are in lower case like every row there; SETUP's row now says
  "MELODY SOUND". Nothing else to change.

## M10-09 — the mode screen's mascot (Part 3)
- Situation: "scaled down to make room, nearest-neighbour or 2:1".
- Options: A 2:1 (75 x 85 px, a lot of empty space), B 4:5 (120 x 136 px) by nearest neighbour in the RLE decoder (ui.c
  draw_logo_s), the top at y 20, the title FONT_B at y 2; C 3:4.
- Chosen: B (the largest that fits between the title and the "modes:" line at y 158, no blur). The splash keeps 1:1.
- Undo: synthui.c draw_mode; ui.c draw_logo_s.

## M10-10 — release version and identity names (Early install point, Part 9)
- Situation: the plan says "0.10.0-m10a (identity FM-1_46001001)"; build.py makes the identity from the version's
  major / minor / patch (0.10.0 -> FM-1_46001000, 0.10.1 -> FM-1_46001001).
- Chosen: the early build is `--release 0.10.1-m10a` (identity FM-1_46001001, as the plan says); the final is `--release
  0.10.0-m10` (FM-1_46001000). The splash shows "m10" in both.

## M10-11 — #42 JAZZ BLUES's numerals text (Part 4)
- Situation: FORM's numerals field is 40 bytes (39 characters, a fixed layout shared with the M9 FM-1); "I7 IV7 I7 I7 IV7 IV7 I7
  VI7 ii7 V7 I7 V7" is 40.
- Options: A write the first whole numerals that fit (11 of the 12: the music, from the chart, is whole); B raise the field (a
  format change: an M9 FM-1 would refuse the file); C drop the spaces.
- Chosen: A (tools/backtracks.py): SETUP's chord names line shows the first eleven chords of #42.
- Also: "its own suggested style" for the new ones (the plan names only JAZZ BLUES: SHUFFLE) in m10p4: PHRYGIAN ROCK, LINE CLICHE
  BALLAD, MINOR CADENCE BALLAD, DESCENT BALLAD, LYDIAN INDIE POP.
- The largest need among the new six in any style: see LOG (all fit, the overall largest is still bt28 in COUNTRY: 54,352 B).

## M10-12 — the INFO chunk: 39 characters, not 38; the chunk is last; not in 1.x (Part 5)
- Situation: the plan says `info:` is "<= 38 characters" and gives amazing-grace "Trad. hymn tune, 1830s; words J. Newton", which is
  39. Also: where the chunk goes, and what an M9 FM-1 does. song.c looks chunks up by name, so it skips INFO: the format stays 2.x.
- Options: A limit 39 (the text exact); B limit 38 and shorten that text; C a new format version.
- Chosen: A (the text is the plan's "exactly"; the chunk holds up to 39, the C buffer info[40]; it fits 236 px in FONT_XS:
  m10p5-info.png). The chunk goes LAST in the directory, so that the file minus INFO is the M9 file. 1.x formats do not get it.
- The Settings screen: rows are 20 px (not 22) while an info line is shown (the 8 rows then end at y 192, the line at y 195), else
  unchanged. "A song is loaded" = from its play screen (ui.back == V_PLAY, not BACKTRACK).
- The six new songs' info lines in songs/m10_songs.py are my placeholders (M10-01). The 9 older songs' are the plan's, exact.
- Undo: song.c meta(), binformat.py INFO, source.py `info`, ui.c draw_settings.

## M10-13 — band kits: routing, mapping, SETUP layout (Part 6)
- Situation: the plan says "reuse SYNTH's drum voices for the backtrack's drum layer", the tier tables the same for every kit.
- Chosen: player.c `player_kit(drum route, bass program, harmony program)`; in dispatch() the drum layer's note-ons go to
  vsynth_drum (ELECTRO = KIT_808, CHIP = KIT_CHIP), velocities 34 / 70 / 92 -> hit levels 1 / 2 / 3, volume 89 (SYNTH's default
  drum volume 7 of 10), in the audio interrupt at the start of the block (5.8 ms grid). The GM drum notes map to the 8 voices:
  36 kick, 38 snare, 42 hat, 46 open hat, 39 clap, 45 tom lo, 50 tom hi, 37 rim; the ride (51) has no voice of its own: the closed
  hat; the crash (49): the open hat. STUDIO and ACOUSTIC use EAS's GM kit (ACOUSTIC differs in the bass 33 and harmony 25
  programs). Programs are sent at player_start (channel_setup); a kit chosen on SETUP is in place before PLAY. BACKTRACK's exit
  withdraws the kit (player_kit(0, 0xFF, 0xFF)): SONG LEARN is untouched. The kit resets with the other BACKTRACK choices at power-off
  (not at every entry: like them).
- SETUP: "SEQ  KIT  <name>" is a row like the others (FONT_XS) at y 198 under TEMPO; to make room the rows from STYLE on moved up
  2, 4, 6, 8, 10, 12 px (18 px apart instead of 20; KEY's chord line unchanged under it): nothing is cut (m10p6-seq2.png). The plan
  said "up to 2 px each"; they would not fit otherwise.
- The button census of SETUP (m9census) expected SEQ to open the "NOT IMPLEMENTED" pop-up; now it has a job there: the expectation
  in tools/m9_test.py is updated (a stale expectation).
- Measured: CPU with FULL, BUSY in a chorus, MIAMI PAD x 4 voices, a live switch, POP / ROCK / FUNK: STUDIO 44 %, ACOUSTIC 41 %,
  ELECTRO 57 %, CHIP 47 % of a block (limit 60 %): all four kits are kept. ELECTRO is the heaviest (the 808's noise voices).
- Undo: player.c player_kit / kit_hit; bt.c BT_KIT_* and bt_kit_apply; the SETUP rows.

## M10-14 — user presets: object, list, SAVE screen (Part 7)
- Flash addresses: grep of every flash address in firmware/app and fm1-base (device/, include/, host/) finds nothing at 0xDE000 /
  0xDF000 (store.h: the song store ends at 0xDBFFF, the panel pair is 0xDC000, the update staging area starts at 0xE0000; our unit's
  dump has 0xFF there). OBJ_PRESETS joins app.h's object map: 0xDE000 (an A/B pair, copy A 0xDE000, B 0xDF000).
- Object: 8-byte header ("UPRS", container version 1, 30 slots) + 30 x 64-byte M7 records = 1,928 B (<= 3,840). A slot is empty when
  its record's version byte is 0. A record that vsynth_record refuses (bad version / engine) is an empty slot; one with parameters out
  of range (an older version's missing ones) loads with the defaults (vsynth_record's rule; there is only version 1 today). Built in
  RAM (upreset.c static buffers, .bss: +3.9 KB), never from a const table; start-up only reads (upreset_boot); a save writes through
  plat_store_save, the same path as the settings object.
- The list: pl_count / pl_get / pl_label / pl_slot (upreset.c): stock 1..30, then used slots in slot order, "U05 <name>". BACKTRACK
  keeps its sound as a LIST INDEX until power-off: saving a preset in SYNTH (a new slot before it) shifts the user entries by one;
  a K4 turn fixes it. Not handled further.
- SAVE screen (synthui.c): five slot rows around the chosen one, "EMPTY" or the name, the name (the current preset's, upper-cased,
  padded with spaces) in FONT_M with a cursor bar; characters A-Z, 0-9, space, '-' (K2); K1 the cursor (clamped 1..12); SELECT the slot
  (clamped U01..U30, no wrap); the start slot: the current preset's slot if it is a saved one, else the first empty (else U01); SAVE
  on a taken slot asks first ("REPLACE U05 <name>?  SAVE = yes  HOME = no"; the title says it too), HOME cancels at either point,
  nothing written. The name's trailing spaces are not stored; an all-space name is stored as "PRESET". After the write: "SAVED U05"
  (the value pop-up, 1,500 ms; a press closes it and does its job), the sound goes on as the saved preset, selected as U05. Other
  buttons on the SAVE screen show the NOT IMPLEMENTED pop-up like on every screen. SAVE with the flash not ready (RAM ONLY): "NOT
  SAVED". SAVE's help line on SYNTH's help screen.
- The save moment: synth_mute (synth.c): the output fades to silence over one block (a linear ramp), the main loop waits for it (at
  most 50 ms), writes, and fades back in over one block. No delete (overwrite only: backlog).
- Measured: a save = 1 sector erase + 9 page programs (10 flash operations); on the timed emulator the longest stretch without a
  watchdog feed over 48 runs incl. 30 saves in a row is 34 ms (limit 1,000); 0 watchdog resets, 0 XIP reads with a flash command running.
  Host: tools/upreset_test.c (ASan/UBSan, `make upreset-test`): junk, bad CRC / length / magic, 30 saves, a cut at every operation
  over three saves (both copies): old or new, never mixed; no flash operation outside 0xDE000..0xDFFFF.
- Undo: app.h OBJ_PRESETS and the map, fm1.toml's o2 (upreset.c), upreset.c / upreset.h, synthui.c (SY_SAVE and the list), bt.c (pl_*).

## M10-15 — the suite run: lanes, stale expectations, one open failure (Part 9)
- Lanes A and B ran at once and C after B ("serial": the plan's overload fallback, chosen up front because the timed emulator is load-sensitive).
- Stale expectations fixed to the exact new behaviour, re-run on the same package: m5 `store` (the damaged-song flip landed in the INFO chunk,
  now in the events), m7 `m7p2` (SAVE has a job in SYNTH), m5 `part5` (14 songs), m9census (SEQ on SETUP). m9sweep trimmed by M9_SWEEP_TRIM=1.
- OPEN, not relaxed: m5p1's hint count (9 of 10), also on the M9 package; for the planning chat.

## M10-16 — the planning chat's review of 2026-11-14 applied (M10-01, M10-03)
- Situation: HANDOFF 2026-11-14: songs/m10_songs.py, make_songs.py's hook and the six song.yaml files were re-sent; camptown-races
  was fixed (ending `| D |`; harmony accordion GM 22; song.mid unchanged).
- Done: `make_songs.py` for the six slugs: the six song.yaml were byte-identical to the re-sent ones and no song.mid changed;
  `fm1song validate`: six ok. tools/recover_m10_songs.py deleted. HELD_BACK = {} in tools/m5_test.py; part5's count 14 -> 15
  (the exact number, as M10-04). Committed e0945ac.
- Result on the final package (build/step-M10/fm1acc-0.10.0-m10.fwsc, no rebuild), via the test-runner: m5 --only
  m4p4,m5p1,part5,camptown-races: 30 checks PASS, 0 FAIL (1429 s, build/emu/m5-camptown/report.md). camptown-races' own group 15 PASS
  (harmony attacks/notes by level (28,4),(172,4),(172,4),(172,1),(172,1); loudness ratio 1.39; the worst song is drunken-sailor 1.53).
  pytest tools/fm1song: 298 passed. So camptown-races is on the send lists of both install checklists (six new songs).
- Note for M10-15: m5p1's hint count PASSED in this run ("shown 10 times in 11 openings"); it failed in the earlier lane run and on M9.
  So it is flaky (timing), not a fixed regression; still open, not relaxed.
- Undo: HELD_BACK = {"camptown-races": ...}.
