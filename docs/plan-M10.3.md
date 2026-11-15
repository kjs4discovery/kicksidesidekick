# Milestone M10.3: instructions. Your M10.2 notes: BACKTRACK fixes, SONG LEARN text and keys, POTATO, the new artwork, open-source files

From the planning chat, 2026-11-15. Scope: the user's M10.2 notes (vault Brainstorms/2026-11-04 Prototype feedback,
"2026-11-15 — M10.2 build"), vault decision D072. **Not in M10.3:** the built-in starter library (vault D070 option 1);
it changes how every song is listed and opened and gets its own milestone (M10.4) with emulator tests.

State at the start: the user installed 0.10.3-m10.2 (FM-1_46001003) and used it. Record that in LOG and device.md
first (install details: not given; write "installed and used; notes in the vault").

Version: **0.10.4** (`--release 0.10.4-m103`; no dot in the suffix, M10.1-15). FW_VERSION "0.10.4-m10.3", FW_MILESTONE
"m10.3". Package `build/step-M10.3/fm1acc-0.10.4-m10.3.fwsc`; identity FM-1_46001004 if the scheme holds. Record what you get.

## Read this first (the coder is Sonnet)

This plan is deliberately prescriptive. Follow it literally.
- **MUST / MUST NOT** are hard rules. Everything else is the intended design; if it turns out impossible or clearly
  worse in the code, choose the SMALLEST change that does the job, then record it.
- **docs/decisions-M10.3.md (new; you write it)**, same format as docs/decisions-M10.2.md, numbered M10.3-01, ...
  Every judgment call this plan does not spell out goes there.
- **MUST NOT** change: the song store's flash code, the update loader, the rescue combos, the factory reset, the
  hardware safety rules in CLAUDE.md, the settings record's layout and size, the user presets already saved on the
  unit, any song file (no song changes in M10.3), any vendor file's own licence header or text, `firmware/vendor/`
  code.
- **MUST NOT** refactor code a part does not need. No renames of existing functions or files (new ones are fine).
- **No test cycle (the user's choice, D072, as D069).** No full suite, no fast lane, no emulator runs, no new tests, no
  screenshots. What stays: the firmware builds with no new warnings; `pytest tools/fm1song` passes; `songs/make_songs.py`
  for ALL slugs changes NO file; the image previews of Part 4 (a PNG written by the converter, no emulator). Where a
  changed text or behaviour is named in an existing test's expectations (grep tools/m*_test.py, e.g. "Band Kit",
  "NO SONGS YET", "Send songs from the computer", "NO BACKTRACKS", APP_NAME on the splash, "tmp-logo", LOGO_W), update
  the expectation so a later suite does not fail on stale text; do not run those tests; list each change in
  decisions-M10.3.md.
- **Never run a device command.** End by writing docs/step-M10.3-install.md and STOP.

## PART 0 — Housekeeping
- LOG + device.md: the M10.2 install (above). Create docs/decisions-M10.3.md.
- Commit the planning chat's uncommitted files first, in their own commit: docs/HANDOFF.md, docs/plan-M10.2.md,
  docs/plan-M10.3.md. The user's `import/` stays untracked. `docs/dropbox/` is handled in Part 4.

## PART 1 — BACKTRACK
1. **The section indicator leaves LOOP at once.** Today (bt.c bt_ind_update, bt_top) the indicator reads "LOOP" while
   `bt.structure == BS_LOOP`, and bt.structure only changes when the swap is adopted at the next phrase boundary, so
   after ALGO picks VERSE/CHORUS or FULL the label keeps saying LOOP until the next section. Change: the indicator
   reads LOOP only when BOTH `bt.structure` and `bt.w_structure` are BS_LOOP; otherwise it shows the section playing
   now (bt_section_name, as for the other structures), from the frame ALGO is turned. Same rule in bt_ind_update
   (`bt.ind_loop`, player_stats.ind_name) and bt_top. Make sure the top line's redraw hash changes with it. The
   switch itself (at the phrase boundary) is unchanged. Blinking: as the other structures do today.
2. **HOME while playing or paused: straight back to SETUP.** On BACKTRACK's play screen (bt.on), a HOME tap in ANY
   player state (playing, count-in, paused, stopped) stops the backtrack and goes to SETUP in one press: the same path
   as today's stopped branch in input_play (`ui.view = V_SETUP; lib_release(); ui.redraw = 1`), after stopping the
   player the way that branch expects (lib_release waits for the player to be idle). SONG LEARN's HOME is unchanged
   (playing: back to the start point; stopped: the list). HOME held 2 s (to the mode screen) is unchanged.
