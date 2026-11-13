# Milestone M8 — instructions: BACKTRACK mode, renames (after M7 is installed)

From the planning chat, 2026-11-12. Scope: vault decisions D046–D050 (the user's M7 notes, quoted in the vault's
Brainstorms/2026-11-04 Prototype feedback, 2026-11-12). The progression list is the vault note
Research/2026-11-12 Backtrack chord progressions.md, copied below (Part 3) so this plan stands alone.
M7.1 (saving user presets, D040) comes AFTER M8: no preset-saving code in M8.

State at the start: the user installed 0.7.0-m7 (FM-1_46000700) and ran docs/step-M7-install.md: all fine;
Rockin' deleted on the unit, Zombie resent (2.1); Zombie's quieter lead sounds right. Record that in LOG and
device.md first.

```text
Follow CLAUDE.md and docs/HANDOFF.md. Build Milestone M8 in the parts below, in PRIORITY order. Commit after each
part. Fast checks after each part; the full suite plus make m51-test (via the test-runner subagent) only ONCE, on
the final package in Part 7. No early install point. Emulator with the M5.1 flash timing. Mock screenshots in
docs/controls.md ("M8" section) for every new screen. If the user is away: decide, record in LOG briefly, carry on.
Never relax a check to pass. Never run a device command.

PART 0 — Measure first (no behaviour change)
- LOG: free app space, RAM .data+.bss, the song buffer's use in LEARN for the largest song, the CPU of LEARN's
  busiest song (EAS) alone, and of SYNTH's 4 voices of the heaviest preset with and without delay/reverb.
- Decide the BACKTRACK melody sound (D049) from these numbers: the synth presets at 4 voices beside EAS if the song
  buffer can be split (backtrack song + the synth's effect lines, shortening the delay's maximum time in BACKTRACK
  if needed) and the CPU of EAS band + 4 synth voices + effects stays <= 60 % of a block; otherwise 5 General MIDI
  sounds (PIANO, E-PIANO, ORGAN, GUITAR, SYNTH LEAD). Record the choice and the numbers.

PART 1 — Renames (D050)
- Mode screen: three choices in this order: SONG LEARN, BACKTRACK, SYNTH (cursor on SONG LEARN at every power-on,
  the mascot as now). Everywhere "LEARN" names the mode (screens, Settings > Switch mode, help, controls.md), it
  becomes "SONG LEARN". HOME held 2 s and Switch mode reach the same 3-choice screen from every mode.
- Splash: "Notami - SongSidekick" instead of "FM-1 Accompaniment", under it only the milestone, "m8" (FW_VERSION
  stays 0.8.0-m8 for --info, help and the package). Change APP_NAME in app.h (it also names fm1-base's crash / safe-mode screens and the screen reader: fine)
  and ui.c's other "FM-1 Accompaniment" text (help/about line). If it does not fit 240 px in FONT_B, use the next
  smaller font, never cut it. Do NOT change the USB / MIDI port names, the identity (FM-1_4600xxxx) or the package
  name: the laptop tools find the unit by them.
- Presets: E-PIANO, E-GUITAR, E-SITAR become presets 1, 2, 3 (the rest keep their order after them), in SYNTH and in
  BACKTRACK's melody sound list. SYNTH's start preset stays preset 1 (now E-PIANO). Update every test and table that
  names a preset by number.
- Songs: twelve-bar-blues title "12-Bar Blues"; four-chords title "Four Chord Tune" (slugs unchanged). Both need a
  resend after the install (checklist; the user runs it).

PART 2 — Format 2.2 (additive) and the generator styles (D047, D048)
- Format 2.2, additive like 2.1 (songs that use nothing new stay 2.0/2.1 byte-identical; golden tests unchanged):
  (a) a song KIND: song / backtrack (SONG LEARN lists only songs; BACKTRACK only backtracks; an M7 FM-1 refuses
  2.2, so backtracks are sent only after the install); (b) per section: a ROLE (intro, verse, chorus, bridge,
  ending, other) and an ENERGY offset 0..2 added to every generated layer's level in that section (capped at the
  layer's top level). Update docs/song-format.md, the compiler, the Python reader, the C parser and song_fuzz.sh.
- ENERGY on the FM-1: the levels a layer plays in a section with an offset are generated for those bars at the
  higher level, when the song opens (no rebuild, no gap at the boundary). If that cannot be done cleanly in gen.c
  and the player, fall back to: no offset, a drum fill (L3 fill) in the last bar before each chorus; record why.
- New generator styles, in gen.c AND tools/fm1song/arrange.py, with golden tests (each new style: a test chart, every
  level, C and Python identical): ROCK, INDIE POP, BOSSA NOVA, FUNK, REGGAE. Each: a harmony rhythm and voicing
  feel, a bass pattern, drums L1-L3 (take the grooves from M7's docs/drum-patterns.txt ROCK1, POP3, BOSSA, FUNK1,
  REGGAE where they fit). The six existing styles are unchanged (their golden tests must stay identical).
- The style list shown in BACKTRACK (11): POP, ROCK, INDIE POP, BALLAD, SHUFFLE, STRIDE (= the march style's
  stride harmony), WALTZ (3/4), COUNTRY (= train), BOSSA NOVA, FUNK, REGGAE. WALTZ sets the metre to 3/4 (every
  progression has one chord a bar, so all work). Learning songs may use the new styles in song.yaml too.

PART 3 — The 30 backtracks (D046, D047)
- tools/backtracks.py writes songs/backtracks/<slug>/song.yaml (+ song.mid only if the compiler needs one; no
  melody) from ONE table (tools/backtracks.tsv) holding the list below: number, short device name (<= 15 chars),
  the numerals, the verse progression, the optional chorus progression, the mode (major/minor/blues). Every song in
  C major or A minor, kind backtrack, style POP by default (the FM-1 overrides it), category "Backtracks".
- Each file holds the FULL form, in order, repeats stored once: INTRO (the verse's last 4 bars, energy 0, at the
  style's lowest levels), VERSE (x2), CHORUS (x2: the chorus progression if one is given, else the verse chords;
  energy +1 either way), VERSE, CHORUS, BRIDGE (by the rule below, energy 0), CHORUS (energy +1), ENDING (one bar,
  I or i, held). Blues forms (20-22): INTRO (last 4 bars), 3 rounds (energy 0, +1, +1), ENDING; no bridge.
- Bridge rule: major: IV V vi vi, or vi IV V V if the verse or chorus starts on IV; minor: bVI bVII i i, or
  iv v bVI bVII if the verse starts on bVI. The last bridge bar becomes V (major) / V (minor, harmonic) to lead back.
- Structures on the FM-1 (no extra data): LOOP = the VERSE section looped; VERSE/CHORUS = the first VERSE + CHORUS
  bars looped; FULL = the whole file, with an ending. Use the existing loop machinery.
- The list: 30 (the user's choice, 2026-11-12; verse | chorus, "-" = none; one chord a bar; b = flat; lower case
  = minor):
  1 POP 1 I V vi IV | vi IV I V        2 POP 2 vi IV I V | I V vi IV        3 50S I vi IV V | IV V I I
  4 POP 3 IV I V vi | -                5 POP 4 I IV vi V | vi IV I V        6 LA BAMBA I IV V IV | -
  7 FOLK I IV I V | IV I V I           8 ROYAL ROAD IV V iii vi | IV V I I 9 CANON I V vi iii IV I IV V | -
  10 I V I IV | IV V I I               11 MIXO I bVII IV I | -              12 I bVII IV IV | -
  13 MARIO I bVI bVII I | -            14 CREEP I III IV iv | -
  15 MINOR POP i bVI bIII bVII | bVI bVII i i   16 i bVII bVI bVII | -      17 ANDALUSIAN i bVII bVI V | -
  18 i iv bVII bIII | bVI bVII i i     19 DORIAN i IV | -
  20 12-BAR BLUES I I I I IV IV I I V IV I V (blues)   21 12-BAR QUICK I IV I I IV IV I I V IV I V (blues)
  22 MINOR BLUES i i i i iv iv i i bVI V i V (blues)   23 ii-V-I ii7 V7 Imaj7 Imaj7 | -
  24 TURNAROUND Imaj7 vi7 ii7 V7 | -   25 CIRCLE I IV iii vi ii V I I | -   26 FUNK I7 I7 IV7 IV7 | -
  27 TWO CHORD I IV | -                28 COUNTRY 8 I I IV I V V I I | -    29 BOSSA ii7 V7 Imaj7 VI7 | -
  30 PLAGAL IV iv I I | -
  (The names are drafts; keep them <= 15 chars; the setup screen shows the numerals on a second line.)
- Compile all 30, report the total size and the song store's room with the user's 13 songs + 30 backtracks.

PART 4 — BACKTRACK mode: setup and play (D046, D048)
- SETUP screen (instead of a song list): K1 progression (1-30, its name + numerals in the chosen key), K2 style
  (11), K3 structure (LOOP, VERSE/CHORUS, FULL), K4 key (the 12 keys; major or minor by the progression; shown as
  chords in that key), SELECT tempo (each style's default until turned), PLAY starts, GLO help, HOME held 2 s the
  mode screen. Defaults at entry: 1 POP 1, POP, LOOP, C, the style's tempo. The last choices are kept until power-off.
- PLAY screen = SONG LEARN's play screen and controls (same buttons, same legend): PLAY/pause, HOME stop then back
  to SETUP, the layer buttons (mute / quiet, hold = your part), EDIT views, SAVE = KEY mode, SELECT tempo, SEQ loops,
  ARP count-in, OCT-/OCT+, GLO help. K1-K3 = drums / bass / harmony level (shown as "busier / simpler"; energy
  offsets apply on top). K4 = the MELODY SOUND (D049; shown for a moment) because there is no melody part.
- Every view of drums, bass and harmony works as in SONG LEARN (chord boxes, tab, notation, rolls...).
- The keys play the melody sound (SONG LEARN keeps Electric Piano 1).
- The "not implemented" pop-up rules of D044 apply in both new screens.

PART 5 — The melody: "what can I play now" (D049)
- Melody views (EDIT, with melody as your part, or the default when nothing is your part): SCALE KEYBOARD (the
  FM-1's 27 keys drawn, the scale's notes marked, the current chord's tones bright, changing at each chord, the
  next chord's tones dim in the last beat) and SCALE FRETBOARD (guitar, or ukulele by the M6 setting: the same
  marking over the first 12 frets). The key and scale named on top.
- The FM-1's own key lights: chord tones bright, scale notes at the middle brightness (the M3 key-light rules).
- Scale: pentatonic by default: major pentatonic for major progressions, minor pentatonic for minor, blues scale for
  the blues forms. Settings row "Scale guide": PENTATONIC / FULL SCALE / CHORD TONES.

PART 6 — The melody sound (D049)
- As decided in Part 0: either the 30 SYNTH presets (PRESET name shown; 4 voices; delay/reverb as fitted) mixed
  over EAS, or 5 GM sounds on channel 16. K4 chooses; the choice is kept until power-off. No new preset data.

PART 7 — Tests, budget, install checklist. STOP for review.
- Full suite + make m51-test once on the final package (test-runner subagent). New: m8 checks: every style x 3
  structures x 3 sample progressions plays to its end in the emulator without xruns; the energy offset changes the
  chorus's level and nothing else; LOOP / VERSE-CHORUS loop at the right bars; key change re-spells chords; the scale
  views and key lights follow the chord; the census (D044) extended to SETUP and BACKTRACK's play screen; the mode
  screen's three choices and the rescues (OCT-+OCT+, REC, GLO+HOME) unchanged; 24 quick HOME taps never the mode
  screen; golden tests of the six old styles identical.
- Budget table as step-M7-install.md, plus BACKTRACK: the CPU worst case (busiest style, FULL, harmony L5, the
  melody sound's 4 voices with its effects): <= 60 % of a block; the song store with 13 songs + 30 backtracks; app
  space left >= 32 KB (M7.1).
- docs/step-M8-install.md: install over M7; then (the user) send songs/backtracks/*/ and resend twelve-bar-blues and
  four-chords; Known items; STOP.
```

Not in M8: saving user presets (M7.1, D040: next); a melody line for backtracks; recording; msfa/DX7; song info
page; ALBUM level.
