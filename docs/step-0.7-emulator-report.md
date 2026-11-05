# Step 0.7 prototype: emulator test report

Package: firmware/app/build/fm1acc.fwsc (commit 8bc977e, sha256 58be4537be314bf463dc828c919b18675c2626d01be5a01724560b3cc760d54f, identity FM-1_4600), emulator: fm1-live --bench (headless, emulated clock).

## 1. Harmony muted

- PASS: the stats block is in each run's RAM dump
- PASS: fewer voices with harmony muted in 32 of 32 bars (most: all layers 14, harmony muted 7)
- the first sound at frame 147461 (3344 ms of audio)
- PASS: the all-layers and harmony-muted audio are identical until the song starts (first difference at frame 178944)
- PASS: harmony muted still plays: level 2277 (all layers 2380)
- written: step-0.7/harmony-muted.wav (local, git-ignored; `make proto-test` remakes it) (75.4 s)
- audio output starts 93.2 ms after the emulated clock (fm1-live's WAV vs its end time)

## 2. Screen

- bar 1 beat 1: step-0.7/bar-01-1.png
- bar 7 beat 2: step-0.7/bar-07-2.png
- bar 16 beat 3: step-0.7/bar-16-3.png
- bar 25 beat 1: step-0.7/bar-25-1.png

Chord changes (the new chord's first audio frame from the bass alone; runs ending d ms before that frame is output):

| chord | tick | first frame | d=-20 | d=-10 | d=0 | d=10 | d=20 | d=30 | d=40 | d=60 | d=80 | d=120 | lead |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| G7 | 2880 | 273440 (x349) | new | new | new | new | new | new | new | old | old | old | 40 ms |
- PASS: G7 at tick 2880: on the screen 40 ms or more before its first frame is output
| D | 10080 | 745973 (x5) | new | new | new | new | new | new | new | old | old | old | 40 ms |
- PASS: D at tick 10080: on the screen 40 ms or more before its first frame is output
| G | 17280 | 1218591 (x301) | new | new | new | new | new | new | new | old | old | old | 40 ms |
- PASS: G at tick 17280: on the screen 40 ms or more before its first frame is output
| D7 | 24000 | 1659686 (x138) | new | new | new | new | new | new | new | old | old | old | 40 ms |
- PASS: D7 at tick 24000: on the screen 40 ms or more before its first frame is output
| D7 | 34560 | 2352932 (x378) | new (heard) | new | new | new | new | new | new | new | new | old | 80 ms |
- PASS: D7 at tick 34560: on the screen 80 ms or more before its first frame is output
| G | 44640 | 3014470 (x13) | new (heard) | new (heard) | new | new | new | new | new | new | new | old | 80 ms |
- PASS: G at tick 44640: on the screen 80 ms or more before its first frame is output

'heard': the run's audio already holds the chord's first frame. A FAIL would be a 'heard' cell with the old chord.
The d values place each run's end from fm1-live's WAV length and its end time (a ~93 ms offset); the 'heard' marks
show that offset is good to about 20 ms, so read the leads as 40-80 ms, roughly +-20 ms. The screen being ahead is
the point (PP_LEAD_MS = 80 ms); with 30 ms it was up to ~20 ms late. The LCD's SPI speed on the device may differ.

## 3. Flash ids

- PASS: flash id 856014: plays (level 2386 over the last 3 s), step-0.7/flash-856014.png
- PASS: flash id b36014: plays (level 2386 over the last 3 s), step-0.7/flash-b36014.png
- PASS: flash id 123456: plays (level 2386 over the last 3 s), step-0.7/flash-123456.png
- PASS: 856014 and b36014: identical screens (flash accepted)
- PASS: 123456 (unknown): the top band differs (RAM ONLY shown; see the picture)
- PASS: 123456 (unknown): the same chord at the same time (the song plays on, no crash)

## 4. Load (emulator)

Per bar: the most EAS voices, the platform's audio load (plat_cpu_pct, smoothed over ~16 blocks) and the longest EAS render in % of one block (256 frames, 5.8 ms).

| bar | voices all | voices harmony muted | load all % | load harmony muted % | render all % |
|---|---|---|---|---|---|
| 0 | 1 | 1 | 5 | 5 | 4.9 |
| 1 | 8 | 5 | 10 | 8 | 11.5 |
| 2 | 10 | 6 | 11 | 8 | 13.2 |
| 3 | 10 | 6 | 11 | 8 | 13.2 |
| 4 | 10 | 6 | 11 | 8 | 13.1 |
| 5 | 9 | 6 | 10 | 8 | 12.2 |
| 6 | 9 | 6 | 10 | 8 | 12.2 |
| 7 | 10 | 6 | 11 | 8 | 13.1 |
| 8 | 10 | 5 | 11 | 8 | 13.0 |
| 9 | 10 | 6 | 11 | 8 | 13.1 |
| 10 | 10 | 6 | 11 | 8 | 13.2 |
| 11 | 10 | 6 | 11 | 8 | 13.2 |
| 12 | 10 | 6 | 11 | 8 | 13.1 |
| 13 | 9 | 6 | 10 | 8 | 12.2 |
| 14 | 10 | 6 | 11 | 8 | 13.1 |
| 15 | 10 | 6 | 11 | 8 | 13.1 |
| 16 | 13 | 6 | 14 | 8 | 15.8 |
| 17 | 14 | 7 | 14 | 9 | 16.5 |
| 18 | 10 | 6 | 11 | 8 | 13.2 |
| 19 | 10 | 6 | 11 | 8 | 13.2 |
| 20 | 10 | 6 | 11 | 8 | 13.1 |
| 21 | 9 | 6 | 10 | 8 | 12.2 |
| 22 | 9 | 6 | 10 | 8 | 12.2 |
| 23 | 10 | 6 | 11 | 8 | 13.0 |
| 24 | 10 | 5 | 11 | 8 | 13.0 |
| 25 | 10 | 6 | 11 | 8 | 13.1 |
| 26 | 10 | 6 | 11 | 8 | 13.2 |
| 27 | 10 | 6 | 11 | 8 | 13.2 |
| 28 | 10 | 6 | 11 | 8 | 13.1 |
| 29 | 9 | 6 | 10 | 8 | 12.2 |
| 30 | 10 | 6 | 11 | 8 | 13.0 |
| 31 | 10 | 6 | 11 | 8 | 13.1 |
| 32 | 9 | 6 | 10 | 8 | 12.0 |

Busiest bar (longest render, all layers): bar 17: 14 voices, load 14 %, render 16.5 % of the block. Most voices anywhere: 14 of 24. Longest render: 16.5 %. Blocks played: 12211.

Result: ALL PASS