3. **SEQ steps the band kit while playing too.** Why SEQ is lit today: lights() marks SEQ "has a job" on every play
   screen (it is SONG LEARN's loop button) and BACKTRACK never took it out. Now give it the job: on BACKTRACK's play
   screen SEQ steps the kit exactly as on SETUP (`bt.kit = (bt.kit + 1) % BT_KITS; bt_kit_apply(1); bt_vp(VK_KIT)`),
   acted on at once (no phrase boundary: nothing is rebuilt). learn_jobs: V_PLAY keeps SEQ when bt.on. The drums change
   from the next hit (player_kit already sets kit_dr). Bass and harmony: player_kit today only takes effect at
   channel_setup (song load), so add a "programs due" flag that player_kit sets and the player's own audio-side code
   acts on at its next event: send 0xC0 for layer 1 and layer 2 with the kit's program, or the layer's own program
   (`ly->program`) for 0xFF (STUDIO), skipping a layer without variants. Notes already sounding keep their old sound
   until released (that is fine). MUST NOT touch the swap / phrase-boundary code. Help (BACKTRACK's play-screen BTN
   table) and its play legend: SEQ "band kit". bt_play_input's comment ("SEQ and SAVE have no job here") is updated.
4. **"Band Drum Kit".** The kit pop-up's title (bt.c bt_vp_select, VK_KIT) becomes "Band Drum Kit" (user's text). The
   SETUP row label "KIT" stays. Value unchanged ("ELECTRO").

