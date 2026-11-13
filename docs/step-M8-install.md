# Milestone M8: install on the FM-1

**Status: PREPARED, waiting for review** by the planning chat and the user (docs/plan-M8.md, Part 7). Nothing has
been installed or sent. Claude Code ran no device command while preparing it. **The user runs every device command**
(the install, the sends), one at a time, in their own terminal. Claude Code runs only the read-only ones (`--info`,
`fm1song list`) between steps and records them.

Prepared 2026-11-13 by Claude Code. Rules, pre-flight and restore paths as the approved M7 checklist
([step-M7-install.md](step-M7-install.md)); what differs is here. M8 had no early install point.

---

## What is new (docs/controls.md, "M8")

- **The mode screen**: SONG LEARN, BACKTRACK, SYNTH (the cursor on SONG LEARN). LEARN is called SONG LEARN.
- **The splash**: "Notami - SongSidekick", under it "m8".
- **BACKTRACK** (new): a band to jam over. SETUP: K1 one of 30 chord progressions, K2 one of 11 styles, K3 LOOP /
  VERSE/CHORUS / FULL, K4 the key, SELECT the tempo, PLAY. Then SONG LEARN's play screen: K1-K3 the drums', bass's,
  harmony's level, K4 the melody sound (the 30 SYNTH presets, 4 voices), your keys play it; the screen shows what fits
  now: SCALE KEYBOARD / SCALE FRETBOARD (the scale, the chord's tones, the next chord's in its last beat), and the key
  lights do the same. Settings > Scale guide: PENTATONIC / FULL SCALE / CHORD TONES.
