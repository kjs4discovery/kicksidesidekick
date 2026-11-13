# Milestone M9 — instructions: BACKTRACK redesigned (band complexity, live knobs), count-in and folder fixes

From the planning chat, 2026-11-13. Scope: vault decisions D051–D055 (the user's M8 notes, quoted in the vault's
Brainstorms/2026-11-04 Prototype feedback, 2026-11-13). Saving user presets (D040) and swappable sound kits are
**M9.1, not M9**: no preset-saving code, no flash-write code, no kit code in M9.

State at the start: the user installed 0.8.0-m8 (FM-1_46000800) and ran docs/step-M8-install.md: all fine, the 30
backtracks sent, 12-Bar Blues and Four Chord Tune resent. Record that in LOG and device.md first.

## Read this first (the coder for M9 is Sonnet)

This plan is deliberately prescriptive. Follow it literally.
- **MUST / MUST NOT** are hard rules. Everything else is the intended design; if it turns out impossible or clearly
  worse in the code, choose the SMALLEST change that still passes the part's checks, then record it (below).
- **docs/decisions-M9.md (new; you write it).** Every judgment call you make that this plan does not spell out goes
  there, one entry each, numbered M9-01, M9-02, ...:
  ```
  ## M9-NN — <short title> (Part X)
  - Situation: what the plan did not cover, or why it could not be followed as written
  - Options: A ..., B ... (at least two)
  - Chosen: A, because ...
  - Undo: how to reverse it (files, functions)
  ```
  Small choices count (a name, a colour, a threshold). The planning chat reviews this file after the run. LOG keeps
  its usual entries and points to the decision numbers.
- **MUST NOT** change: anything SONG LEARN generates for a song of kind "song" (the 13 songs' golden tests MUST stay
  byte-identical); the song store, the flash code, the settings store; the rescue combos; the hardware safety rules.
- **MUST NOT** refactor code a part does not need. No renames of existing functions or files unless a part says so.
- **Bugs: test first.** For Parts 1 and 2, write the failing emulator check, run it, see it FAIL and record the
  failure in LOG, then fix, then see it PASS.
- If a part's checks fail and you cannot fix them in a reasonable effort: stop that part, record exactly what fails
  in LOG ("Status: blocked"), and go on to the next part only if it does not depend on it.

