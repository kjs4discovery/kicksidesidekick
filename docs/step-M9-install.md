# Milestone M9: install on the FM-1

**Status: PREPARED, waiting for review** by the planning chat and the user (docs/plan-M9.md, Part 7). Nothing has
been installed or sent. Claude Code ran no device command while preparing it. **The user runs every device command**
(the install, the deletes, the sends), one at a time, in their own terminal. Claude Code runs only the read-only ones
(`--info`, `fm1song list`) between steps and records them.

Prepared 2026-11-13 by Claude Code. Rules, pre-flight and restore paths as the approved M8 checklist
([step-M8-install.md](step-M8-install.md)); what differs is here. M9 had no early install point.

---

## What is new (docs/controls.md, "M9")

- **BACKTRACK has one knob map**, on SETUP and on the play screen: **K1** progression (now 36), **K2** style, **K3**
  complexity (SIMPLE / MEDIUM / BUSY, default MEDIUM), **K4** melody sound, **PRESETS** the key, **ALGO** the structure
  (LOOP / VERSE/CHORUS / FULL), **SELECT** the tempo. On the play screen K4 and SELECT act at once; the key and the
  complexity at the next bar line; the progression, style and structure at the next phrase boundary (the top line says
  `NEXT: ...`). The switch is seamless: no gap, no count-in, the music goes on in the same place of the form.
- **The band's tiers**: in BACKTRACK the style sets the rhythm of the drums, bass and harmony together at every
  complexity (4 tiers per layer, tools/band_styles.txt; a chorus plays one tier above the verse). A REGGAE backtrack now
  sounds like reggae at the simplest setting.
- **New progressions**: FIESTA, HEROIC and BITTERSWEET replace LA BAMBA, MARIO and CREEP; 31 MINOR FOLK, 32 EPIC,
  33 AEOLIAN, 34 HARMONIC, 35 MINOR 2-5-1, 36 MINOR FUNK. The setup shows a MAJOR or MINOR badge.
- **Fixes**: the count-in no longer starts in the middle of a song (after a loop wrap, or a pause and PLAY) in either
  mode; "All songs" counts songs only (not the backtracks); the mode screen says "modes:" and "Play with a song"; the
  LFO light is on in BACKTRACK; the MIDI importer reads a file's time signature (3/4, 6/8) and key signature.
- **Removed from BACKTRACK**: K1-K3 as per-layer level knobs, SEQ's loops, the start point (PRESETS / ALGO as in SONG
  LEARN) and SAVE's key mode: SEQ and SAVE show the "not implemented" pop-up there. SONG LEARN is unchanged.

## What changes on the unit

