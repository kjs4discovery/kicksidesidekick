# Milestone M10.1: instructions. Your M10 notes: screens, buttons, sounds, song fixes, ZUNJET, clean install

From the planning chat, 2026-11-14. Scope: the user's M10 notes (vault Brainstorms/2026-11-04 Prototype feedback,
"2026-11-14 — M10 build"), vault decisions D066 to D068.

State at the start: the user installed 0.10.0-m10 (FM-1_46001000) and used it; they saved a user preset named
**ZUNJET** in SYNTH. Record that in LOG and device.md first (install details: not given; write "installed and used,
no problems reported").

Version: **0.10.2** (0.10.1 is the early M10a package). Package `build/step-M10.1/fm1acc-0.10.2-m10.1.fwsc`; the
identity follows the existing scheme (FM-1_46001002 if 0.10.0 is FM-1_46001000). Record the identity you get.

## Read this first (the coder is Sonnet)

This plan is deliberately prescriptive. Follow it literally.
- **MUST / MUST NOT** are hard rules. Everything else is the intended design; if it turns out impossible or clearly
  worse in the code, choose the SMALLEST change that still passes the part's checks, then record it.
- **docs/decisions-M10.1.md (new; you write it)**, same format as docs/decisions-M10.md, numbered M10.1-01, ...
  Every judgment call this plan does not spell out goes there (a name, a position, a threshold, a text).
- **MUST NOT** change: the song store's flash code, the update loader, the rescue combos, the hardware safety
  rules in CLAUDE.md, the compiled files of any song not named in Part 5 (pytest's golden compares prove it), the
  user presets already saved on the unit (the preset object's format stays version 1).
- **MUST NOT** refactor code a part does not need. No renames of existing functions or files.
- **Testing is light on purpose (the user's choice, D068).** No full suite. After each part: build, plus the checks
  that part names. The fast lane in Part 8 runs once at the end. New checks go in tools/m101_test.py (copy the
  structure of tools/m10_test.py). Never relax an existing check to pass; if one fails, record it and say so.
- **Never run a device command.** `factory-reset` and `tools/fresh_install.sh` WRITE: the user runs them, never
  you. (No preset read-out in M10.1: the user withdrew it, D067.)
- Mock screenshots in docs/controls.md (new "M10.1" section) for every changed screen.

## PART 0 — Housekeeping
- LOG + device.md: the M10 install (above).
- Create docs/decisions-M10.1.md.

## PART 1 — Screens and text (quick)
1. **Name.** Every "Tuneka - Song Sidekick" in the firmware (mode screen title, splash if it has the name, and
   `firmware/app/words.tsv`, then regenerate what is built from it) becomes **"KickSide - Your Musical SideKick"**
   (exact spelling and capitals: K-i-c-k-S-i-d-e, S-i-d-e-K-i-c-k). It must fit on one line at 240 px: if it does
   not fit in the current font, use the next smaller font; never cut it. The words.tsv spoken form:
   "KickSide, your musical sidekick". Do not change emulator screenshots of older milestones.
2. **Mode screen order: unchanged** (SONG LEARN stays first; the user withdrew the BACKTRACK-first note).
   **SYNTH edit page titles:** each edit page's title also names the preset being edited: "FX 2 - POTATO",
   "ENVELOPE - U05 ZUNJET" (the name as the list shows it). If a title does not fit at 240 px beside the page number
   ("3/9"), use the next smaller font for the title; never cut the name. Record the font choice.
3. **Instruction row:** where the bottom row alternates pages (today every 3 s: ui.c `plat_ms() / 3000u` in three
   places, synthui.c `now / 3000u`, and any in bt.c), each page now stays **6 s**. Use one named constant
   (`HINT_PAGE_MS 6000u`) for all of them.
4. **BACKTRACK, LOOP structure:** when the structure is LOOP, the section indicator shows **LOOP** (never VERSE),
   and it never blinks for a coming change (there is none). VERSE/CHORUS and FULL unchanged.
5. **Value pop-up in SONG LEARN and SYNTH** (BACKTRACK has it since M10; same code, same look, same timing
   VPOP_MS, a title and the value). SONG LEARN: K1-K3 (title "Drums Level" / "Bass Level" / "Harmony Level", value
   "2 of 3"), K4 (Part 2: "Melody Sound"), SELECT tempo ("Tempo", "112 BPM"), PRESETS (start point, "Start", "BAR 9"
   or the section name), ALGO if it changes a value, and the layer buttons (Part 2: "Your Part" / "Bass Volume").
   SYNTH main screen: the PRESET knob ("Preset", the name), SELECT tempo, K1-K4 quick knobs (title = the parameter
   name as the page names it, e.g. "Cutoff"). SYNTH edit pages already show their four values: no pop-up there.
   Where SYNTH's bottom row today shows the turned value, the pop-up replaces it.
- Checks (m101p1): the title string on the mode screen, the mode order unchanged, an edit page title with the
  preset name (a long name too), a hint page still shown at 5.9 s and the
  next at 6.1 s, LOOP shown in LOOP structure, a pop-up after each listed control in both modes.

## PART 2 — Buttons
1. **SONG LEARN layer buttons (D066, replaces D027's hold):** a press on a layer that is NOT your part makes it
   your part (its volume does not change). A press on your part cycles its volume FULL -> QUIET -> MUTED -> FULL.
   Holding does nothing special any more (a hold counts as one press, acted on at the press). Each layer keeps its
   own volume when your part moves. Harmony is your part when a song opens (as today). Pop-up on every press:
   "Your Part" + the layer name, or the layer name + "Volume" + FULL/QUIET/MUTED.
   Remove "hold your part" from every instruction row and the controls hint; the new short text:
   **"FX-LFO: pick part | again: volume"** (fit it to the row; record the final text).
   BACKTRACK: if its layer buttons (LFO for the melody) use the same press / hold rule, give them the same new rule
   and record it (the planning chat's suggestion, for one rule in both modes); if they work differently, leave them.
2. **HOME (single press):** goes up one level, as today, and from a mode's top level (SONG LEARN's folder list,
   BACKTRACK's SETUP, SYNTH's main screen, and any other first screen of a mode) the next single press goes to the
   **mode screen**. So repeated single presses always end on the mode screen. Holding HOME 2 s still goes straight
   to the mode screen from anywhere (unchanged). In a yes/no question ("REPLACE ...? SAVE = yes HOME = no") HOME is
   still "no", and in SYNTH's SAVE screen HOME still goes back one step. Make sure a press that starts a 2 s hold
   does not first act as a single press (act on release if needed; record which).
3. **SONG LEARN K4 = Melody Sound.** Today K4 picks the melody's level, but every song has one melody level, so K4
   does nothing. New: K4 steps through **SONG** (the song's own instrument, the default when a song opens) and then
   the preset list (stock in the Part 4 order, then the user's saved ones), as BACKTRACK's K4 does. A preset plays
   the melody layer on vsynth (as BACKTRACK plays its melody); the other layers stay on EAS. Kept until another song
   is opened. Volume, mute and your part work on the melody as before. Pop-up "Melody Sound" + the name.
   - Check first that no song (public or private) has more than one melody level; if one does, K4 keeps the level
     job for that song only, record it.
   - CPU: measure EAS + vsynth with the melody on the heaviest stock preset, at the busiest levels of the busiest
     song: MUST stay under 60 % of a block. RAM: vsynth's memory MUST fit beside SONG LEARN's level buffers. If
     either fails: instead of vsynth, K4 steps through a short list of General MIDI instruments for the melody
     (SONG, flute, clarinet, trumpet, alto sax, violin, e-piano, organ, accordion), and record it.
   - Keep the choice as a reference to the sound (stock preset id or user slot), not a list position, so a new
     save or the Part 4 order cannot shift it. Do the same for BACKTRACK's K4 (this closes M10-14).
- Checks (m101p2): first press selects, second cycles, a hold of 1.5 s on a non-selected layer = one select; volumes
  kept when the part moves; HOME presses from deep in each mode end on the mode screen; hold 2 s straight there;
  REPLACE + HOME = no write; K4 cycles SONG and presets, the melody's note-ons unchanged (same notes, other sound);
  the CPU numbers in LOG.

## PART 3 — A factory reset
1. **FACTORY RESET (writes; the user runs it)**: a SysEx command (use 0x78 if unused) with a confirmation word, like
   WIPE: erases the song store (as WIPE), erases the preset object (both sectors), and puts the app's settings back
   to their defaults. Refused in SYNTH and BACKTRACK as sends are (result 9). Afterwards the screen says
   "FACTORY RESET DONE - SWITCH OFF AND ON". Laptop: `tools/fm1song.sh factory-reset --yes`.
   If an existing rescue combo or command already resets the settings, reuse that code and record it.
   MUST: a power cut at any flash operation of the reset leaves a unit that boots (each part already starts on junk
   or an empty area; prove it, see checks).
- Checks: m101p3 in the emulator: FACTORY RESET: afterwards no songs, no presets, default settings, a boot is fine; and in the timed emulator
  (tools/m10_flash_test.py style) a power cut at each flash operation of the reset, then a boot: always boots, no
  crash, the store / presets / settings each either old or empty.

## PART 4 — Preset sounds
1. **Order** of the stock presets (SYNTH's list, BACKTRACK's K4, SONG LEARN's K4):
   E-PIANO, E-GUITAR, E-SITAR, **PARKER, POTATO, EDS, STAB 84**, then the others in today's order (NEON PAD, DUSK
   STRINGS, ... VHS STRINGS). Find every place that names a stock preset by its position (SYNTH's start preset,
   BACKTRACK's default melody sound, kits, tests' expectations) and make each still mean the same sound.
2. **E-GUITAR:** attack **12**, fuzz **35** as the screen shows them (today attack 0, fuzz 90 in vsynth.c's table;
   check the screen shows these two parameters as raw values; if not, convert so the screen reads 12 and 35). Every
   other value unchanged, LEVEL too (the user's words). Run `vsynth_test` with LEVELS=1: if E-GUITAR's loudness is now
   off, record the number in LOG for the user; do NOT change its LEVEL.
3. **EDS: a new sound, a fuzz bass**: like a bass guitar with a slight warble through a fuzz pedal. ANALOG engine,
   ARP off, the name stays EDS. A starting point (tune by ear in the emulator's render and by the loudness check):
   SAW plus a strong SUB, a little detune for the warble (or a slow, shallow LFO on pitch if the engine has one), low
   cutoff (about 35-45), moderate resonance, short attack, medium decay, sustain about 90, short release, FUZZ high
   (about 90-110), no chorus, no delay, little or no reverb. It must sound low: if the presets cannot play an octave
   down, lean on the SUB oscillator; do not add a parameter. Match its LEVEL to the others (vsynth_test LEVELS=1
   passes). Record the final values in decisions.
4. **ZUNJET replaces HAZE PAD** (D067), in HAZE PAD's place in the list (after DRIVE BASS, before LASER ARP). The
   user read it off the unit; the planning chat converted the screen values to the table's raw values (FILTER's ENV
   AMOUNT +14 = 78, TYPE LP = 0, WAVE SYNC = 5, ARP UP / 1/8, OCTAVES shown 1 = 0, DELAY TIME 1/8D = 3; ARP button
   off). Type this row exactly in place of HAZE PAD's:
   ```
   PR("ZUNJET", AN, 4, 70, 105, 40, 92, 26, 78, 0, 5, 80, 72, 52, 24, 64, 36, 0, ARP_UP, RATE_8, 0, 64,
      64, 40, 0, 0, 0, 0, 0, 0, 3, 64, 16, 0, 64, 64, 30, 97, 0),
   ```
   Every value is the user's, LEVEL (97) too: do NOT re-match it; if vsynth_test LEVELS=1 flags ZUNJET, record its
   loudness in LOG for the user. If anything referred to HAZE PAD by name or position, it now means ZUNJET; record it.
   The user's own copy in a U slot is erased by the clean install (that is fine: ZUNJET is stock now).
- Checks: vsynth_test (all), m101p4: the list order on screen, BACKTRACK's and SYNTH's defaults are the same sounds
  as in M10, E-GUITAR's two values on its pages, EDS renders (peak, no clipping).

## PART 5 — Song fixes (the planning chat's exact notes; type them literally)
Sources (published transcriptions of the public-domain tunes): Camptown, Kevin Goess's ABC at stephenfoster.info;
Oh! Susanna, Frank Nordberg's ABC (musicaviva); Scarborough Fair, Soodlum's Tin Whistle Book and the Dupont Music
Circle ABC (abcnotation.com), transposed to D. Michael Row and Creep: the user's corrections. Bar numbers are the FM-1's (a pickup bar is bar 0). Durations in beats. Edit only the lines named; then
`.venv/bin/python songs/make_songs.py <slug>` for each song below, `tools/fm1song.sh validate`, and pytest's golden
files updated **only** for these five public songs (every other song MUST stay byte-identical).
1. **camptown-races** (songs/m10_songs.py, written in C, played in D):
   - Melody, chorus bar 3 (FM-1 bars 11 and 23; "I'll bet my money on the bob-tail nag"): CAMP_C's third row becomes
     `[("G4", .5), ("G4", .5), ("E4", .25), ("E4", .25), ("G4", .25), ("G4", .25), ("A4", .5), ("G4", .5), ("E4", 1)]`.
   - Harmony, verse bars 2 and 6 (FM-1 bars 2, 6, 14, 18; the "doo-dah" bars): A7. The verse chart becomes
     `"| D | A7 | D | A7 D |\n| D | A7 | D | A7 D |"`.
2. **michael-row** (songs/m10_songs.py): the A4 on beat 3 of verse bar 3 (FM-1 bars 3, 11, 19) moves to beat 1 of
   the next bar. In `_ROW`: the third bar `[("B4", 2), ("A4", 2)]` becomes `[("B4", 4)]` and the fourth
   `[(None, 2), ("F#4", 1), ("A4", 1)]` becomes `[("A4", 2), ("F#4", 1), ("A4", 1)]`. The chart's third bar
   "G D" becomes "G" (the D half was there for that A): `"| D | D | G | D |\n| D | A7 | D A7 | D |"`.
3. **oh-susanna** (songs/make_songs.py): tempo **125**. Melody, chorus bars 2 and 6 (FM-1 bars 10, 14, 26, 30;
   "oh don't you cry for me"): in SUSANNA_CHORUS both rows
   `[("A4", 1), ("G4", .5), ("G4", .5), ("E4", 1), ("C4", .5), ("D4", .5)]` become
   `[("G4", .5), ("G4", .5), ("E4", .5), ("C4", .5), ("D4", 1.5), ("C4", .25), ("D4", .25)]` (in C; the song is
   transposed to D as before).
4. **saints** (songs/make_songs.py): tempo **135**. Nothing else.
5. **scarborough-fair** (songs/make_songs.py): the verse is rewritten with the traditional rhythm and becomes **18
   bars** (4 + 4 + 6 + 4). SCARBOROUGH_VERSE becomes (3/4, D dorian, as written):
   ```
   [("D4", 2), ("D4", 1)], [("A4", 2), ("A4", 1)], [("E4", 1.5), ("F4", .5), ("E4", 1)], [("D4", 3)],
   [(None, 1), ("A4", 1), ("C5", 1)], [("D5", 2), ("C5", 1)], [("A4", 1), ("B4", 1), ("G4", 1)], [("A4", 3)],
   [(None, 2), ("D5", 1)], [("D5", 2), ("D5", 1)], [("C5", 2), ("A4", 1)], [("A4", 1), ("G4", 1), ("F4", 1)],
   [("E4", 1), ("C4", 2)], [("C4", 2), (None, 1)],
   [("D4", 2), ("A4", 1)], [("G4", 2), ("F4", 1)], [("E4", 1), ("D4", 1), ("C4", 1)], [("D4", 3)]
   ```
   (write it with `bars(3, ...)` so each bar's length is checked; `bars()` is defined below SCARBOROUGH_VERSE
   today, so move the definition up, no other change). Form `[("Verse 1", 18, "verse"), ("Verse 2", 18,
   "verse")]`. Chart:
   `"| Dm | Dm | C | Dm | F | Dm | G | Dm |\n| Dm | Dm | F | F | C | C |\n| Dm | C | C | Dm |"`.
   If the chart parser needs rows of equal length, split differently and record it; the chords per bar stay.
6. **creep** (songs/private/creep/song.yaml, the user's private import, git-ignored: edit in place, do not commit):
   the chart bars 8, 16 and 40 ("Dm7 Cm") become "Cm" for the whole bar. Bars 24 and 48 ("Csus4 Cm7") MUST stay as
   they are. Then compile it and validate. Nothing else in the file.
- Checks: validate all six; pytest (the five public goldens updated, nothing else changed); m101p5 opens each of the
  five public songs, plays bar 1 to the end at level 1, no halt.

## PART 6 — Clean install like a new user (D068)
The update package cannot carry the songs (the loader writes only the app area, and the library is about 150 KB
against 70 KB free), so the library goes on in the same run, from one list.
1. **songs/library.txt** (new, committed): the public library a new user gets, one path per line: every public
   song folder in songs/ (not songs/private/, not the dropped songs) and every backtrack file that step-M10-install.md
   sends today. Header comment: "the songs and backtracks a new FM-1 gets; the website will use this list too".
2. **tools/fresh_install.sh** (new; the USER runs it; it prints each step and asks y/n before each write):
   (1) `fm1_install.py --info`; (2) install the package given as argument (the same command as the install docs);
   (3) wait for the FM-1 to come back with our identity; (4) a warning and y/n: "this erases ALL songs, saved
   presets (U01-U30) and settings"; (5) `fm1song factory-reset --yes`; (6) "switch the FM-1
   off and on, choose SONG LEARN, press Enter"; (7) send every entry of songs/library.txt (songs, then backtracks);
   (8) `fm1song list`, and compare the count with library.txt. Exit at the first failure with the fallback line.
   Test it against the emulator only (`--emu-pipe` as m2_test does), never the unit.
- Check: one emulator run of the whole script (minus the install step): reset done, every library
  entry sent and listed `ok`, no private song.

## PART 7 — Docs
- docs/controls.md "M10.1" (screenshots), docs/song-format.md only if the format changed (it should not),
  NOTICE unchanged unless a new third-party file came in. The website guide's wording lives in the vault (the
  planning chat updates it).

## PART 8 — The fast lane (once, on the final package, via the test-runner subagent)
pytest tools/fm1song; store test; upreset test; vsynth_test LEVELS=1; m10 (its own suite, as a regression for the
M10 screens); m10_flash; m101 (all parts); and m51 (the M5.1 send suite, about 22 min) ONLY if the user's prompt asks for it
(Part 3 touches library.c and the clean install sends every file again). No m5 to m9, no fuzz. Record times and results in LOG.
Then write **docs/step-M10.1-install.md**: install over M10 (fallback M10's package; a preset object stays readable);
then the user runs `tools/fresh_install.sh build/step-M10.1/fm1acc-0.10.2-m10.1.fwsc` (or, without the clean install,
re-sends the five changed songs); then a short by-hand list, one line per note of the user's: name,
edit page titles, hint timing, LOOP, pop-ups, layer buttons, HOME, K4 melody sound, preset order, ZUNJET in HAZE PAD's place, E-GUITAR, EDS, the five songs.
STOP. The user installs; Claude Code runs only --info / fm1song list and records.