- **Five new styles** in the generators (ROCK, INDIE POP, BOSSA NOVA, FUNK, REGGAE), song format **2.2** (backtracks,
  a section's energy: the chorus one level busier).
- **SYNTH**: E-PIANO, E-GUITAR, E-SITAR are presets 1-3; SYNTH starts on E-PIANO.
- Two titles: "12-Bar Blues", "Four Chord Tune".

## What changes on the unit

| | Now | After |
|---|---|---|
| Firmware | M7 **FM-1_46000700**, v0.7.0-m7 | **FM-1_46000800**, v0.8.0-m8 |
| Step 1 writes | — | The app area 0x4000–0x92FFF and the update staging area 0xE0000–0xE4FFF, by our update loader, as every install before. fm1-base (incl. the loader) unchanged since 98fc08c: `git diff 98fc08c -- firmware/vendor/fm1-base` is empty. |
| Your 13 songs | stored, 2.0 / 2.1 | **Kept and read as they are** (M8 reads 2.0, 2.1 and 2.2). |
| Step 2 writes (the songs) | — | The song store 0x97000–0xDBFFF only: the 30 backtracks **added** (92 blocks; the store then 264 of 552 blocks), twelve-bar-blues and four-chords **sent again** (their new titles; each replaces its copy). Only after step 1: M7 refuses 2.2 and `fm1song send` stops before sending anything to an M7 FM-1. |
| Settings | your M7 settings (40 bytes) | Kept: the same record; the new row "Scale guide" starts at PENTATONIC; "Wipe song store" is still the last row. |
| 0xDE000–0xDFFFF | unused | unused (still kept free for M7.1's user presets) |

## Pre-flight (any ❌ = stop)

| # | Check | State | Evidence |
|---|---|---|---|
| P1 | Off-laptop backup in `docs/device.md` | ✅ | Unchanged. |
| P2 | USB port + cable | ✅ | Port **1-2**, the new cable. A `-110` beyond the usual one = stop and report. |
| P3 | Restore per D015 | ✅ | `firmware/vendor/fm1-base` unchanged since 98fc08c. |
| P4 | The package: commit + sha256 + emulator | ✅ | `build/step-M8/fm1acc-0.8.0-m8.fwsc`, sha256 `19e054002de21a3a204611b553fad6997a944db2f3fa83c08da651725e91665f`, identity FM-1_46000800, built from commit 13c759e (two builds, the same sha256). The full suite ran once on this package (test-runner): store ALL PASS; song_fuzz 600 s, 822,000 files clean (2.2 seeds, a random restyle of every accepted file); pytest 153 passed (the 13 songs' golden events unchanged; the 5 new styles and all 30 backtracks C = Python; the FM-1's restyle = a recompile in all 11 styles); vsynth ALL PASS; guitar / ukulele all chords; **m8 42 PASS** (renames, BACKTRACK, the scale views, the melody sound, the census, 99 style x structure x progression runs, CPU); m7 53 PASS; m6 21 PASS / 1 FAIL (Zombie drums, *Known*); m51 (the timed emulator, `make m51-test`'s test on this package) 17 PASS; m5 283 PASS / 1 FAIL (a stale expectation, fixed and re-run on this package: ALL PASS, *Known*). Watchdog: longest stretch 92 ms, 0 resets, 0 XIP-off reads. |
| P5 | Flash IDs | ✅ | In the M5 suite on this package: 0x856014 and 0xB36014 the same; an unknown one: `NO SONG STORAGE`, a send refused, nothing written. |
| P6 | Unit state before step 1 | (check) | `--info`: `FM-1_46000700  [running]`. In SAFE MODE: press PLAY ("try again") or power off/on first. |
| P7 | Fallback package | ✅ | M7, known good on this unit: `build/step-M7/fm1acc-0.7.0-m7.fwsc`, sha256 `8478a955eb29c1de7474274a94ff6d96e4f7e2b0bfb8d98580012764bbfdb512` (reads the same settings; the backtracks (2.2) then show as unreadable in its list: delete them or leave them, nothing else is affected). Behind it M6: `build/step-M6/fm1acc-0.6.0-m6.fwsc`, sha256 `8944fb02a725978306754601e053e7c1f8b96f0280e13deed23201335888abec`. |
| P8 | The songs | ✅ | Ours unchanged in content (2.0, the golden tests unchanged; two titles new). The backtracks: 30 x 2.2, compiled and played in the emulator. |

## Budget

| | 0.7.0-m7 | 0.8.0-m8 |
|---|---|---|
| App image (slot 585,728 B) | 479,968 B | 493,312 B: **92,416 B free** (M7.1 needs >= 32 KB) |
| RAM .data+.bss (of 98,304) | 81,128 | 81,416 |
| RAM pool (of 344,064) | 289,864 | 291,464 (+1,600: the song list's backtrack fields) |
| SYNTH CPU, the busiest case | 44 % of a block | unchanged code (m7's budget runs pass) |
| **BACKTRACK CPU, the busiest case** (emulated) | — | **37 %** (limit 60 %): INDIE POP, FULL, drums L3 / bass L4 / harmony L5, MIAMI PAD (the heaviest preset, as made, every effect it has) on 4 voices held, 200 BPM; EAS 15 %. Every style 26-37 %. The 99-run sweep at 250 BPM: worst 26 %. |
| The song buffer (192 KiB) in BACKTRACK | — | the backtrack's levels up to 112,896 B (the largest in any style: 54,032 B, bt28 in FUNK), the synth's lines 83,712 B (the delay's maximum halved: 0.37 s) |
| The song store (552 blocks) | 13 songs, 172 blocks | + 30 backtracks (92 blocks): **264 blocks**, 288 free |
| Longest stretch without a watchdog feed (timed emulator) | 92 ms | 92 ms (0 resets) |

## Known

- **Two stale test expectations / known failures** (the firmware unchanged, the package the one tested): m5's
  "a song sent in SYNTH mode" looked for the laptop's message "SYNTH mode"; it now reads "the FM-1 is in SYNTH or
  BACKTRACK mode" (BACKTRACK refuses sends too). Fixed to the exact new text, re-run on this package: m5p3 ALL PASS.
  **Zombie drums L1** about 1.68x quieter than L2/L3: as M7 (backlog), left failing, not relaxed.
- **The styles, grooves, bass lines and the 30 progressions' forms were written from genre conventions**, not heard on
  the unit (tools/backtracks.tsv, the tables in gen.c / arrange.py): listen and say which to change. The names of 10,
  12, 16, 18 (FOLK ROCK, CLASSIC ROCK, MINOR ROCK, MINOR SOUL) are mine (drafts).
- **Energy** applies only to generated layers (a hand-made or imported level is left as it is).
- **INTRO**: plays at the levels chosen (L1 at the start), energy 0; there is no "lower than L1".
- **The melody sound** plays the presets with their arpeggio off (notes, not arpeggios); the delay's longest time is
  0.37 s in BACKTRACK (a longer note value is halved, as in M7 when it does not fit).
- **The mode screen's cursor** goes back to SONG LEARN whenever it opens (M7 put it on "the other" mode).
- **Sends**: only in SONG LEARN (BACKTRACK and SYNTH refuse: the synth holds part of the song buffer).
- **An M7 FM-1 (fallback) and 2.2**: going back to M7 leaves the backtracks in the store, unreadable to M7 (shown as
  errors); the songs are not affected.

---

## Checklist

### Step 1 — Install 0.8.0-m8

1. Claude Code runs `--info` (read-only) → `FM-1_46000700  [running]`.
2. Check the package:
   ```bash
   sha256sum build/step-M8/fm1acc-0.8.0-m8.fwsc
   ```
   expect `19e054002de21a3a204611b553fad6997a944db2f3fa83c08da651725e91665f`.
3. Install (you run it; 1–2 minutes):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M8/fm1acc-0.8.0-m8.fwsc
   ```
4. Claude Code runs `--info` → `FM-1_46000800  [running]`. The splash shows `Notami - SongSidekick` and `m8`, then
   the mode screen with three choices.
5. SONG LEARN: your 13 songs are listed (Claude Code may run `tools/fm1song.sh list`, read-only: 13 songs, each `ok`).

If it fails: install M7 (P7) and report.

### Step 2 — The backtracks, and two songs again (writes the song store; only after step 1, in SONG LEARN)

Switch the FM-1 off and on and choose **SONG LEARN** first (BACKTRACK and SYNTH refuse sends).

1. Send the 30 backtracks (adds them; nothing else is touched; to undo: `tools/fm1song.sh delete <slug>` each, or
   leave them):
   ```bash
   tools/fm1song.sh send songs/backtracks/*/
   ```
   30 lines `stored`.
2. Send the two renamed songs (each replaces its copy):
   ```bash
   tools/fm1song.sh send songs/twelve-bar-blues/ songs/four-chords/
   ```
   Two lines `stored`.
3. Claude Code runs `fm1song list` → 43 entries, each `ok`.

### Step 3 — SONG LEARN (a minute)

1. The list shows your 13 songs and no backtrack; "12-Bar Blues" and "Four Chord Tune" with their new names.
2. Open any song: it plays as before.

### Step 4 — BACKTRACK (a few minutes)

1. Hold HOME 2 s → the mode screen → SELECT to **BACKTRACK**, PLAY: SETUP shows 1 POP 1, POP, LOOP, C, 100 BPM.
2. PLAY: the band plays the verse in a loop; the screen shows the SCALE KEYBOARD (the chord's notes bright). Play
   along on the keys (the melody sound, E-PIANO). Do the FM-1's key lights follow the chords?
3. Turn **K4**: other melody sounds (the name shows). **EDIT**: the SCALE FRETBOARD. **K1-K3**: the drums, bass,
   harmony busier / simpler.
4. HOME, HOME: back to SETUP. Try: K2 **FUNK** or **BOSSA NOVA**, K3 **FULL** (intro to ending; the choruses a level
   busier), K4 another key, PLAY. Try **WALTZ** (3/4) and **20 12-BAR BLUES** (the blues scale).
5. GLO, GLO, Settings > **Scale guide**: FULL SCALE, then CHORD TONES: the marking changes. Back.
6. Which styles and progressions sound right, which do not? (The grooves and lines are written from genre
   conventions, not heard on the unit.)

### Step 5 — SYNTH

1. Hold HOME 2 s → SYNTH: the preset is **E-PIANO**; PRESET: 2 E-GUITAR, 3 E-SITAR, 4 NEON PAD ...

### Step 6 — Power off and on

The splash, the mode screen (cursor on SONG LEARN); BACKTRACK: back at 1 POP 1 (its choices last until power-off);
SONG LEARN: your settings kept.

## If anything goes wrong

- The install fails, or the app misbehaves: install M7 (P7); your songs and settings stay:
  ```bash
  .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M7/fm1acc-0.7.0-m7.fwsc
  ```
- A send fails ("disconnected", "stopped answering"): stop, don't retry; Claude Code runs `--info` and reads the
  crash log (read-only). The store recovery (Settings > Wipe song store, the last row, or
  `tools/fm1song.sh wipe --yes`) is as at M5.1.

## Restore paths (D015, unchanged)

As [step-M3-install.md](step-M3-install.md).

## After the user has run it

Claude Code records the outputs, identities, `fm1song list` output and impressions in `docs/LOG.md`, and the
firmware in `docs/device.md`.
