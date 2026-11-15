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

## M10.3-03 — Part 2: SONG LEARN
- List footer: drawn as row n of the list (the cursor clamps to n - 1, so it never lands there), two FONT_XS C_GRAY lines centred ("Download and install more" / "songs from our website": one line would not fit 228 px). The scroll rule: with the cursor on the last song `top` is raised so row n is on the screen. The folders screen: no footer.
- Empty screens (SONG LEARN flash OK and BACKTRACK): "Please visit our website" / "to download songs."; the old laptop command line (the tools/fm1song.sh line, C_HI) is removed from SONG LEARN's screen, "They stay here when it is off." stays (C_DIM).
- Keys: `ui.key_route[k]` (1 = vsynth) set at the press as `bt.on || lm_active()`; the release uses the recorded route. LM_VOICES stays 4 (CPU): the keys and the melody layer share the four voices. keys_program(song): 0xC0 on SYNTH_KEYS_CH with the open song's melody layer program (0xFF or no melody layer: GM_ELECTRIC_PIANO_1, the default synth.c sets at init).
  Called: open_play (SONG LEARN only), lm_set(LM_SONG) (K4 back to SONG), the HOME-from-stopped branch that goes back to the list, and mode_leave (synthui.c) when SONG LEARN is left with a song open.
- Start pop-up: `vpop.big_sub` (set by say_from after vpop_show when the section name is non-empty; cleared by vpop_keep, so by every other pop-up). Name at y = 32 + value font height + 6, FONT_B C_WHITE, FONT_XS if wider than 208 px. 32 + 24 + 6 + FONT_B height stays well inside the 104 px box.

## M10.3-04 — Part 3: POTATO
- Row before: `PR("POTATO", AN, 6, 60, 100, 30, 50, 30, 74, 0, 6, 8, 70, 0, 30, 40, 0, 0, ARP_UP, RATE_8, 0, 64, 64, 30, 30, 25, 20, 40, 30, 0, 3, 64, 20, 0, 64, 80, 30, 104, 0)`
  after: the same with the 8th number of the second line (VP_CRUSH, index 27) 0 -> 127; LEVEL (104) unchanged. A user preset saved from POTATO earlier is a copy and does not change.
- tools/m101_test.py's "only ... differ from M10's table" list now includes POTATO (25 rows byte for byte). Not run.

