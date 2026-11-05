# Controls (proposal, M1)

M1's control map for the FM-1 panel. A proposal for step 1.6 to refine; the screens are simple on purpose.
Rule kept from Step 0.7: **REC, OCT− and OCT+ stay free** (fm1-base's rescues at power-on), and the firmware never
needs a button held at start-up.

## Song list

| Control | Does |
|---|---|
| SELECT (turn) | move the cursor |
| PLAY or EDIT | open the song (the play screen, stopped) |
| keys | Electric Piano 1 on channel 16 |
| MASTER | volume |

Each row: title, key, metre, tempo. A song that fails its check when opened shows `SONG ERROR <reason>` and
stays on the list.

## Play screen

| Control | Does | Light |
|---|---|---|
| PLAY | play from the top (with the count-in if on) / stop | green while playing |
| FX, SEL, ENV, LFO | mute / unmute drums, bass, harmony, melody, at any time | lit = the layer plays |
| K1, K2, K3, K4 (turn) | level (variant) of drums, bass, harmony, melody: 1 .. what the song has | |
| SELECT (turn) | tempo, 5 % a detent, 50–120 % | |
| ARP | count-in on / off (default on) | lit = on |
| SEQ | loop: off → whole song → this section → off | lit = a loop is on |
| HOME | stop, back to the song list | |
| MASTER | volume | |
| keys | Electric Piano 1 on channel 16, over the song | the held keys |
| REC, OCT−, OCT+ | **not used** (rescue) | |
| EDIT, GLO, SAVE, ALGO, PRESET | free (for step 1.6: e.g. transpose, keyboard view, settings) | |

Screen, top to bottom: title, tempo %, key, LOOP / CNT flags, `bar:beat`; the current chord large with NEXT and a
countdown ("IN 3") in the last bar before a change; the bar strip (this bar + the next 3, chord names in boxes,
a chord held over from the bar before is dimmed, the beat cursor under the first box); the four layers (level
`L1`/`L2`, amber `MUTE`); the section name and a debug line `CPU n% V now/most`.

## Why this map

- The layer mute buttons are the same as Step 0.7's (the user has used them).
- K1–K4 sit in a row like the four layer buttons, so "the knob above the layer" is easy to remember.
- SELECT is the big encoder: tempo is the setting changed most while practising.
- ARP / SEQ are play-mode buttons on stock firmware too (arpeggiator, sequencer), next to PLAY.
- Open questions for step 1.6: show the keyboard voicing (stored per chord at levels 1 and 2); a way to start
  from a section, not only from the top; whether HOME should ask before stopping.
