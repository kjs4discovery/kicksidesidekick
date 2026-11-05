# Controls (M2; M3 changes at the end)

The FM-1's panel, as M2 uses it. M1's map (2026-11-04) was the start; M2 adds what the user asked for after testing
M1 (D020: tempo in BPM, loops you can find, pause, the keys' octave) and the planning chat's rule from M1's lesson
(the user could not find section looping): **every screen shows a short legend of its controls, and GLO shows them
all**. New screens were drawn here first (mock-ups, `tools/mock_screens.py`), then built; the built screens are in
`docs/step-M2/` (emulator screenshots).

Kept: **REC is not used** (held at power-on: the chip's update mode). OCT− and OCT+ held *together* at power-on
stay fm1-base's panel set-up; after start-up they shift the keys' octave (D021). Nothing needs a button held at
start-up.

Panel, for orientation: left of the screen MASTER, SELECT (top row), PRESETS, ALGORITHM (second row), OCT− OCT+;
right of it K1–K4 (top), then two rows of buttons: FX SEL ENV LFO EDIT GLO / HOME SAVE ARP SEQ PLAY REC.

## Start-up

The splash: the logo (a placeholder: `firmware/assets/tmp-logo.jpg`, the one file to replace) with the name and
version, for at least 3 s; then the song list. The power-on checks (REC, OCT− + OCT+) come before it and are not
delayed.

![splash](step-M2/mock-splash.png)

## Song list

| Control | Does |
|---|---|
| SELECT (turn) | move the cursor |
| PLAY or EDIT | open the song (the play screen, stopped, at the song's own tempo) |
| GLO | the controls (help) |
| keys | Electric Piano 1 on channel 16; OCT− / OCT+ their octave |
| MASTER | volume |

No songs stored: the list says how to send them (`tools/fm1song.sh send songs/*/`). A song that fails its check
when opened stays in the list with `SONG ERROR <reason> (send it again)`.

## Play screen

| Control | Does | Light |
|---|---|---|
| PLAY | stopped: play from the start point (count-in if on); playing: **pause**; paused: **go on** (count-in on: a bar of clicks, then from the paused bar's beat 1; off: from exactly where it paused) | flashes on every beat (bright on beat 1), count-in too |
| HOME | playing or paused: **stop**, back to the start point; stopped: back to the song list | |
| FX, SEL, ENV, LFO | mute / unmute drums, bass, harmony, melody, at any time | lit = the layer plays |
| K1, K2, K3, K4 (turn) | level (variant) of drums, bass, harmony, melody | |
| SELECT (turn) | tempo in **BPM**, 30–250, 1 BPM a click, 5 a click when turned quickly; amber when not the song's own; every song opens at its own | |
| PRESETS (turn) | the **start point** by section (each click: the next section; the section's pickup is kept). In LOOP BARS: bar **A** | |
| ALGORITHM (turn) | the start point by bar. In LOOP BARS: bar **B** | |
| SEQ | loop: off → **SECTION** (the section of the start point; while playing: the one playing) → **BARS A–B** (from the start bar, 4 bars to begin with) → **SONG** → off | lit = a loop is on |
| ARP | count-in on / off (default on) | lit = on |
| GLO | the controls (help); GLO again or any other button: back | |
| OCT−, OCT+ | the keys' octave, −3..+3 (shown for a moment) | lit = shifted that way |
| EDIT | the view: **chord** → **keyboard** → **chart** (part 3) | |
| SAVE | the **KEY** mode (part 3): SELECT changes the key, a semitone a click, through all twelve (the music moves the nearest way: at most 5 down or 6 up, so bass and melody stay in their range); SAVE again, or 4 s without a turn: back to the tempo. Every song opens in its own key | |
| keys | Electric Piano 1 on channel 16, over the song | |
| MASTER | volume | |

**Start point and loops, together.** Where PLAY starts is the start point: the song's start until you turn PRESETS
(by section) or ALGORITHM (by bar). A loop starts there too: SECTION loops the section the start point is in, BARS
loops A–B (A is the start point's bar). With a loop on, PLAY starts at the loop's start. Turning PRESETS while
playing a section loop moves the loop to the next section: it jumps there at its next bar line. Pickups follow
docs/song-format.md 3.3: a section's pickup is played at its start, and the same beats are cut from the end of the
loop; bars A–B have no pickup.

![stopped with a loop of bars 5-8](step-M2/mock-stopped-loop-bars.png) ![playing, a section loop](step-M2/mock-playing.png)

![paused](step-M2/mock-paused.png) ![the controls (GLO)](step-M2/mock-help.png)

Screen, top to bottom: title; tempo, key, CNT, the loop; `bar:beat` (or STOP, PAUSE, IN); the current chord large,
NEXT and a countdown (`IN 3`) in the last bar before a change; the bar strip (this bar and the next 3; no dimming of
a chord held over: the strip shows it is the same chord; an amber line under the bars of a loop, `A` on the start
bar); the beat cursor; the four layers (level `L1`/`L2`, amber `MUTE`); the section, the loop; **the legend**. The
CPU and voices line moved into the help screen.

## Part 3: the part you play (views, key, cue, bass, key lights)

**The part you play** is the muted one of harmony and bass: harmony muted → you play the chords; bass muted (harmony
playing) → you play the bass; **both muted → harmony wins** (the bass notes are still named on the status line).
That is a proposal (plan-M2 G asked for one): one rule for the rhythm cue and the key lights, nothing to set.

- **Key (SAVE, then SELECT).** The whole band but the drums moves; the chord names, the key name and the bass
  names are spelled for the new key (sharps or flats by its signature: F# in D, Gb in Db; C major mixed); the top
  line shows `A (+2)`, amber. A key change while playing ends the sounding notes at once (no note hangs at the old
  pitch).
- **Chord view** (as M1/M2, plus): left of the big chord, the **rhythm cue**: a note symbol for how long the part's
  attack lasts at the chosen level (whole, half, quarter, eighth, sixteenth; read from the song's own events for
  that level), bright for a moment at each attack, grey between; `L1`/`L2` under it. Shown when harmony or bass is
  muted.
- **Keyboard view**: the chord, `next … IN n`, and a keyboard C3–B5 with the chord's keys lit (the voicing of the
  harmony's level: L1 root position, L2 the inversions that move least), the next chord's keys outlined in the last
  bar before it, the bass note amber when the bass is muted.
- **Chart view**: the section's bars (16 a page) with their chords, the bar now framed.
- **Bass names** (the bass muted): the status line shows `BASS C3  then G2` (the note now and the next, in the key
  now, following the bass level).
- **Key lights**: the FM-1's own keys light for the part you play, while playing (and in the count-in, paused):
  harmony → the chord's keys (the level's voicing) bright and the next chord's keys dim in the last bar before a
  change; bass → the bass note. Folded into the 27 keys (F3–G5) and following OCT− / OCT+ (the lit keys are where
  those notes are under your hands). Keys you press light as ever.

![chord view: the rhythm cue, bass names](step-M2/mock-chord-cue.png) ![keyboard view](step-M2/mock-keyboard-view.png)

![chart view](step-M2/mock-chart-view.png) ![the KEY mode](step-M2/mock-key-mode.png)

## Why this map

- PLAY pauses, as on most players; stopping is HOME, the "go back" button, which goes one step further when already
  stopped (the list). "Stop and back to the top" is one press.
- SELECT, the big encoder, stays the tempo: the setting changed most while practising.
- PRESETS and ALGORITHM sit side by side under SELECT: left = A (or the coarse step, sections), right = B (or the
  fine step, bars).
- The layer buttons, K1–K4 above them and ARP / SEQ are as in M1 (the user has used them).
- GLO ("global") is the help: one press shows every control where it is on the panel.
- EDIT ("edit what you see") changes the view; SAVE was free and the key is a setting you set and keep for the song
  (the legend says `SAVE: KEY`). A shift (hold GLO + turn) was rejected: GLO opens the help on its press.

## M3 (docs/plan-M3.md), part by part

### Part 1: bug fixes

- **Beat light:** PLAY flashes at full brightness on every beat, for 220 ms on beat 1 and 110 ms on the others.
  M2's mid-level flash on beats 2.. was not visible on the unit.
- **Loop marks:** the strip repeats a loop's bars after its end (it shows the bars as they will be heard). The A and
  B marks are now drawn only on the bars in song order, so there is one A and one B.
- **Start bar after a loop:** setting LOOP BARS (A and B) no longer moves the start point. With the loop off, PLAY
  starts where the start point was set before, and the bottom line says so only if it was set.
- **Count-in in the keyboard and chart views:** the count replaces the view while it counts in, as the chord view
  does.

![count-in over the keyboard view](step-M3/p1-keys-countin.png) ![one A, one B](step-M3/p1-loop-marks.png)

### Part 2: "follow your part" (all four layers), blinking in rhythm

- **Your part** is the muted layer. The top line says so (`BASS - YOURS`), and its box in the layers row says
  YOURS with a double frame. The screen, the rhythm cue, the key lights and the views (EDIT) follow it:
  - **harmony:** the chord (as before);
  - **bass:** the bass note with its octave, the next note, the chord under it; the strip and chart show each bar's
    bass notes;
  - **melody:** the melody note, the next one, the chord; the bars' notes;
  - **drums:** the hit now (KICK, SNARE, ...; another sound of the same hit under the next), the next hit; the strip
    and chart show each bar as a small grid (kick, snare, cymbals, others). Drums have no keyboard view: EDIT skips
    it.
- **Several muted (proposal):** your part is the one muted last. Unmute it and the one muted before takes over. No
  extra control is needed, and the YOURS box shows which one is followed. The M2 "harmony wins" rule is gone.
- **Nothing muted:** the harmony is shown as before (with its cue), and the keys stay dark.
- **Key lights:** your part's keys flash bright at each of its attacks at its level, and stay at the middle
  brightness between attacks. The harmony's next chord is dim in the last bar before it. Drums: no key lights (the
  beat light is enough). The keyboard view does the same: bright amber at an attack, darker amber between, the next
  outlined.
- **Rhythm cue:** always shown (not only when muted), right of the big name, half its M2 height, no level label (the
  layers row has the level).
- Each part remembers its last view (EDIT).

Mock-ups (drawn first): ![harmony](step-M3/mock-p2-harmony.png) ![bass](step-M3/mock-p2-bass.png)
![melody](step-M3/mock-p2-melody.png) ![drums](step-M3/mock-p2-drums.png) ![keys between attacks](step-M3/mock-p2-keys-blink.png)

Built (emulator): ![bass](step-M3/p2-bass.png) ![bass, keyboard](step-M3/p2-bass-keys.png)
![bass, chart](step-M3/p2-bass-chart.png) ![melody](step-M3/p2-melody.png) ![drums](step-M3/p2-drums.png)
![two muted](step-M3/p2-two.png) ![none muted: the cue](step-M3/p2-none.png)

### Part 3: loops redesigned (bars A–B only)

- **One kind of loop:** bars A–B. M2's section loop and whole-song loop are gone: a section is A..B with PRESETS (it
  jumps to section edges), and the whole song is A at the first bar, B at the last.
- **Setting it, step by step** (the legend and a badge on the top line say what to do at each step):
  1. **SEQ:** "LOOP: SET A". A starts at the bar playing (or the start point, stopped). ALGORITHM moves A by a bar,
     PRESETS to a section's first bar.
  2. **SEQ:** A is set; "LOOP: SET B". B starts 4 bars on. ALGORITHM moves it (never before A), PRESETS to a
     section's last bar.
  3. **SEQ:** B is set and the loop is on. Playing outside it, the music jumps in at the next bar line.
  - **HOME** while setting: from B back to A; from A, cancelled (no loop).
  - **SEQ with the loop on:** the loop is off. Nothing else changes it, so there is no hidden state. PRESETS and
    ALGORITHM say "SEQ: OFF FIRST".
- **Shown:** the loop's bars are filled (dark amber) on the strip and the chart, with a line under them, a small A on
  bar A and a small B on bar B. The bar being moved has a white frame. There is exactly one A and one B: the marks
  are only on bars in song order, not on the strip's repeated copies after the loop's end. While A and B are being
  set the loop is not on yet.
- **Start points:** with the loop off, PRESETS = start from a section, ALGORITHM = start from a bar, as before. With
  the loop on, PLAY starts at A (on its beat 1, no pickup).

Mock-ups: ![set A](step-M3/mock-p3-loop-set-a.png) ![set B](step-M3/mock-p3-loop-set-b.png)
![loop on](step-M3/mock-p3-loop-on.png)

Built: ![set A](step-M3/p3-set-a.png) ![set B](step-M3/p3-set-b.png) ![loop on](step-M3/p3-loop-on.png)

### Part 4: screen polish, count-in, inversion label, settings

- **Legend:** button names in amber, actions in gray, a thin divider between pairs (`PLAY pause | EDIT view | ...`),
  on every screen. If a legend is too wide it uses the smaller font.
- **No section row:** the space goes to the views (144 rows, from y 32 to 176). The layers row moves down to 180.
  The start point (`FROM 3`) and the loop (`LOOP 5-8`) are on the top line. The section's name is still in the
  stopped screen and the chart view.
- **Inversion line** (chord view, under the strip): the voicing at the harmony's level, e.g. `C/E, 1st inversion`
  or `G7, root position`, with `L1 root pos.` or `L2 inversions` on the right. For another part: its chord and
  level. The keyboard view has the same line under the keys.
  - Level 1 is always root position; level 2 voice-leads through inversions. On Amazing Grace, 19 of 29 chords at
    level 2 are in an inversion.
- **Keyboard view, zoomed:** 11 white keys (about 1.5 octaves), 21 px each. The window follows your part's notes:
  it moves only when a note now or next would fall outside, with a key to spare. Notes show at their real pitch.
- **Count-in in every view:** the count (and the first chord or note) replaces the view while it counts in. This
  covers the chord, keyboard and chart views, and the later views use the same path.
- **Count-in sound:** a beep on a channel the song does not use: square lead, C6 on beat 1 and G5 on the others,
  70 ms. Its peak is 21x M2's wood-block clicks (7725 against 362 at MASTER 100 %). With no free channel it falls
  back to M2's clicks.
- **Settings (GLO, then GLO again):**
  - SELECT picks a row; ALGORITHM, PLAY or EDIT change its value; GLO or HOME go back and save.
  - The rows: key lights blink ON/OFF; count-in length 1 or 2 BARS; count-in volume LOW, MEDIUM or HIGH (the
    beep's channel volume 64, 96 or 127); note names under the notation ON/OFF (Part 7). Guide volume comes in
    Part 8.
  - Saved in fm1-base's settings store (OBJ_SETTINGS, two copies at 0xFC000 and 0xFD000, the newer valid one wins),
    only when something changed. The stock firmware never uses these sectors: they are all 0xFF in our unit's
    stock dump.
- **Help:** "GLO again: settings".

Mock-ups: ![layout and legend](step-M3/mock-p4-layout.png) ![zoomed keyboard](step-M3/mock-p4-keys-zoom.png)
![settings](step-M3/mock-p4-settings.png)

Built: ![chord view, L2](step-M3/p4-inv-l2.png) ![zoomed keyboard](step-M3/p4-keys.png)
![bass on the keyboard](step-M3/p4-keys-bass.png) ![settings](step-M3/p4-settings-screen.png)
![count-in in the chart view](step-M3/p4-count-chart.png)