```text
Follow CLAUDE.md and docs/HANDOFF.md. Build Milestone M9 in the parts below, in PRIORITY order. Commit after each
part. Fast checks after each part; the full suite plus make m51-test (via the test-runner subagent) only ONCE, on
the final package in Part 7. No early install point. Emulator with the M5.1 flash timing. Mock screenshots in
docs/controls.md ("M9" section) for every new or changed screen. If the user is away: decide, record in
docs/decisions-M9.md and LOG briefly, carry on. Never relax a check to pass. Never run a device command.
New checks go in tools/m9_test.py (copy the structure of tools/m8_test.py).

PART 0 — Measure first (no behaviour change)
- LOG: free app space, RAM .data+.bss, BACKTRACK's song buffer use for the largest backtrack in its busiest style.
- Measure, in the emulator, for the largest backtrack: how long (ms, and in audio blocks) lib_open + lib_restyle +
  building the drums, bass and harmony levels takes, and whether the audio has any xrun or gap while it runs (play a
  backtrack, then call the same functions again from the main loop as a style change would).
- Measure the same for rebuilding only the three layers' levels (what a complexity change will do, Part 4).
- Record whether TWO copies of the largest built backtrack fit in BACKTRACK's song room (112,896 B; the largest
  need in M8 was 54,032 B).
- From these numbers choose the switch method for Part 4 and record it in decisions-M9.md (M9-01):
  (A) SEAMLESS: build the new version into the second half of the song room while the old one plays, swap at the
      change point. Allowed only if two copies fit AND the build never causes an xrun.
  (B) ONE-BAR BREAK: at the change point the band stops, the new version is built, and it starts on the next bar
      line with no count-in; the screen shows "CHANGING..." meanwhile. At most one bar of silence at 60 BPM or
      faster; if it takes longer than one bar, it starts on the bar line after that.
  Prefer (A). Choose (B) if (A) fails either condition.

PART 1 — Count-in bugs (both modes)
User's report: in BACKTRACK (POP 3, i.e. progression 4) turning a knob while playing, most often K4, restarted the
count-in in the middle of the song, counting from "20-something"; and the count-in also started at random times,
apparently when the loop started over.
Likely cause (planning chat, from reading the code; verify): player.c plays count-in clicks for any position below
music_from (around line 369) and, after a loop jump, sets state = PL_COUNT_IN whenever the new position is below
music_from (around line 643). music_from is set by every player_start(), including resume-after-pause (ui.c around
line 1016) and a start point later in the song. So when the loop's start is BEFORE music_from, every loop jump
re-enters the count-in. The K4 case may be a second path (e.g. a player_start or seek from bt.c / ui.c while
playing): find it.
- First, the failing checks (m9p1), in the emulator, in BOTH modes:
  (a) BACKTRACK, progression 4, LOOP and FULL: PLAY, wait 2 bars, turn K4 by +1 and by -1 several times while
      playing, and turn K1-K3 too: the player state MUST never be PL_COUNT_IN again after the music started, and no
      count-in click may sound (check the click counter / player_stats).
  (b) BACKTRACK LOOP: PLAY, pause in the 3rd bar of the loop, PLAY again (count-in on), let the loop wrap 3 times:
      exactly one count-in (the resume's), none at the wraps.
  (c) SONG LEARN: a song with a SECTION loop and a BARS A-B loop, start point later than the loop's start, and the
      same pause/resume: no count-in at any wrap.
  (d) Count-in OFF: none of the above ever clicks.
- The rule to implement (MUST): a count-in happens only right after PLAY from stopped, or PLAY to resume from
  pause, with the count-in on. Once the music has started, the player never returns to PL_COUNT_IN and never plays a
  count-in click until the next player_start. Suggested: a flag counting, set in start() when a count-in is
  scheduled, cleared when the position reaches music_from; both the click test and the loop-jump state test require
  it. Keep the existing behaviour otherwise.
- Fast checks: m9p1 ALL PASS; m8p4, m8p6, m5p3, m6p1 (only the known Zombie failure).

PART 2 — Small fixes and text
- "All songs" count (SONG LEARN folder list): it counts every stored entry, including the 30 backtracks (13 songs +
  30 = 43). In ui.c draw_folders (around line 1665) the count loop MUST skip lib_is_backtrack(k), as folders_build
  already does. Also check the two other places that use lib_count() as "are there songs?" in SONG LEARN (input_list
  HOME, around line 985; around line 1268): with only backtracks in the store, SONG LEARN must behave as with an
  empty store. Failing check first (m9p2): with the 13 songs + backtracks, "All songs" says 13 and every category's
  count equals the number of songs listed when it is opened.
- Mode screen (synthui.c draw_mode): a title line "modes:" (exactly this text, FONT_XS, left-aligned at x 10, in
  the subtitle colour) just above the first choice; move the three rows down if needed, the mascot unchanged. The
  SONG LEARN subtitle becomes "Play with a song" (exactly; the other two unchanged). Update docs/controls.md and
  any test that reads these strings.
- BACKTRACK play screen: the LFO button's light is ON steadily (the melody has no layer, but holding LFO makes the
  melody your part). When the melody is your part, it shows as SONG LEARN shows "your part" on a layer button.
- Importer (tools/fm1song/importer.py, _meta, around lines 121-135): it reads e.data, but tools/fm1song/smf.py
  stores a time signature as a = numerator, b = denominator (already 2**power), and a key signature as a = sharps
  /flats (signed), b = major/minor. So 3/4 files import as 4/4 and the key signature is ignored. Fix both to read a
  and b. Failing test first: a synthetic 3/4 MIDI file and one with a key signature of 2 flats minor (pytest).
  Then re-run the importer on every file in import/ (public and private) and compare with the songs in the repo:
  record in LOG which songs change (if any). A changed private song is NOT resent by you; list it for the user.
  (The website's JavaScript port already reads these correctly: no change there.)
- Fast checks: m9p2 ALL PASS, pytest, m8census.

PART 3 — Band complexity for backtracks: the tier tables (D052)
Why: in M8 a backtrack's levels are SONG LEARN's learning ladder (harmony L1 held, L4 broken chords, L5 arpeggio for
every style), so a REGGAE backtrack at L1 does not sound like reggae. In BACKTRACK the style must define the rhythm
of drums, bass and harmony together at every complexity.
- New data file tools/band_styles.txt: the tier tables below, copied EXACTLY (the user will edit it by ear later).
- New tools/band_styles.py reads it and generates BOTH firmware/app/band_styles.h (C tables) and
  tools/fm1song/band_styles_data.py (the same data for arrange.py), so C and Python cannot drift.
  band_styles.py MUST reject (with the line number) a bar of the wrong length (16 or 12 characters as the style's
  metre and steps say), an unknown character, a missing tier, or a style not in M8's list of 11. Generated files
  carry a "generated by tools/band_styles.py, do not edit" line. A pytest checks the generated files are up to date.
- Generator: for a song of kind BACKTRACK only, gen.c (harmony, bass, drums) and arrange.py use the tier tables
  instead of the existing per-level code. Each of the three layers then has exactly 4 levels = tiers 1..4. Songs of
  kind SONG keep the existing code path untouched (MUST: golden tests byte-identical).
- Energy (the chorus lift, D047) MUST apply only to kind backtrack: a chorus plays one tier above the verse, capped
  at tier 4 (the existing level_at() rule, with top = 4). The compiler MUST refuse an energy offset in a song of kind
  song ("energy is for backtracks only"); no current song has one.
- Complexity 1..3 (chosen on the FM-1, Part 4) = the tier of the verse, intro and bridge; chorus = complexity + 1.
  So tier 4 is heard only in choruses at complexity 3. INTRO, VERSE, BRIDGE, ENDING keep their M8 energy (0).
- Final bar (ENDING) at every tier: the chord held (CLOSE voicing at tiers 1-2, WIDE at 3-4), the bass root held, a
  crash + kick on beat 1. As M8 does.
- Crash: at tiers 2-4, a crash (49, vel 92) with the kick on beat 1 of the first bar of every section except INTRO.
- Fill: at tiers 3-4, in the last bar of every section that is followed by another section (not ENDING): the
  bar's last beat is replaced by a snare fill: every step of that beat, the first 'o', the rest 'x'; kick and hats
  silent in that beat. Tier 4 adds tom lo (45, 'x') on the beat's first step.
- Golden tests: every backtrack (all 36 after Part 5) in each of the 11 styles at tiers 1-4: C = Python. Replace
  M8's backtrack golden files (they change by design; record this in LOG). The 13 songs' golden files unchanged.
- CPU: tier 4 is the busiest; check in Part 7.

THE TIER TABLE FORMAT (tools/band_styles.txt)
- A style block starts with:  STYLE <name>  steps <per beat>  [swing]
  steps 4 = sixteenths (16 characters a 4/4 bar, 12 a 3/4 bar); steps 3 = triplet eighths (12 characters a 4/4 bar).
  WALTZ is 3/4 (12 characters, steps 4). Everything else is 4/4.
- Then TIER 1..4, each with one line per layer: HARM (harmony), BASS (bass), and drum rows K S H O R C Y T (kick
  36, snare 38, closed hat 42, open hat 46, rim 37, clap 39, ride 51, tom lo 45). A drum row not listed is silent.
  (Note: "H" alone is the closed hat; the harmony line always starts with "HARM".)
- A line holds one bar, or two bars separated by " | " (played alternately: odd bars the first, even the second,
  counting bars from the start of each section).
- Harmony line: first the voicing word, CLOSE (the level-1 voicing, v1), WIDE (v2) or UPPER (v2 without its lowest
  note), then the steps: x = the chord, accent (vel 72); o = the chord (58); g = the chord, ghost (44); l / m / h =
  only the lowest / middle / highest note of the WIDE voicing (58); '-' = the previous hit goes on sounding;
  '.' = silence. A hit lasts its own step plus the '-' after it; a note is cut at a chord change.
- Bass line: R root, F fifth, O octave above the root, T third (major or minor as the chord), N the next chord's
  root, A a semitone below the next chord's root; upper case = vel 88, lower case = vel 68; '-' and '.' as harmony.
  The bass register as M5 decided (real octave, the speaker setting).
- Drum rows: x (92), o (70), g (34), '.' silence. Drum notes are 1 step long.
- Bars of another length (a 2/4 or 6/4 bar never occurs in a backtrack): repeat the pattern from its start.
- A chord changing mid-bar (the doubled short progressions): every step uses the chord sounding at that step.

THE TIER TABLES (copy into tools/band_styles.txt exactly; drafts by the planning chat, the user prunes by ear)

STYLE POP  steps 4
TIER 1
HARM CLOSE x-------o-------
BASS R-------F-------
K x.......x.......
S ....x.......x...
H x...x...x...x...
TIER 2
HARM CLOSE x-o-o-o-x-o-o-o-
BASS R---R-r-R---F-A-
K x.......x.x.....
S ....x.......x...
H o.o.o.o.o.o.o.o.
TIER 3
HARM WIDE x--o--o-x--o--o-
BASS R--r--R-R--r--A-
K x.....o.x.x.....
S ....x.......x...
H o.o.o.o.o.o.o.o.
O ..............o.
TIER 4
HARM WIDE x--ox--ox--ox-o-
BASS R-r-R-r-R-r-F-A-
K x...x...x...x...
S ....x.......x...
C ....x.......x...
H o.o.o.o.o.o.o.o.
O ..o...o...o...o.

STYLE ROCK  steps 4
TIER 1
HARM CLOSE x-------x-------
BASS R-------R-------
K x.......x.......
S ....x.......x...
H x.x.x.x.x.x.x.x.
TIER 2
HARM CLOSE x-o-o-o-x-o-o-o-
BASS R-r-R-r-R-r-R-r-
K x.......x.o.....
S ....x.......x...
H x.o.x.o.x.o.x.o.
TIER 3
HARM WIDE x-o-o-o-x-o-o-x-
BASS R-r-R-r-R-r-F-A-
K x.....o.x.o.....
S ....x.......x...
H x.o.x.o.x.o.x.o.
TIER 4
HARM WIDE x-x-x-x-x-x-x-x-
BASS R-R-R-R-R-R-F-A-
K x.o...o.x.o.....
S ....x.......x..g
Y x.o.x.o.x.o.x.o.

STYLE INDIE POP  steps 4
TIER 1
HARM CLOSE x-----o-----o---
BASS R-----R-----F---
K x.....x.........
S ....x.......x...
H o...o...o...o...
TIER 2
HARM CLOSE x-----o-----o---
BASS R-r-R-r-R-r-R-r-
K x.....x...x.....
S ....x.......x...
H o.o.o.o.o.o.o.o.
TIER 3
HARM WIDE x--o--x--o--x-o-
BASS R-r-R-r-R-r-F-A-
K x.....x...x.....
S ....x.......x...
H xoxoxoxoxoxoxoxo
TIER 4
HARM WIDE x-x-x-x-x-x-x-x-
BASS R-R-R-R-R-R-F-A-
K x...x...x...x...
S ....x.......x...
C ....x.......x...
H o.o.o.o.o.o.o.o.
O ..o...o...o...o.

STYLE BALLAD  steps 4
TIER 1
HARM CLOSE x---------------
BASS R---------------
K x.......x.......
R ....x.......x...
TIER 2
HARM CLOSE x-------o-------
BASS R-------F-------
K x.......x.......
R ....x.......x...
H o...o...o...o...
TIER 3
HARM WIDE l---m---h---m---
BASS R-------F---A---
K x.......x.o.....
S ....x.......x...
H o.o.o.o.o.o.o.o.
TIER 4
HARM WIDE l-m-h-m-l-m-h-m-
BASS R-----r-F-----A-
K x.......x.o.....
S ....x.......x...
H o.o.o.o.o.o.o.o.
Y ..o...o...o...o.

STYLE SHUFFLE  steps 3  swing
TIER 1
HARM CLOSE x-----x-----
BASS R-----F-----
K x.....x.....
S ...x.....x..
H x.ox.ox.ox.o
TIER 2
HARM CLOSE ...x.....x..
BASS R..F..O..F..
K x.....x.....
S ...x.....x..
H x.ox.ox.ox.o
TIER 3
HARM CLOSE x.ox.ox.ox.o
BASS R.rF.fO.oF.f
K x.....x.....
S ...x.....x.g
H x.ox.ox.ox.o
TIER 4
HARM WIDE x.ox.ox.ox.o
BASS R.rT.tF.fO.o
K x.....x.....
S ...x.....x..
C ...x.....x..
Y x.ox.ox.ox.o

STYLE STRIDE  steps 4
TIER 1
HARM CLOSE ....x.......x...
BASS R-------F-------
K x.......x.......
H ....x.......x...
TIER 2
HARM WIDE ....x.......x...
BASS R-------F-----A-
K x.......x.......
S ....g.......g...
Y x...x.x.x...x.x.
TIER 3
HARM WIDE ....x.o.....x.o.
BASS R---T---F---A---
K x.......x.......
S ....x.......x...
Y x...x..ox...x..o
TIER 4
HARM WIDE x...x.o.x...x.o.
BASS R---T---F---A---
K x...x...x...x...
S ....x.......x...
Y x..ox..ox..ox..o

STYLE WALTZ  steps 4
TIER 1
HARM CLOSE ....x---x---
BASS R---........
K x...........
H ....o...o...
TIER 2
HARM CLOSE ....x...x...
BASS R........... | F...........
K x...........
S ....o...o...
H x...o...o...
TIER 3
HARM WIDE ....x.o.x.o.
BASS R.......F... | F.......A...
K x.......x...
S ....x...x...
H x.o.x.o.x.o.
TIER 4
HARM WIDE l-m-h-m-h-m-
BASS R...F...O... | F...T...A...
K x.......x...
S ....x...x...
Y x.o.x.o.x.o.

STYLE COUNTRY  steps 4
TIER 1
HARM CLOSE ....x.......x...
BASS R-------F-------
K x.......x.......
S ....x.......x...
TIER 2
HARM CLOSE ....x.o.....x.o.
BASS R-------F-------
K x.......x.......
S ggggxgggggggxggg
TIER 3
HARM WIDE x...x.o.x...x.o.
BASS R---F---R---F---
K x.......x.......
S gogoxogogogoxogo
TIER 4
HARM WIDE x.o.x.o.x.o.x.o.
BASS R---F---R---F-A-
K x...x...x...x...
S gogoxogogogoxogo
O ..o...o...o...o.

STYLE BOSSA NOVA  steps 4
TIER 1
HARM CLOSE x-----o---------
BASS R-------F-------
K x.......x.......
R x..x..x...x..x.. | ..x..x..x..x....
TIER 2
HARM CLOSE x--o--o---o--o-- | --o--o--o--o----
BASS R-----F-F-----N-
K x..ox..ox..ox..o
R x..x..x...x..x.. | ..x..x..x..x....
H o.o.o.o.o.o.o.o.
TIER 3
HARM WIDE x--o--o---o--o-- | --o--o--o--o----
BASS R-----F-F-----N-
K x..ox..ox..ox..o
R x..x..x...x..x.. | ..x..x..x..x....
H ogogogogogogogog
TIER 4
HARM WIDE x--o--o-x-o--o-- | --o--o--o--o--o-
BASS R-----F-F---A-N-
K x..ox..ox..ox..o
R x..x..x...x..x.. | ..x..x..x..x....
Y o.o.o.o.o.o.o.o.

STYLE FUNK  steps 4
TIER 1
HARM UPPER ....x..o....x...
BASS R-....r...R.....
K x.........x.....
S ....x.......x...
H o.o.o.o.o.o.o.o.
TIER 2
HARM UPPER ....x..o....x..o
BASS R..r..R...O.R..A
K x.x.......x.....
S ....x.......x...
H xoxoxoxoxoxoxoxo
TIER 3
HARM UPPER x..o..oo..o.o.o.
BASS R-.O..Rr..F-O.A-
K x.x.......x..o..
S ....x..g.g..x..g
H xoxoxoxoxoxoxoxo
TIER 4
HARM UPPER x..o..oo.ox.o.o.
BASS R-.O.oRr.oF-O.A-
K x.x.......x..o..
S ....x..g.g..x..g
C ....x.......x...
H xoxoxoxoxoxoxo..
O ..............o.

STYLE REGGAE  steps 4
TIER 1
HARM UPPER ..x...x...x...x.
BASS R-----..F-------
K ........x.......
R ........x.......
H o.o.o.o.o.o.o.o.
TIER 2
HARM UPPER ..x...x...x...x.
BASS R---..r.F-..R...
K ........x.......
S ........x.......
R ........x.......
H ..o...o...o...o.
TIER 3
HARM UPPER ..xo..x...xo..x.
BASS R-.r..R.F-.f.O..
K ........x.......
S ........x.......
R ........x.......
H ogo.ogo.ogo.ogo.
TIER 4
HARM UPPER ..xo..x...xo..x.
BASS R-.r..RtF-.f.ON.
K x...x...x...x...
S ........x.......
H ..o...o...o...o.
O ..............o.

(The style list on the FM-1 stays M8's 11, in M8's order, with these names. WALTZ sets 3/4 as in M8.)

- Fast checks: pytest (the new C = Python tier tests, the 13 songs' golden tests identical, the generated files up
  to date); song_fuzz 60 s; m8p4 with its level expectations updated to tiers (record which expectations changed).

PART 4 — BACKTRACK controls: one knob map, changes on the phrase (D053)
SETUP and the PLAY screen use THE SAME knob map, so no knob changes meaning between the two screens:

| Control | Does (SETUP and PLAY) | On PLAY, takes effect |
|---|---|---|
| K1 | progression (1-36) | at the next phrase boundary |
| K2 | style (11) | at the next phrase boundary |
| K3 | complexity: 1 SIMPLE, 2 MEDIUM, 3 BUSY (default 2) | at the next bar line |
| K4 | melody sound (the 30 presets, as M8) | at once (as M8) |
| PRESETS | key, a semitone a click, all 12 (the music moves the nearest way, as SONG LEARN's KEY mode) | at the next bar line |
| ALGO | structure: LOOP, VERSE/CHORUS, FULL | at the next phrase boundary |
| SELECT | tempo (as M8) | at once (as M8) |

- Phrase boundary: the next bar line where (bar - first bar of the current section) is a multiple of 4, or the
  loop's wrap, whichever comes first. While a change waits, the top line shows "NEXT: <what>" (e.g. "NEXT: REGGAE",
  "NEXT: 15 MINOR POP", "NEXT: FULL"); several waiting changes are applied together at the same boundary; turning
  the knob again updates the waiting value (turning back to the current value cancels it).
- Progression and style changes use the switch method chosen in Part 0 (M9-01). After the switch the music goes on
  from the SAME position in the form (the same section and bar number) if the new file has it, else from the start
  of the new structure's loop (LOOP, VERSE/CHORUS) or the first VERSE (FULL). No count-in (Part 1's rule).
- Structure change: if the play position is inside the new structure's loop, play on; otherwise jump to the new
  loop's start at the boundary. FULL removes the loop; reaching the ENDING stops as in M8.
- Complexity: all three layers' levels change together to the new tier (chorus = complexity + 1 as Part 3).
- Removed from BACKTRACK (MUST): the per-layer level knobs (K1-K3 no longer set drums/bass/harmony levels
  separately), SONG LEARN's start point and loops (PRESETS by section, ALGO by bar, SEQ's SECTION / BARS / SONG
  loops), and SAVE's KEY mode (PRESETS does the key). On the BACKTRACK play screen SEQ and SAVE show the "not
  implemented" pop-up (D044 rules). SONG LEARN is unchanged.
- Kept as M8 on the PLAY screen: PLAY play/pause, HOME stop then SETUP, the layer buttons (press: FULL / QUIET /
  MUTED; hold: your part), EDIT views, ARP count-in, OCT-/OCT+, GLO help, LFO held: the melody yours.
- Implementation hint: bt.c's play input runs before input_play; in BACKTRACK it MUST consume K1-K4, PRESETS, ALGO,
  SEQ and SAVE, and input_play MUST NOT see them (a flag, e.g. ui.bt_mode, that input_play checks before reading
  LAYER_ENC, PRESETS, ALGO, SEQ, SAVE). Do not change SONG LEARN's behaviour.
- SETUP screen rows (top to bottom): K1 PROGRESSION: number, name, and a MAJOR or MINOR badge (MAJOR in the amber
  highlight colour, MINOR in a clearly different colour, e.g. the blue used for the QUIET state; text, not colour
  alone), the numerals under it; K2 STYLE; K3 COMPLEXITY; K4 SOUND; PRESETS KEY (e.g. "C major" / "A minor", and
  the progression's chords in it); ALGO STRUCTURE; SELECT TEMPO. If seven rows do not fit 240 px in the current
  fonts, use FONT_XS for the knob labels and values except the progression's name; never cut text; record the
  layout in decisions-M9.md.
- Badge rule: MAJOR for tsv mode major and blues, MINOR for minor and minorblues. The same badge appears on the
  play screen's top line next to the progression's name.
- PLAY screen top line: the progression's number and name, its badge, the style, the structure, "C1"/"C2"/"C3";
  the layers row shows each layer's tier now playing (L1-L4, so a chorus shows one more) and FULL/QUIET/MUTED.
- Help screens (GLO) for SETUP and PLAY updated to this map.
- Checks (m9p4): every row of the table above on both screens; a style change while playing waits for the
  boundary, shows NEXT, then plays in the new style from the same bar (or the rule above) with no count-in and no
  xrun; same for progression and structure; complexity changes at the next bar and moves the chorus tier with it;
  PRESETS key re-spells the chords; SEQ and SAVE give the pop-up on PLAY; SONG LEARN's play screen unchanged (m5p3,
  m6p1, m8p4's SONG LEARN parts pass).

PART 5 — Progressions: renames, 6 new minor ones (D054)
- tools/backtracks.tsv: rename (names AND slugs; the generated folders follow):
  6 LA BAMBA -> FIESTA; 13 MARIO -> HEROIC; 14 CREEP -> BITTERSWEET. Remove the old folders
  (songs/backtracks/bt06-la-bamba, bt13-mario, bt14-creep) from the repo (git rm).
- Add six minor progressions (drafts by the planning chat; the user prunes by ear), numbers 31-36, mode minor:
  31 MINOR FOLK   i iv v i         | bVI bVII i i
  32 EPIC         i bIII bVII iv   | -
  33 AEOLIAN      i bVI bVII i     | -
  34 HARMONIC     i bVI iv V       | -
  35 MINOR 2-5-1  ii7b5 V7 i i     | -
  36 MINOR FUNK   i7 i7 iv7 iv7    | -
  "ii7b5" = half-diminished (m7b5) on ii; if tools/backtracks.py does not accept it, add it (pytest). "i7" / "iv7"
  = minor seventh chords. Bridges by M8's rule.
- Generate, compile all 36; report the total size and the song store's room with 13 songs + 36 backtracks.
- No progression is named after a song or a trademark (check the 36 names; record any doubt in decisions-M9.md).
- Fast checks: pytest (the 36 golden C = Python), m9p4 with progressions 31-36.

PART 6 — Version, docs
- FW_VERSION 0.9.0-m9, identity FM-1_46000900, the splash's second line "m9". USB / MIDI names unchanged.
- docs/controls.md "M9" section: the mode screen, SETUP, PLAY, the help screens (screenshots), and the census table.
- docs/song-format.md: energy is for kind backtrack only; the tiers of a backtrack (no format change: say so).

PART 7 — Tests, budget, install checklist. STOP for review.
- Full suite + make m51-test once on the final package (test-runner subagent), plus the m9 checks:
  m9p1, m9p2, m9p4; m9census (D044 census extended: SETUP and PLAY of BACKTRACK with the new map: every button has
  its job or the pop-up); m9sweep (11 styles x 3 structures x complexity 1-3 x progressions 1, 9, 20, 35 at 250
  BPM, plus one live style change and one progression change in each run: to the end without xruns); the mode
  screen and the rescues (OCT-+OCT+, REC, GLO+HOME) unchanged; 24 quick HOME taps never the mode screen.
- m9cpu: BACKTRACK worst case (the busiest style at tier 4 in a chorus, FULL, every layer on, the melody sound's 4
  voices with its effects, during a seamless switch if M9-01 chose A): MUST be <= 60 % of a block. If over: first
  thin tier 4 of the offending style (fewer hits; record it), never change SONG LEARN.
- Budget table as step-M8-install.md; app space left MUST be >= 32 KB (M9.1 needs it).
- docs/step-M9-install.md: install over M8; then (the user runs these) delete the three renamed backtracks:
  tools/fm1song.sh delete bt06-la-bamba, bt13-mario, bt14-creep (marks the records deleted, erases nothing); send
  songs/backtracks/*/ (all 36; same slugs are replaced); if Part 2 changed a private song, resend it. Known items;
  STOP.
- LOG: the usual entry, plus "Decisions for review: M9-01 ... M9-NN".
```

Not in M9: saving user presets (M9.1, D040); swappable sound kits for BACKTRACK (M9.1); a major/minor switch (D054:
none, by design); the chorus lift in SONG LEARN songs (later, if wanted); recording; msfa/DX7; song info page;
ALBUM level.