| | Now | After |
|---|---|---|
| Firmware | M8 **FM-1_46000800**, v0.8.0-m8 | **FM-1_46000900**, v0.9.0-m9 |
| Step 1 writes | — | The app area 0x4000–0x92FFF and the update staging area 0xE0000–0xE4FFF, by our update loader, as every install before. fm1-base (incl. the loader) unchanged since 98fc08c: `git diff 98fc08c -- firmware/vendor/fm1-base` is empty. |
| Your 13 songs | stored | **Untouched** (the importer fix changes no song: Creep, True Colors, Let It Be, Zombie, Rockin' re-imported: identical). |
| The 30 backtracks of M8 | stored (3 / 4 / 5 levels a layer) | **M9 refuses them** ("BACKTRACK ERROR ... LAYERS" on opening): a backtrack now has 4 tiers a layer. Step 2 replaces them. |
| Step 2 writes (the song store 0x97000–0xDBFFF only) | — | `delete` of bt06-la-bamba, bt13-mario, bt14-creep (the records are marked deleted; nothing is erased now), then **36 backtracks sent** (the 27 same slugs replaced, 9 new). Only after step 1. |
| Settings | your M8 settings (40 bytes) | Kept: the same record. (The complexity and the other BACKTRACK choices last until power-off, as in M8.) |
| 0xDE000–0xDFFFF | unused | unused (kept free for M9.1's user presets) |

## Pre-flight (any ❌ = stop)

| # | Check | State | Evidence |
|---|---|---|---|
| P1 | Off-laptop backup in `docs/device.md` | ✅ | Unchanged. |
| P2 | USB port + cable | ✅ | Port **1-2**, the new cable. A `-110` beyond the usual one = stop and report. |
| P3 | Restore per D015 | ✅ | `firmware/vendor/fm1-base` unchanged since 98fc08c. |
| P4 | The package: commit + sha256 + emulator | ✅ | `build/step-M9/fm1acc-0.9.0-m9.fwsc`, sha256 `8a1c341d28eff7ba112463f8e633bc6828a4e703f7fa1e21313fdc8bcde3cf91`, identity FM-1_46000900, built from commit 03ffd12, the same sha256 in two builds. Tested as the user directed (decision M9-21: only the new milestone's tests): see **Test results** below. |
| P5 | Flash IDs | ✅ | In the M5 suite on this package: 0x856014 and 0xB36014 the same; an unknown one: `NO SONG STORAGE`, a send refused, nothing written. |
| P6 | Unit state before step 1 | (check) | `--info`: `FM-1_46000800  [running]`. In SAFE MODE: press PLAY ("try again") or power off/on first. |
| P7 | Fallback package | ✅ | M8, known good on this unit: `build/step-M8/fm1acc-0.8.0-m8.fwsc`, sha256 `19e054002de21a3a204611b553fad6997a944db2f3fa83c08da651725e91665f` (the 36 M9 backtracks then show as errors in M8's BACKTRACK: nothing else is affected). Behind it M7: `build/step-M7/fm1acc-0.7.0-m7.fwsc`, sha256 `8478a955eb29c1de7474274a94ff6d96e4f7e2b0bfb8d98580012764bbfdb512`. |
| P8 | The songs | ✅ | Ours unchanged. The backtracks: 36 x 2.2, compiled; every one in all 11 styles at all 4 tiers: the FM-1's generators = the laptop's (pytest, 36 x 11 x 3 layers x 4 tiers). |

## Budget

| | 0.8.0-m8 | 0.9.0-m9 |
|---|---|---|
| App image (slot 585,728 B) | 493,312 B | 507,440 B: **78,288 B free** (M9.1 needs >= 32 KB) |
| RAM .data+.bss (of 98,304) | 81,416 | 82,456 |
| RAM pool (of 344,064) | 291,464 | 291,464 |
| **BACKTRACK CPU, the worst case** (emulated) | 37 % | **44 %** (limit 60 %; every style 38-44 %): complexity 3 (tier 4 in a chorus), FULL, every layer on, MIAMI PAD x 4 voices with its effects, a seamless switch made in the run, 250 BPM |
| The song buffer in BACKTRACK | one version in 112,896 B (largest 54,032 B) | **two versions**, 56,448 B each (the largest 54,448 B: bt28 in COUNTRY), the synth's lines 83,712 B as before |
| Making a version (the largest) | — | about 47 ms in the emulator (open + restyle + 3 levels), the audio keeps its blocks (m9p4: no block over its time) |
| The song store (552 blocks) | 13 songs + 30 backtracks: 264 | 13 songs (172) + 36 backtracks (110): **282 blocks**, 270 free |
| Longest stretch without a watchdog feed (timed emulator) | 92 ms | not re-measured (m51 not run for M9, M9-21); no flash code changed |

## Test results (the full suite, once, on this package)

Only M9's own tests were run (the user's instruction, decisions-M9.md M9-21). **The m5, m6, m7, m8 and m51 suites were NOT run
on this package**: nothing here claims SONG LEARN, SYNTH or the store passed their older suites on it (earlier in M9 the Part 1
fast checks m8p4/m8p6, m5p3 and m6p1 passed on a dev build; m6p1 only its known Zombie failure).

| Item | Result |
|---|---|
| store test (simulated flash, power cut at every operation) | rc 0 |
| pytest (tools/fm1song) | **203 passed**: the 13 songs' golden tests unchanged; the 36 backtracks x 11 styles x 4 tiers C = Python; the table validator; the importer |
| vsynth, guitar / ukulele | rc 0 |
| song_fuzz 60 s | clean (14,000 files in the final run, no sanitizer report) |
| m9 (tools/m9_test.py, build/step-M9/emu-m9/report.md) | m9p1 count-in 6/6, m9p2 4/4, m9p4 25/25, m9census 4/4, m9cpu 2/2 (worst 44 % of a block), m9sweep: 384 of 396 runs passed in the first run; the 12 that failed (WALTZ, FULL) were a test timing mistake (a run timed for 3-beat bars after the live style change made it 4/4); fixed in the test and those 12 re-run on the same package: all pass (worst 28 %) |

Screenshots: docs/step-M9/.

## Known

- **The tier tables are drafts** (the planning chat's): tools/band_styles.txt, written from genre conventions, **not heard
  on the unit**. The user prunes by ear: edit the file, run `tools/band_styles.py`, `tools/backtracks.py`, rebuild and
  send. Listen to every style at the three complexities and say which to change.
- **Names**: the new ones (FIESTA, HEROIC, BITTERSWEET, 31-36) and the old drafts are the user's to decide; CANON is the
  name of a piece (docs/decisions-M9.md, M9-13).
- **Sending order**: the 30 backtracks from M8 stay in the store until replaced and cannot be opened ("LAYERS"); do step 2
  right after step 1.
- **A change made while stopped or paused** takes effect at once and stops the band (PLAY then starts from the loop's start);
  while playing it waits for its boundary.
- **Zombie drums L1** about 1.68x quieter than L2/L3: as M7, M8 (backlog), left failing, not relaxed.
- **Decisions for review**: docs/decisions-M9.md, M9-01 to M9-20 (every judgment call the plan did not spell out).
- **Sends**: only in SONG LEARN (BACKTRACK and SYNTH refuse: the synth holds part of the song buffer).
- **An M8 FM-1 (fallback)**: M8 cannot play the M9 backtracks (they have 4 tiers a layer): its BACKTRACK shows errors;
  SONG LEARN and the songs are not affected.

---

## Checklist

### Step 1 — Install 0.9.0-m9

1. Claude Code runs `--info` (read-only) → `FM-1_46000800  [running]`.
2. Check the package:
   ```bash
   sha256sum build/step-M9/fm1acc-0.9.0-m9.fwsc
   ```
   expect `8a1c341d28eff7ba112463f8e633bc6828a4e703f7fa1e21313fdc8bcde3cf91`.
3. Install (you run it; 1–2 minutes):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M9/fm1acc-0.9.0-m9.fwsc
   ```
4. Claude Code runs `--info` → `FM-1_46000900  [running]`. The splash shows `Notami - SongSidekick` and `m9`, then the
   mode screen with "modes:" above three choices; SONG LEARN says "Play with a song".
5. SONG LEARN: your 13 songs are listed (Claude Code may run `tools/fm1song.sh list`, read-only: 13 songs + the 30 old
   backtracks; the backtracks `ok` for now).

If it fails: install M8 (P7) and report.

### Step 2 — The backtracks (writes the song store; only after step 1, in SONG LEARN)

Switch the FM-1 off and on and choose **SONG LEARN** first (BACKTRACK and SYNTH refuse sends).

1. Delete the three renamed backtracks (each marks its record deleted; nothing is erased; to undo: send the M8 folder
   again from git history):
   ```bash
   tools/fm1song.sh delete bt06-la-bamba
   tools/fm1song.sh delete bt13-mario
   tools/fm1song.sh delete bt14-creep
   ```
   Three lines `deleted`.
2. Send the 36 backtracks (the 27 with the same slugs replace the old ones; 9 are new; nothing else is touched):
   ```bash
   tools/fm1song.sh send songs/backtracks/*/
   ```
   36 lines `stored` (the store may answer "making room" once: send again as M5.1 taught).
3. Claude Code runs `fm1song list` → 13 + 36 = 49 entries, each `ok`. (If your 13 songs include a private song that the
   importer fix changed, resend it: none changed.)

### Step 3 — SONG LEARN (a minute)

1. The list shows your 13 songs and no backtrack; "All songs" says 13 (HOME: the folders).
2. Open a song, PLAY with the count-in on: it counts in once. Set a loop (SEQ) with the start point later in the song, or
   pause in the loop and PLAY again: **no count-in at the loop's wrap**.

### Step 4 — BACKTRACK (a few minutes)

1. Hold HOME 2 s → the mode screen → SELECT to **BACKTRACK**, PLAY: SETUP shows the seven rows: 1 POP 1 with a **MAJOR**
   badge (try K1 to 15 MINOR POP: **MINOR**), POP, MEDIUM, a melody sound, the key, LOOP, 100 BPM.
2. PLAY. Turn **K3**: BUSY then SIMPLE: it changes at the next bar line; the layers row shows the tier (L1-L4; a chorus one
   more). Turn **K2** to **REGGAE**: the top line says `NEXT: REGGAE`; at the next phrase it switches without a gap or a
   count-in, in the same bar. Do the same with **K1** (another progression), **ALGO** (FULL: the loop ends; back to LOOP while
   in a chorus: it jumps to the loop at the boundary) and **PRESETS** (the key, at the next bar line). **K4**: another melody
   sound at once.
3. **SEQ** and **SAVE** show the "not implemented" pop-up. Hold **LFO**: the melody is yours (its light is on).
4. HOME, HOME: SETUP: try WALTZ (3/4), 20 12-BAR BLUES, 31-36, FULL at MEDIUM and at BUSY (a chorus is the busiest).
5. Which styles at which complexity sound right, which do not? Is any switch audible as a gap or a click?

### Step 5 — SYNTH, then power off and on

SYNTH as before (preset 1 E-PIANO). Power off and on: the splash, the mode screen (cursor on SONG LEARN); BACKTRACK back
at 1 POP 1, MEDIUM; SONG LEARN: your settings kept.

## If anything goes wrong

- The install fails, or the app misbehaves: install M8 (P7); your songs and settings stay:
  ```bash
  .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M8/fm1acc-0.8.0-m8.fwsc
  ```
- A send fails ("disconnected", "stopped answering"): stop, don't retry; Claude Code runs `--info` and reads the
  crash log (read-only). The store recovery (Settings > Wipe song store, the last row, or `tools/fm1song.sh wipe --yes`)
  is as at M5.1.

## Restore paths (D015, unchanged)

As [step-M3-install.md](step-M3-install.md).

## After the user has run it

Claude Code records the outputs, identities, `fm1song list` output and impressions in `docs/LOG.md`, and the firmware in
`docs/device.md`.
