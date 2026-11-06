# Milestone M4 — emulator report and budget

Package: `build/step-M4/final/fm1acc-0.5.0-m4.fwsc`, identity **FM-1_46000500**, v0.5.0-m4. Firmware source = the
Part 4 commit with the version 0.5.0; sha256 `c811e0f7f002a2d7573843394e812490dbe3c2709c425d4b457c2351f767195b`.

`tools/m4_test.py` ran on **this exact package** in fm1-emu: **258 checks, ALL PASS** (`build/step-M4/final/report.md`, screens in
`build/step-M4/final/emu/`). The run covers:
- all 9 songs, each to the end on time, with the chord on screen before its sound;
- the store, with power cuts and damaged files;
- flash ids 856014 / b36014 / unknown;
- settings over a restart;
- every M3 part check that still applies, adapted to M4's controls (layer presses, SEQ cycle);
- M4 parts 1–5, including the 5 private songs on a flash of their own.

Laptop-only tests:
- `tools/song_fuzz.sh 600` (formats 1.3, 1.1, 1.0): 968,000 files, no sanitizer report;
- pytest: 78 pass (the compiler, formats 1.0–1.3, the importer);
- `tools/store_test.sh`: ALL PASS;
- `tools/guitar_check.py`: all pass.

Part builds (`build/step-M4/partN/`):

| Part | Build | Identity | Checks then |
|---|---|---|---|
| 1 quick fixes | 0.4.1-m4p1 | FM-1_46000401 | m4p1, part3: all pass |
| 2 your part, volumes | 0.4.2-m4p2 | FM-1_46000402 | m4p1, m4p2, views, part4, a full song: 42 pass |
| 3 views | 0.4.3-m4p3 | FM-1_46000403 | m4p2, m4p3, part7: 21 pass |
| 4 five levels | 0.4.4-m4p4 | FM-1_46000404 | **the full suite: 246 checks pass** (early install point) |
| 5 import (laptop) | — (no firmware change) | — | m4p5 on 0.4.4: 11 pass |

## Highlights (measured)

| What | Result |
|---|---|
| Whole-song loop | Happy Birthday goes round from its pickup (24480 → 960), never ends |
| Volumes (D027) | the harmony alone: FULL rms 698, QUIET 50 % 337, QUIET 25 % 177, MUTED 0 |
| Your part | harmony by default, lit unmuted; a held bass button: bass yours, lit, playing |
| Notation | harmony grand staff and drum notation; the Blues at 250 BPM, harmony L2, on time, CPU ≤ 16 % |
| Five levels | every song: harmony 5, bass 4, drums 3; L5 spans 19 semitones; L5 key flashes = notes played |
| Busiest | Oh! Susanna, 250 BPM, harmony L5, bass L4, drums L3: on time, 15 voices, CPU 15 % |
| Older formats | a 1.2 copy plays (3 levels); fm1song send to M3's own flash: 1.2, stored |
| Imported songs | 5 stored; each to the end at the top levels; ≤ 14 voices, CPU ≤ 14 % |

## Budget

| | M3 | M4 | Room |
|---|---|---|---|
| App image (slot ~581 KB) | 414,272 B | **419,056 B** | ~162 KB |
| RAM .data+.bss (of 98,304) | 74,740 | **74,836** | 23 KB |
| RAM pool (of 344,064) | 215,304 | **284,936** (the decode buffer 128 KiB, for 12 levels) | 58 KB |
| Song store (69 sectors, ~276 KB) | 9 songs, 24 sectors | 9 songs (1.3), **168 KB in 45 sectors**; the 5 private songs 221 KB (not all beside them) | 24 sectors |
| Events a song (decoded together) | 8192 max | **16384 max**; ours up to 8526 (Oh! Susanna) | |
| CPU / voices (worst) | 15 % / 14 | 16 % / 15 | |

## Not covered by the emulator

- The sound and feel: the levels by ear, the quiet level, the beep at MEDIUM.
- The real LEDs: the keys' middle level for L4/L5's chord around the note.
- The hold timing (900 ms) on the real buttons.
- Whether the imported charts are right, and the notation as a staff reader would read it.