## PART 2 — SONG LEARN
1. **The end of the song list.** In the song list (V_LIST: "All songs" and every folder), after the last song a footer
   that the cursor never lands on, centred, FONT_XS, C_GRAY: "Download and install more" / "songs from our website" (the
   user's sentence on two lines; one line if it fits 228 px in FONT_XS). When the cursor is on the last song the list
   scrolls so the footer is fully visible; a list shorter than the screen shows the footer under its last song. The
   folders screen gets no footer. Not a pop-up (Pop-ups OFF does not hide it).
2. **No songs yet / no backtracks (vault D070 option 4).** draw_list_empty (the flash-OK branch): keep "NO SONGS YET";
   replace the three laptop lines with "Please visit our website" / "to download songs." (FONT_XS, C_GRAY, same
   positions); keep "They stay here when it is off." in C_DIM. bt.c's "NO BACKTRACKS": the same two lines instead of
   "send songs/backtracks/ from" / "the laptop (in SONG LEARN)". The "NO SONG STORAGE" (RAM ONLY) screen is unchanged.
3. **The keys play the melody's sound.** Today SONG LEARN's keys always play GM on SYNTH_KEYS_CH (ui.c input). Change:
   - when a preset is the melody sound (`lm_active()`), the keys play it: `vsynth_key(note, 1/0)`, as BACKTRACK does.
     The keys share LM_VOICES (4) with the melody layer; keep 4 (CPU), record it. A key released after the preset was
     switched off or the song closed must not hang: release through the same route it was pressed on (remember the
     route per key beside `ui.key_note[k]`).
   - when the melody sound is SONG: when a song opens and when K4 returns to SONG, send a program change on
     SYNTH_KEYS_CH with the song's melody layer program (`ui.s->layer[L_MELODY].program`; if 0xFF or the song has no
     melody layer, the keys' default program). When the song closes (back to the list), the keys' default program
     again.
   - the song list and the folders: the keys as today. BACKTRACK unchanged. Help/legend texts "keys play along" stay.
4. **The Start pop-up's section name, bigger.** say_from's pop-up ("Start" / "BAR 9" / "Chorus 1"): the section name is
   drawn in FONT_B, C_WHITE, about 6 px under the value (not at the sub line's y 84 in FONT_XS, C_DIM). Only for this
   pop-up (a flag in vpop set by say_from, cleared by every other vpop_show / vpop_keep). If the name is wider than
   208 px in FONT_B, FONT_XS in C_WHITE. Every other pop-up's sub line unchanged ("Track Selected" keeps its small
   line). Must stay inside the 104 px box.

## PART 3 — POTATO: CRUSH 127
- vsynth.c PRESETS, the "POTATO" row: VP_CRUSH (the 28th value: the 4th of the FX 2 group, the 8th number on the
  row's second line) 0 -> 127. Nothing else in the row, LEVEL included (no loudness run; the user listens). Record the
  row before and after. A user preset saved from POTATO earlier is a copy and does not change. m101_test.py's "changed
  vs M10" list gets POTATO (not run).

## PART 4 — The new artwork (docs/dropbox: "Retro Logo2.jpg", "KickSideMasscot.jpg", the user's own)
1. **Assets.** Move (git mv / mv) the two originals to `firmware/assets/src/kickside-logo.jpg` and
   `firmware/assets/src/kickside-mascot.jpg`; docs/dropbox is then empty (leave the folder). `firmware/assets/tmp-logo.jpg`
   stays in the repo, no longer built (record it). Both are the user's: Part 5's REUSE.toml marks them as not under
   the open licences.
2. **tools/logo_to_c.py** gets three options, defaults keeping today's behaviour: `--palette "R,G,B;R,G,B;..."` (map to
   exactly these colours, no dithering, instead of MAXCOVERAGE: on these images MAXCOVERAGE turns the cream background
   grey), `--prefix NAME` (the macros and arrays become NAME_W, NAME_H, NAME_BG, NAME_PAL, NAME_RLE; default LOGO), and
   `--preview OUT.png` (decodes the RLE it just wrote back to a PNG, exactly what the firmware will draw). The palette
   the planning chat tested (sampled from the images): `253,239,202;18,32,18;254,180,59;240,140,40;230,87,27;255,250,235;200,60,20`
   (cream background first, ink, orange, mid orange, red-orange, light cream, deep red). Measured by the planning chat
   with this palette: the logo at 240 x 160 about 7.2 KB, the mascot at 120 x 136 about 2.7 KB (today's logo 4.4 KB:
   about +5.5 KB of app space). Background: the top-left pixel, as today; both images' corners map to the cream entry,
   so both share one background colour.
3. **fm1.toml gen**: two lines: the logo `--width 240 --prefix LOGO` -> `{gen}/logo_gen.h`; the mascot
   `--width 120 --prefix MASCOT` -> `{gen}/mascot_gen.h`; both with the palette above. Write the two previews to
   `docs/step-M10.3/splash-logo.png` and `docs/step-M10.3/mode-mascot.png` once (not from the build) and look at them
   (Read the PNG): the cream background whole, the outlines unbroken, the text "KickSide" and "Your Musical SideKick"
   readable. If a colour is clearly wrong, adjust the palette by sampling the original, re-run, record.
4. **Drawing.** draw_logo_s draws from a small descriptor (W, H, BG, PAL, RLE) passed in, so one function draws both;
   draw_logo and the mode screen pass theirs. The mascot is stored at its display size: draw it 1 : 1 (no 4 : 5 scaling).
5. **Splash** (app_splash): background LOGO_BG; the new logo (240 x 160) at y 36; **no APP_NAME line** (the logo has the
   name); the version, `"v" FW_VERSION`, FONT_XS, right-aligned at x 234, its baseline about 6 px above the bottom edge,
   colour RGB(150, 136, 118). SPLASH_MS unchanged. APP_NAME stays defined (help, crash / safe-mode screens).
6. **Mode screen** (synthui.c draw_mode): the mascot at y 20 where the scaled logo was (120 x 136 ends at 156, above
   "modes:" at 158), background MASCOT_BG (the same cream). The title across the top, "KickSide - Your Musical SideKick",
   with **"Kick" bold in both words**: "Kick" in FONT_B, the rest in FONT_S (check in tools/gen_font.py that FONT_S is
   the regular weight of FONT_B's family; if it is not, the regular 16 px font that is), drawn as segments
   ("Kick" | "Side - Your Musical Side" | "Kick"), the whole line centred. If the line is wider than 232 px, fall back to
   FONT_XS for the regular parts and keep "Kick" in the smallest bold that keeps the line within 232 px; if there is
   none, FONT_XS throughout and record it. Never cut the text.

## PART 5 — Open source (vault D071; GitHub Open-Source Prep section 3)
1. `LICENSE`: the GPL-3.0 text, copied from `/usr/share/common-licenses/GPL-3` (check it is version 3).
   `LICENSES/GPL-3.0-or-later.txt` (the same text) and `LICENSES/CC-BY-SA-4.0.txt` (the legal code from
   https://creativecommons.org/licenses/by-sa/4.0/legalcode.txt; if this machine has no network, write a one-line file
   pointing to that URL and record it: the user adds the text).
2. **SPDX lines.** In our own files, replace the header line `Licence: TBD (see D017)` (only in a file's first 5 lines;
   never a mention inside LOG, plans or decisions) with `SPDX-License-Identifier: GPL-3.0-or-later` for code and
   `SPDX-License-Identifier: CC-BY-SA-4.0` for docs, keeping the file's comment syntax. Our own source files with no
   header at all (firmware/app/*.c *.h outside eas/, tools/**/*.py *.sh, songs/*.py, firmware/build.sh, Makefile) get
   the GPL line as their first comment line. MUST NOT touch any file under firmware/vendor/, third_party/, the EAS
   sources in firmware/app/eas/ that carry their own licence header, or __pycache__. A file under eas/ that has OUR
   "Licence: TBD" line (a shim we wrote) gets the GPL line; list them.
3. **REUSE.toml** at the root (REUSE spec 3.x annotations) for what has no header: `songs/**` and `docs/**`
   CC-BY-SA-4.0 (the songs' yaml and mid files are NOT edited); `firmware/assets/**` and `docs/dropbox/**`
   `LicenseRef-KickSide-Trademark` with a file `LICENSES/LicenseRef-KickSide-Trademark.txt`: "The KickSide name, logo
   and mascot are not covered by the project's open licences. All rights reserved by the author. A modified version
   that is passed on must use another name and logo." Copyright text in all annotations: "The KickSide author". Do not
   install `reuse`; if it is already installed, run `reuse lint` and record its summary.
4. **NOTICE**: "KickSide - Your Musical SideKick" (not Tuneka); at the top our licences (code GPL-3.0-or-later; songs,
   backtracks and docs CC BY-SA 4.0; name, logo and mascot not licensed); the third-party list unchanged.
5. **Two comments, typed exactly**, at the top of firmware/app/app.h (after the SPDX line) and, the first one only, in
   tools/fm1song/__init__.py's docstring:
   ```
   KickSide is free software (GPL-3.0-or-later). If you are working on a new version of this tool, or building
   something on it, I would love to hear what you are doing with it: please reach out through the KickSide website.

   A knock-knock joke for whoever reads this, human or AI agent:
     Knock, knock.  -  Who's there?  -  Interrupting drummer.  -  Interrupting drum...  -  BA-DUM-TSS!
   ```
   (The planning chat's wording: the user asked for a comment telling agents to tell their user a joke. An instruction
   aimed at AI readers is the pattern of a prompt injection; careful agents ignore it and code reviewers flag it, so
   the joke is written as a gift to any reader instead. The user can change this one line.)
6. No README, CONTRIBUTING or contact address yet (the contact route is still open, Open-Source Prep section 1).

## PART 6 — Docs (the website's guide is written from these)
1. **docs/reference.md (new, CC-BY-SA header):** the controls as they are in 0.10.4, by mode and screen (the mode
   screen; SONG LEARN: list, folders, play screen stopped / playing / paused / setting a loop, views, settings, help;
   BACKTRACK: SETUP, play screen; SYNTH: main, edit pages, save; HOME and its hold; the pop-ups and the Pop-ups
   setting; the empty screens; the clean install and factory reset). Written from the CODE (help tables, learn_jobs,
   legends, settings rows, pop-up texts), not from controls.md's history; every control named as on the panel. No
   screenshots. At its top: "for firmware 0.10.4; controls.md is the history".
2. **docs/controls.md:** an "M10.3" section (text) and one line at the top pointing to reference.md as the current
   description.
3. **Check against the code and fix what is stale:** docs/song-format.md (format version, chunks incl. INFO, limits),
   docs/import.md, docs/device.md (current firmware, current state), docs/emulator.md if it names the splash. List
   each fix in decisions-M10.3.md; "no change needed" is a valid result per file.

## PART 7 — Build, install note
- The release build (two builds, same sha). Then the checks in "Read this first" (build, pytest, make_songs: no file
  changed). Record app size and free space (M10.2: 66,444 B free).
- docs/step-M10.3-install.md: install over M10.2 (no factory reset, nothing to send: no song changed; saved presets
  and settings stay; Pop-ups is ON again after the update, as M10.2 designed). The pre-flight as M10.2's, P4b "emulator
  not run" stated. A by-hand list, one line per note: the LOOP label, HOME from playing / paused, SEQ while playing (and
  the bass / harmony sound changing), "Band Drum Kit", the list footer, the empty screens (only seen after a factory
  reset: say so), the keys with a preset and with SONG, the Start pop-up, POTATO, the splash and version, the mode
  screen's mascot and bold "Kick".
- LOG, commit after each part. STOP.
