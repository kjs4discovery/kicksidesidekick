<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->
<!-- Copyright: The KickSide author -->
# KickSide - Your Musical SideKick: controls reference

**For firmware 0.10.5 (0.10.5-m10.4). controls.md is the history.** Written from the firmware's code (the help tables,
`learn_jobs`, the legends, the settings rows, the pop-up texts), not from controls.md. Every control is named as printed on the
panel. No screenshots.

The FM-1's panel, for orientation: left of the screen **MASTER**, **SELECT** (top row), **PRESETS**, **ALGORITHM** (second row),
**OCT-** **OCT+**; right of it **K1-K4** (top), then two rows of buttons: **FX SEL ENV LFO EDIT GLO** / **HOME SAVE ARP SEQ PLAY REC**.
The keys are the 13 keys below the screen.

## Everywhere

| Control | Does |
|---|---|
| MASTER | the volume |
| OCT- / OCT+ | the keys' octave, 3 down to 3 up (the message line says "OCTAVE 1") |
| keys | play (the sound depends on the mode: below) |
| GLO | the help for this screen (SONG LEARN and BACKTRACK; SYNTH has its own); GLO again on the help: the settings; any other button on the help: back |
| HOME, a tap (released within 2 s) | up one level (below); from a mode's first screen, to the mode screen |
| HOME, held 2 s | straight to the mode screen from any screen except the mode screen and the WIPE question; the song stops |

A button with no job on a screen opens a small box "NOT IMPLEMENTED" (any button closes it, or 2.5 s) - unless **Pop-ups** is OFF
(then it does nothing). The encoders never open it. HOME never opens it.

## The mode screen

The first screen after the splash (the logo for 4 s, with the firmware version at the lower right), and where HOME ends. The title
"KickSide - Your Musical SideKick" ("Kick" in bold both times) and the mascot; under "modes:" three choices.

| Control | Does |
|---|---|
| SELECT | move the cursor: SONG LEARN ("Play with a song"), BACKTRACK ("jam over a band"), SYNTH ("play the synth") |
| PLAY or EDIT | start the mode |
| other encoders | nothing (their turns are dropped) |

## SONG LEARN

The band plays drums, bass, harmony and melody from a song stored on the unit; you mute one layer and the screen shows what to play.

### The folders

The first screen of SONG LEARN when songs are stored: "All songs" and one folder for each category in the songs, each with its count.

| Control | Does |
|---|---|
| SELECT | choose |
| PLAY or EDIT | open the folder (the song list) |
| HOME | the mode screen |
| GLO | help |
| keys | play along (Electric Piano 1) |

### The song list

| Control | Does |
|---|---|
| SELECT | move the cursor (one song a click) |
| PLAY or EDIT | open the song: its play screen, stopped, at the song's own tempo |
| HOME | up to the folders (with no songs stored: to the mode screen) |
| GLO | help |
| keys, OCT- / OCT+ | play along |

Each row: the title, then key, metre, tempo. A song that fails its check stays in the list with "SONG ERROR: <reason> (send it again)".
After the last song, a footer (never selected): "Download and install more songs from our website". It is not a pop-up, so
Pop-ups OFF does not hide it. The folders screen has no footer.

### The play screen

The top line: the song's title and the bar. The middle: what to play, in the view of the layer that is **your part** (harmony when a song
opens). The bottom row: the controls that work now (pages of the instruction row change every 6 s when stopped).

**Stopped**: "PRESS PLAY" and, under it, "from bar 9 (Chorus 1)" (the start point). **Count-in**: the beat in the bar (1 2 3 4 | 1 2 3 for a 4/4 song with a one-beat
pickup). **Paused**: "PLAY: from bar N" when ALGORITHM has moved the start meanwhile, otherwise PLAY goes on from where it stopped.