## M10.3-05 — Part 4: the artwork
- Originals moved (mv: docs/dropbox was untracked) to firmware/assets/src/kickside-logo.jpg (1536 x 1024) and kickside-mascot.jpg (1178 x 1335). firmware/assets/tmp-logo.jpg stays in the repo, no longer built.
- tools/logo_to_c.py: --palette (PIL quantize to a fixed palette, no dithering; unused slots repeat the first colour), --prefix, --preview (decodes the RLE it wrote: the firmware's own loop, as RGB565 -> RGB). Defaults unchanged (a run with the old arguments gives the old header).
- The planning chat's palette gave: logo 240 x 160, 7 colours, 5,643 B (the plan guessed ~7.2 KB); mascot 120 x 136, 2,264 B. Previews docs/step-M10.3/splash-logo.png and mode-mascot.png looked at: the cream background whole, outlines unbroken, "KickSide" and "Your Musical SideKick" readable. No palette change.
- ui.c: `struct art` {w, h, bg, pal, rle, len}; draw_logo_s(&art, oy) draws 1 : 1 (the num / den scaling is gone); ART_LOGO and ART_MASCOT built from the two headers. The version on the splash: FONT_XS, right edge x 234, its top at 240 - 6 - FONT_XS.h (the font's box is 15 px; "baseline about 6 px above the bottom edge" is met to within the descender), colour RGB(150, 136, 118). SPLASH_MS and the start-up melody unchanged.
- Mode screen: the title is drawn in three segments; "Kick" in FONT_B, the middle in FONT_S (checked: Barlow Medium 15 px, the regular weight of FONT_B = Barlow Bold 15 px, both 16 px boxes). Fallbacks as the plan says (FONT_XS for the regular part; then FONT_XS throughout: there is no smaller bold with lower case, FONT_T has capitals only). Colours as before, RGB(60, 50, 40).
- Test expectations updated, not run: m10_test.py (splash: the name line is gone, the check counts the version's ink in rows 216-235, "m10.3" milestone; "Band Drum Kit"; SEQ on BACKTRACK's play screen now steps the kit instead of the NOT IMPLEMENTED pop-up), m101_test.py (the splash check as m10_test's).
  Other tests that name "Tuneka"/splash only in comments or descriptions: left.

## M10.3-06 — Part 5: open source
- LICENSE = /usr/share/common-licenses/GPL-3 (checked: "Version 3, 29 June 2007"); LICENSES/GPL-3.0-or-later.txt the same text. LICENSES/CC-BY-SA-4.0.txt: this machine had network, so it is the real legal code from https://creativecommons.org/licenses/by-sa/4.0/legalcode.txt (20,138 B, plain text; not edited). LICENSES/LicenseRef-KickSide-Trademark.txt: the plan's sentence.
- SPDX lines (a script run once over `git ls-files`; not kept): `Licence: TBD (see D017)` -> `SPDX-License-Identifier: GPL-3.0-or-later` in 81 tracked files outside firmware/vendor, third_party, docs and NOTICE (no file in docs/ carried the header line; docs are covered by REUSE.toml). 23 had the words at the end of a comment line (tools/fm1song/*.py, several tools/*.py): the sentence stays, the SPDX tag goes on a line of its own under it.
  The search window was the first 12 lines, not 5: tools/band_styles.py (line 6) and songs/make_songs.py / songs/m10_songs.py (inside the docstring, line 10 / 17: left as SPDX in the docstring, REUSE reads it there) have it a few lines down.
  No header at all, line added (after a shebang): tools/*.sh (4), tools/store_blocks.py, tools/fm1song/__main__.py, tools/fm1song/tests/*.py (8), songs/m10_songs.py (had a docstring only), songs/amazing-grace/make_amazing_grace.py (a script, so GPL although it is under songs/).
  Generated files: firmware/app/band_styles.h and tools/fm1song/band_styles_data.py get the line from tools/band_styles.py (the generator's templates changed, both files regenerated; the data is byte-identical otherwise); firmware/app/jam_patterns.h likewise through tools/jam_patterns.py's template (it said "Licence: TBD" in its own template).
  Under firmware/app/eas/ our shims carried OUR "Licence: TBD" line, so they got the GPL line (7 files): cutils/log.h, eas_host_fw.c, eas_options.h, eas_version.h, eas_visibility.h, fm1inc/string.h, log/log.h. Nothing under firmware/vendor/, third_party/ or any __pycache__ was touched. docs/m3-design/notation_mock.py has no header and is left to REUSE.toml (docs/**, CC-BY-SA).
- REUSE.toml: the plan's two annotations (songs/** + docs/** CC-BY-SA-4.0; firmware/assets/** + docs/dropbox/** the trademark LicenseRef) plus: CLAUDE.md, NOTICE, backups/SHA256SUMS and tools/fm1song/tests/data/** (a song) as CC-BY-SA-4.0; .gitignore, .claude/**, tools/.gitkeep, tools/backtracks.tsv, tools/band_styles.txt, tools/patches/** as GPL-3.0-or-later. These were the tracked files with no header the plan's list did not name. The assets annotation has precedence "override" so no header can contradict it. `reuse` is not installed: no lint run.
- NOTICE: the title is "KickSide - Your Musical SideKick"; the top lists our three licences; the third-party list is unchanged.
- The comments are typed as the plan has them. app.h: a block comment after the SPDX line (both paragraphs). tools/fm1song/__init__.py has no docstring (it is a `#` header), so the first paragraph is a `#` block under the SPDX line.
- No README, CONTRIBUTING or contact address (the plan).
