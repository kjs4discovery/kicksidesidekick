# Milestone M3 — emulator report and budget

Package: `build/step-M3/final/fm1acc-0.4.0-m3.fwsc`, identity **FM-1_46000400**, v0.4.0-m3, built from the Part 8
commit `dde3dc1` with the version 0.4.0, sha256 `7bb7e58ad8c27cb8eb4038fef641f02a708fa01344fd856931cf1280d027787d`.

`tools/m3_test.py` ran on **this exact package** in fm1-emu (fm1-live, patched: `tools/apply_patches.sh`):
**235 checks, ALL PASS** (`build/step-M3/final/report.md`, screens in `build/step-M3/final/emu/`).

Laptop-only tests:
- the song store with a power cut at every operation (`tools/store_test.sh`): ALL PASS;
- the compiler: 75 pytest tests, including C/Python parser agreement on 3000 mutated 1.0/1.1/1.2 files;
- the device parser fuzzed for 10 minutes under ASan/UBSan (`tools/song_fuzz.sh 600`, seeds in formats 1.2, 1.1
  and 1.0): 1,788,000 files, no sanitizer report;
- the guitar table (`tools/guitar_check.py`): all 53 shapes and 12 barre templates match their chords, and all the
  songs' chords in all 12 keys find a shape.

Part builds, each tested on its own release build when it was made (`build/step-M3/partN/report.md`):

| Part | Build | Identity | Checks then |
|---|---|---|---|
| 1 bugs | 0.3.1-m3p1 | FM-1_46000301 | part1: all pass |
| 2 your part | 0.3.2-m3p2 | FM-1_46000302 | parts 1–2, 2 full songs: all pass |
| 3 loops | 0.3.3-m3p3 | FM-1_46000303 | parts 1–3: all pass |
| 4 polish, settings | 0.3.4-m3p4 | FM-1_46000304 | parts 1–4 and 2 full songs: all pass |
| 5 Happy Birthday | 0.3.5-m3p5 | FM-1_46000305 | **the full suite, 219 checks: all pass** (the early install point) |
| 6 guitar, tab | 0.3.6-m3p6 | FM-1_46000306 | parts 2, 6: all pass |
| 7 scrolling views | 0.3.7-m3p7 | FM-1_46000307 | parts 6, 7: all pass |
| 8 folders, guide | 0.3.8-m3p8 | FM-1_46000308 | parts 2, 4, 8: all pass |

## What was tested (by item)

| Part | What | Result |
|---|---|---|
| 1 | L1 drums with a backbeat (Greensleeves, Oh! Susanna); a full flash on every beat; one A and one B; the start point kept after a loop; the count-in over the keyboard view | PASS |
| 2 | your part = the muted layer (each of the four; several: the last muted); key flashes = the part's attacks at L1 and L2; drums: no key lights; none muted: the cue still shown | PASS |
| 3 | A–B loop set and confirmed (stopped and while playing), not on while being set, HOME back and cancel, SEQ off, PRESETS to section edges | PASS |
| 4 | beep 21x M2's click peak; volume LOW / HIGH; 2-bar count-in; settings saved and kept over a restart; blink off; L2 inversions (19 of 29 chords, Amazing Grace); count-in in every view | PASS |
| 5 | Happy Birthday: 3/4, pickup, played from the list | PASS |
| 6 | guitar table checked; guitar view and key change; bass tab scrolling; the Blues at 250 BPM in the tab, on time | PASS |
| 7 | melody roll, notation (treble, bass, a flat key, 3-bar zoom), drum grid; the Blues at 250 BPM in the notation and the grid, on time | PASS |
| 8 | folders (All songs, 5 categories, Other), 1.1 and 1.0 files still read, a send to M2 gets 1.1, guide 30 % | PASS |
| M2 items | the store (9 songs byte for byte, junk, damaged, stalled, power cuts at 6 points), start-up, practice, key and views, levels (no sample at full scale), flash ids 856014 / b36014 / unknown c84017 | PASS |

## Per song (own tempo, the full run)

| Song | Length vs prediction (frames) | Chord ahead of sound | Voices (of 24) | CPU |
|---|---|---|---|---|
| Amazing Grace | +56 | 81–99 ms | 14 | 14 % |
| Four Chords in G | −112 | 81–99 | 12 | 12 % |
| Greensleeves | −19 | 64–99 | 12 | 11 % |
| Happy Birthday to You | −189 | 64–93 | 14 | 14 % |
| Londonderry Air | −76 | 87–99 | 11 | 12 % |
| Oh! Susanna | −64 | 81–99 | 13 | 13 % |
| Scarborough Fair | −148 | 81–99 | 9 | 10 % |
| Twelve-Bar Blues | −146 | 52–99 | 14 | 14 % |
| Saints | −200 | 81–99 | 12 | 13 % |

(Length: within one 256-frame block of the prediction. The store check's text still says "format 1.1"; the songs
were sent as 1.2.)

## Budget

| | M2 | M3 | Room |
|---|---|---|---|
| App image (flash, slot ~581 KB) | 395,232 B | **414,272 B** (the guitar table, the views, notation glyphs: +19 KB) | ~167 KB |
| RAM .data+.bss (of 98,304) | 74,276 | **74,740** | 23 KB |
| RAM pool (of 344,064) | 215,048 | **215,304** | 128 KB |
| Song store (flash data area, 69 sectors) | 8 songs in 22 | 9 songs (format 1.2), 83,360 B in 24 sectors; 45 free (~20 more songs of this size) | |
| Settings (flash) | — | 2 sectors at 0xFC000 (fm1-base's store, A/B) | |
| CPU (audio load, worst bar) | 15 % (250 BPM) | 15 % (the Blues at 250 BPM in the notation view); 14 % at the songs' own tempos | |
| Voices | 14 of 24 | 14 of 24 | |

The UI redraws the scrolling views for every pixel the song moves. This costs the main loop, not the audio: no
deferred audio block in any run, and the whole Blues at 250 BPM in the notation and drum grid views stays on time.

## Screens

See docs/controls.md, section M3 (mock-ups first, then the built screens, in docs/step-M3/).

## Not covered by the emulator

- How it sounds and feels: the beep, the guide level, the clarinet and flute.
- The real LEDs: whether the keys' middle level is visible, and the PLAY flash.
- The real USB song send and settings save on the unit's flash. Both are tested only in the emulator, including
  power cuts for the store.
- Whether a guitarist and a staff reader find the guitar boxes and the notation right.
- The melodies typed from memory.