| Control | Does |
|---|---|
| PLAY | stopped: play from the start point (count-in as set); playing: pause; paused: go on (from the new start if ALGORITHM moved it) |
| HOME, a tap | playing, count-in or paused: stop and back to the start point; stopped: back to the song list; while setting a loop: back one step (B to A, A: cancelled) |
| K1, K2, K3 | the complexity level of drums, bass, harmony: pop-up "Drums Complexity Level" / "2 of 3" (a layer with one level: no job) |
| K4 | **Melody Sound**: SONG (the song's own melody instrument; the default) then the presets (the 30 stock ones, then your saved ones). A preset plays the melody layer on the synth ("no room: keeps SONG" if the song leaves no memory for it). The keys play the same sound (four voices shared with the melody layer). Kept until another song is opened. A melody layer with more than one level uses K4 for its level instead |
| FX, SEL, ENV, LFO | the layers drums, bass, harmony, melody. A press on a layer that is not your part makes it your part ("Track Selected", with "(press again to control volume level)"); a press on your part cycles its volume FULL, QUIET, MUTED ("Bass Volume") |
| SELECT | tempo, a BPM a click (five when turned fast), pop-up "Tempo"; in the scrolling and staff views SELECT zooms instead, and REC switches SELECT to tempo for 4 s |
| REC | the tempo mode on / off (see SELECT) |
| PRESETS | the key, a semitone a click (-5 to +6): pop-up "Key" / "E (+2)", "D (song key)" at 0. While setting a loop it snaps A or B to a section |
| ALGORITHM | the start point, a bar a click: pop-up "Start" / "BAR 9" with the section's name under it when that bar begins a section. While setting a loop, A or B by bar; with a loop A-B on: "LOOP ON / SEQ: OFF FIRST" |
| ARP | the count-in on / off |
| EDIT | the next view of your part (views hidden in the settings are skipped) |
| SEQ | the loop, step by step: whole song, then A (a press sets it), then B (a press sets it), then A-B on, then off |
| SAVE | no job (the box) |
| GLO | help; GLO again: the settings |
| keys | play along; the song's melody sound, or the preset chosen with K4 |

Views, by layer (EDIT steps through them): drums MAIN, DRUM GRID, DRUM NOTATION; bass MAIN, KEYBOARD, CHART, BASS TAB, NOTATION; harmony MAIN, KEYBOARD,
CHART, GUITAR (UKULELE with that setting), NOTATION, BAR VIEW; melody MAIN, KEYBOARD, CHART, MELODY ROLL, NOTATION, GUITAR TAB (the names the screen shows).

Setting a loop (SEQ with no loop): the bottom row says "ALGO bar A|PRESETS section|SEQ set|HOME cancel", then "... bar B ... HOME back".

### The song help and settings

GLO shows every control of the screen you came from (the song list, the play screen) and CPU, voices, the version. The settings (GLO again):
SELECT moves the row, ALGORITHM, PLAY or EDIT changes its value, GLO or HOME leaves (saved).

| Row | Values (default first where it is one) |
|---|---|
| Key lights blink | ON, OFF |
| Count-in length | 1 BAR, 2 BARS |
| Count-in volume | MEDIUM, LOW, HIGH |
| Note names (notation) | ON, OFF |
| Zoom at start | 1.5 BARS, 3 BARS |
| Quiet level | 50 %, 25 % |
| Key lights | ON, OFF |
| Views (hide some) | a screen: SELECT a view, PLAY, EDIT or ALGORITHM shows / hides it; each layer keeps at least one |
| Bass sound | +1 OCTAVE, REAL |
| **Pop-ups** | ON, OFF (see below) |
| Switch mode | PLAY: the mode screen |
| Chord boxes, tab | GUITAR, UKULELE |
| Scale guide | PENTATONIC, FULL SCALE, CHORD TONES |
| Wipe song store | a question; PLAY held 2 s erases every stored song (HOME or any other button: no) |

## BACKTRACK

A band plays a chord progression in a style; you play the melody on the keys with the scale marked on the screen. Backtracks are stored songs
sent from the computer.

### SETUP (the first screen)

Rows: K1 PROGRESSION (number, name, MAJOR / MINOR badge, the numerals), K2 STYLE (POP, ROCK, INDIE POP, BALLAD, SHUFFLE, STRIDE, WALTZ (3/4), COUNTRY,
BOSSA NOVA, FUNK, REGGAE), K3 COMPLEXITY (SIMPLE, MEDIUM, BUSY), K4 MELODY SOUND, PRE (PRESETS) KEY with its chords, ALG (ALGORITHM) STRUCTURE (LOOP,
VERSE/CHORUS, FULL), SEL (SELECT) TEMPO, SEQ KIT.

| Control | Does |
|---|---|
| K1 | the progression (pop-up with its name and badge) |
| K2 | the style (the tempo follows the style until SELECT has been turned) |
| K3 | the complexity |
| K4 | the melody sound: the 30 stock presets, then your saved ones |
| PRESETS | the key |
| ALGORITHM | the structure |
| SELECT | the tempo (60-200 BPM) |
| SEQ | the band kit: STUDIO, ACOUSTIC, ELECTRO, CHIP, STUDIO ... (pop-up "Band Kit") |
| PLAY | start (lit when there are backtracks) |
| HOME, a tap | the mode screen |
| GLO | help; GLO again: the settings |
| keys | play the melody sound (four voices) |

The kit sets the drums (STUDIO and ACOUSTIC: the General MIDI kit; ELECTRO: an 808; CHIP) and the bass and harmony sounds (STUDIO: the song's own).

### The play screen

The top line: the progression and, at the right end, the **section indicator**: the section playing (INTRO, VERSE, CHORUS, BRIDGE, ENDING), blinking in the last
bar before a different section. It says **LOOP** only while the structure is LOOP and no other structure is waiting; the frame ALGORITHM is turned to
VERSE/CHORUS or FULL it shows the section playing. Under it: what waits ("NEXT: STYLE REGGAE +1") or the style, structure and complexity.

| Control | Does |
|---|---|
| K1, K2, ALGORITHM | progression, style, structure: wait for the next phrase boundary (marked * in the help) |
| K3, PRESETS | complexity, key: at the next bar line |
| K4, SELECT, SEQ | at once: the melody sound; the tempo; the band kit (drums from the next hit, bass and harmony programs sent at the player's next event; notes sounding keep their sound until released) |
| PLAY | pause / go on (stopped: start) |
| HOME, a tap | in any state (playing, count-in, paused, stopped): stop and back to SETUP, in one press |
| FX, SEL, ENV | choose drums, bass, harmony as your part, again: its volume |
| LFO | the melody is yours (you play it with the keys: no volume) |
| ARP | the count-in on / off |
| EDIT | the next view (SCALE KEYBOARD, SCALE FRETBOARD) |
| REC | the tempo mode as in SONG LEARN (SELECT is the tempo here anyway) |
| GLO | help; GLO again: the settings |

The settings are the same rows as in SONG LEARN.

### No backtracks

"NO BACKTRACKS / Please visit our website / to download songs." (the BACKTRACK screen); PLAY does nothing.

## SYNTH

The keys play a synthesizer with 30 stock presets and 30 places for your own (U01-U30).

### The main screen

| Control | Does |
|---|---|
| PRESETS | the sound (the list: 1 E-PIANO, 2 E-GUITAR, 3 E-SITAR, 4 PARKER, 5 POTATO, 6 EDS, 7 STAB 84, 8 NEON PAD, 9 DUSK STRINGS, 10 OCT BOUNCE, 11 SUNSET LEAD, 12 SYNC SCREAM, 13 CHROME BRASS, 14 GLASS PLUCK, 15 VAPOR KEYS, 16 RISER, 17 CHIP BLIP, 18 DRIVE BASS, 19 ZUNJET, 20 LASER ARP, 21 NIGHT DRIVE, 22 SAW STRINGS, 23 HORIZON PWM, 24 TAPE CHOIR, 25 GRID PULSE, 26 OUTRUN BASS, 27 MIAMI PAD, 28 CHROME ARP, 29 NEON BELLS, 30 VHS STRINGS, then U01...) |
| SELECT | the tempo (arpeggio, delay and drums follow) |
| K1-K4 | cutoff, resonance, reverb mix, delay mix (pop-up with the name and value) |
| ALGORITHM | the edit pages, forward and back through the main screen |
| EDIT | the next edit page; after the last, the main screen |
| ENV | the ENVELOPE page (on it: the main screen) |
| FX | the first FX page; on an FX page the next, after FX 4 the main screen |
| ARP | the arpeggiator on / off |
| PLAY | the drums on / off |
| SEQ | the drum pattern menu: SELECT, SEQ chooses, PLAY chooses and plays, HOME back |
| SAVE | save this sound (below) |
| HOME, a tap | an edit page: the main screen; the main screen: the mode screen |
| GLO | the SYNTH help; GLO again: SYNTH settings |
| LFO, SCL, REC | no job yet |

### The edit pages

ENVELOPE, FILTER, SOUND A, SOUND B, ARP, FX 1, FX 2, FX 3, FX 4: K1-K4 are the page's four values. The title names the preset ("ENVELOPE - E-PIANO",
"ENVELOPE - U05 ZUNJET"); the page number is at the right.

### Save (SAVE)

"SAVE TO Unn": SELECT picks the slot (U01-U30), K1 moves the cursor in the name (up to 12 characters), K2 changes the character, **SAVE** saves (a slot that is taken asks
"REPLACE Unn name?" and SAVE again replaces), **HOME** cancels (nothing is written). The pop-up "SAVED U05" confirms.

### SYNTH settings

Drum volume 0-10, Drum kit ACOUSTIC / 808 / CHIP. SELECT moves, ALGORITHM, PLAY or EDIT changes, GLO or HOME leaves (saved).

## Pop-ups

A **value pop-up** is the box over the screen with a title and a value, opened when a knob or button changes a setting (K1-K4, SELECT, PRESETS, ALGORITHM, the
layer buttons, SEQ in BACKTRACK ...). It closes 1.5 s after the last turn, or by a press, which then does its job; the music and the lights go on.
The **Start** pop-up shows the section's name large under the bar number.

The setting **Pop-ups** (GLO twice): ON or OFF. OFF silences the value pop-ups, the "Track Selected" box and the "NOT IMPLEMENTED" box. It does not silence
confirmations and errors ("SAVED U05", "NOT SAVED"), questions, warnings, FACTORY RESET and WIPE. SYNTH's bottom row shows the turned value instead.
**Pop-ups is ON again after every firmware update.**

## Built-in songs

From 0.10.5 the firmware itself holds the starter library, `songs/library.txt`: **15 songs and 42 backtracks**, packed small inside the firmware (about 26 KB). They are
a second source beside the song store, so a **new FM-1, or one after a factory reset, already has them**: SONG LEARN lists the 15 songs and BACKTRACK the 42 backtracks,
folders and categories as for any song, and they open and play exactly like a sent song. Private songs are never built in.

- **The store wins.** A song you send with the same slug as a built-in (for example `oh-susanna`) replaces its built-in copy in the lists; **deleting** it brings the built-in back.
  There is never a duplicate in the list.
- **Sending** a song that is not built in adds it to the store as before. **Deleting** a song that is only built in does nothing (it is not in the store: `fm1song delete`
  answers "not found"); a built-in cannot be removed except by a different firmware.
- **WIPE SONG STORE and the factory reset** erase the store (and the presets and settings, for the reset) but not the built-ins: the screen then says "Only the built-in songs now."
- **`fm1song list` shows only the store**, never the built-ins: its song count is the store's. A unit with nothing sent lists "0 song(s)" and still plays the 57 built-ins.
- The built-ins use no song-store space. Each is unpacked into a small buffer when it is opened (SONG LEARN one at a time, BACKTRACK two while it swaps versions);
  a built-in that failed to unpack or check would show SONG ERROR and the rest would work.
- If the flash chip is not usable ("NO SONG STORAGE") the lists stay empty, built-ins included.

## When there is nothing to show

With the built-ins, the two empty screens below appear only if the built-in pack itself is damaged (and the store is empty), or after nothing is left to list.

| Screen | Text |
|---|---|
| SONG LEARN, no songs | "NO SONGS YET / Please visit our website / to download songs. / They stay here when it is off." |
| BACKTRACK, no backtracks | "NO BACKTRACKS / Please visit our website / to download songs." |
| the flash chip is not usable | "NO SONG STORAGE / The flash chip is not usable / (RAM ONLY): songs cannot be kept." The settings and presets are not kept either |

## A clean install and the factory reset

A **factory reset** (`tools/fm1song.sh factory-reset --yes`, run by you; without `--yes` it only explains) erases the song store, both preset sectors (U01-U30) and puts the
settings back to their defaults; the unit then shows "FACTORY RESET DONE / SWITCH OFF AND ON" and reads nothing until it is switched off and on. It is refused in SYNTH and
BACKTRACK. The firmware and the panel calibration stay. `tools/fresh_install.sh PACKAGE` does the install, the reset and the song library in one go. After a reset the unit
shows the built-in songs only (the empty screens appear only if the built-in pack is damaged); the `fresh_install.sh` library send then stores copies of the same 57 files.
