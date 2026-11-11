# Milestone M7: install on the FM-1

**Status: PREPARED, waiting for review** by the planning chat and the user (docs/plan-M7.md, Part 6). Nothing has
been installed, sent or deleted. Claude Code ran no device command while preparing it. **The user runs every device
command** (the install, the delete, the send), one at a time, in their own terminal. Claude Code runs only the
read-only ones (`--info`, `fm1song list`) between steps and records them.

Prepared 2026-11-11 by Claude Code. Rules, pre-flight and restore paths as the approved M5.1 / M6 checklists
([step-M6-install.md](step-M6-install.md)); what differs is here. M7 had no early install point.

---

## What is new (docs/controls.md, "M7")

- **Learning:** a button with no job shows **NOT IMPLEMENTED** (any button closes it). Songs can open on another
  level than 1 (format 2.1): Zombie's bass opens on L2. Zombie's lead a little quieter. Rockin' removed.
- **SYNTH, rebuilt:** PRESET the sound (30 presets, ANALOG and the new PHASE engine), SELECT the tempo, K1..K4 quick
  (cutoff, resonance, reverb, delay), EDIT / ALGO through 9 edit pages (ENVELOPE, FILTER, SOUND A, SOUND B, ARP,
  FX 1-4), ENV / FX jump there, ARP on / off, GLO help then settings (drum volume, drum kit). The EDIT-held SHIFT
  layer is gone.
- **Effects:** new tremolo, phaser, fuzz (with chorus, delay, reverb, bit-crush), each preset its own mix.
- **Drums:** our own step patterns (25: ROCK1 default; SEQ the menu, PLAY on / off, a change on the next bar), three
  kits: ACOUSTIC (the GM kit), 808, CHIP. M6's drum jam is gone.

## What changes on the unit

| | Now | After |
|---|---|---|
| Firmware | M6 **FM-1_46000600**, v0.6.0-m6 | **FM-1_46000700**, v0.7.0-m7 |
| Step 1 writes | — | The app area 0x4000–0x92FFF and the update staging area 0xE0000–0xE4FFF, by our update loader, as every install before. fm1-base (incl. the loader) unchanged since 98fc08c: `git diff 98fc08c -- firmware/vendor/fm1-base` is empty. |
| Your 14 songs | stored, format 2.0 | **Kept and read as they are** (M7 reads 2.0 and 2.1). |
| Step 2 writes (the songs) | — | The song store 0x97000–0xDBFFF only: Rockin' **deleted** (its record's magic set to zero: nothing erased), Zombie **sent again** as 2.1 (its bass on L2, its lead at 88). Only after step 1: M6 refuses a 2.1 file (and `fm1song send` would send M6 the 2.0 form). |
| Settings | your M6 settings (36 bytes) | Kept: the record grows to 40 bytes; an M6 record is read, SYNTH's new settings start at their defaults (drum volume 7 of 10, kit ACOUSTIC). |
| 0xDE000–0xDFFFF | unused | unused (kept free for M7.1's user presets; nothing in M7 touches it: checked in code and in 1,207 emulator flash images) |

## Pre-flight (any ❌ = stop)

| # | Check | State | Evidence |
|---|---|---|---|
| P1 | Off-laptop backup in `docs/device.md` | ✅ | Unchanged. |
| P2 | USB port + cable | ✅ | Port **1-2**, the new cable. A `-110` beyond the usual one = stop and report. |
| P3 | Restore per D015 | ✅ | `firmware/vendor/fm1-base` unchanged since 98fc08c. |
| P4 | The package: commit + sha256 + emulator | ✅ | `build/step-M7/fm1acc-0.7.0-m7.fwsc`, sha256 `8478a955eb29c1de7474274a94ff6d96e4f7e2b0bfb8d98580012764bbfdb512`, identity FM-1_46000700, built from commit d1140ba (two builds, the same sha256). The full suite ran once on this package: store ALL PASS; pytest 95 (after one stale test expectation was fixed, see *Known*); vsynth ALL PASS (30 presets, every effect, every drum sound); guitar / ukulele all chords; the fuzz 600 s, 2,452,000 files clean; m7 52 PASS / 1 FAIL (preset loudness in the emulator, *Known*); m6 21 PASS / 1 FAIL (Zombie drums, *Known*); m51 (the timed emulator, as `make m51-test` but on this package) 16 PASS / 1 FAIL and m5 283 PASS / 1 FAIL, both stale expectations, fixed and re-run on this package: **ALL PASS**. Watchdog: longest stretch 92 ms, 0 resets, 0 XIP-off reads. |
| P5 | Flash IDs | ✅ | In the M5 suite on this package: 0x856014 and 0xB36014 the same; an unknown 0xC84017: `NO SONG STORAGE`, a send refused, nothing written; SYNTH plays. |
| P6 | Unit state before step 1 | (check) | `--info`: `FM-1_46000600  [running]`. In SAFE MODE: press PLAY ("try again") or power off/on first. |
| P7 | Fallback package | ✅ | M6, known good on this unit: `build/step-M6/fm1acc-0.6.0-m6.fwsc`, sha256 `8944fb02a725978306754601e053e7c1f8b96f0280e13deed23201335888abec` (reads the same settings; Zombie 2.1 it refuses: send Zombie again after going back, `fm1song send` then sends 2.0). Behind it M5.1: `build/step-M5.1/fm1acc-0.5.4-m5p4.fwsc`, sha256 `197b84a552100969ee727d451d018cd83eeed7a0e2bdb744740dd84e1f0a187d`. |
| P8 | The songs | ✅ | Ours unchanged (2.0, byte for byte: the golden tests unchanged). The private ones: Rockin' removed here; Zombie 2.1 (compiled and played in the emulator: bass on L2). |

## Budget

| | 0.6.0-m6 | 0.7.0-m7 |
|---|---|---|
| App image (slot 585,728 B) | 460,496 B | 479,968 B: **105,760 B free** (M7.1 needs >= 32 KB) |
| RAM .data+.bss (of 98,304) | 78,888 | 81,128 |
| RAM pool (of 344,064) | 289,864 | 289,864 |
| SYNTH CPU, the busiest case (emulated) | 8 voices SYNC SCREAM, chorus + delay + reverb 127 + the drum jam L3: 34 % of a block | **44 %** (limit 60 %): 8 voices of the heaviest preset (MIAMI PAD, PHASE), every effect at its top, the busiest pattern (SYNTHWAVE2) on the 808 kit; ACOUSTIC 35 %, CHIP 42 %. Nothing dropped. |
| The song buffer in SYNTH (192 KiB) | delay + chorus + reverb 149,248 B, the jam after it | the same 149,248 B (the drums need none now) |
| Longest stretch without a watchdog feed (timed emulator; limit 8,000 ms) | 67 ms | 92 ms (m51, every send scenario; 0 resets) |

## Known

- **The synth UI code is bigger than M6's** (8,320 B vs 6,464 B, measured the same way): the plan's aim "not bigger"
  is missed; it has five screens now (main, 9 pages, pattern menu, help, settings) instead of one.
- **LEVEL is logarithmic** (FX 4: 100 = as made, half a dB a step, 127 = x4.8): a linear one could not lift the
  arpeggio presets to the chords'. The 30 presets are matched within 0.3 dB (laptop), 1.4x in the emulator.
- **Zombie's lead** measured at the public songs' median before the change (melody / harmony + bass: rms 1.05,
  A-weighted 2.80; the public median 1.23 / 2.90); it was lowered to their lower quartile (volume 88: 0.90 / 2.40) on
  the user's ear. Please say whether it is right now.
- **Zombie drums L1** about 1.68x quieter than L2/L3 (as M6): accepted for now (backlog); M6's loudness check still
  fails on that one row (not relaxed).
- **Four checks the final suite found stale** (the firmware unchanged, the package is the one tested): they expected
  every private song to be format 2.0 (pytest's generator golden test, m5's m4p5: Zombie is 2.1 now, its generated
  levels the same in C and Python) and m51's incident store to end with 13 songs (it began with M4's Rockin', which
  that scenario never deletes: 14, all ok; on the unit step 2 deletes it). Each fixed to the exact new expectation
  and re-run on this package: pytest 95 passed, m4p5 9 PASS, m51 sends ALL PASS.
- **Preset loudness in the emulator: left failing (not relaxed).** The laptop matching puts all 30 within 0.3 dB (the
  loudest 100 ms of a chord, 2 s). The emulator's own check (the same measure, 4 keys pressed, limit 1.6x) finds 1.78x
  even with the chord held as long: DUSK STRINGS +2.3 dB, GLASS PLUCK -2.7 dB, the rest within +-1.5 dB. The two
  differ by the keys' scan (four notes not quite together lower a pluck's summed peak) and the PWM / chorus phase in
  the 100 ms window. Changing levels would have changed the package after its one full run. Listen: if DUSK STRINGS
  is loud or GLASS PLUCK quiet, their LEVEL (FX 4) is the knob; M7.1 can save it.
- **m6p3 retired** (not run): it tested M6's SHIFT layer and drum jam, both replaced in M7 (D039, D041); the same
  ground is covered by m7p2–m7p6 and vsynth_test.
- **Presets renamed to fit 12 characters:** MIDNIGHT STRINGS → DUSK STRINGS, OCTAVE BOUNCE → OCT BOUNCE, SOFT SAW
  STRINGS → SAW STRINGS, NIGHT DRIVE SUB → NIGHT DRIVE. POTATO, PARKER and EDS are first drafts: rename or re-voice
  them freely.
- The drum patterns are written from genre conventions, not heard on the unit: listen and prune (docs/controls.md lists
  them as grids; docs/drum-patterns.txt is the one file to edit).

---

## Checklist

### Step 1 — Install 0.7.0-m7

1. Claude Code runs `--info` (read-only) → `FM-1_46000600  [running]`.
2. Check the package:
   ```bash
   sha256sum build/step-M7/fm1acc-0.7.0-m7.fwsc
   ```
   expect `8478a955eb29c1de7474274a94ff6d96e4f7e2b0bfb8d98580012764bbfdb512`.
3. Install (you run it; 1–2 minutes):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M7/fm1acc-0.7.0-m7.fwsc
   ```
4. Claude Code runs `--info` → `FM-1_46000700  [running]`. The splash shows `v0.7.0-m7`, then the mode screen.
5. LEARN: your 14 songs are listed (Claude Code may run `tools/fm1song.sh list`, read-only: 14 songs, each `ok`).

If it fails: install M6 (P7) and report.

### Step 2 — The songs: Rockin' out, Zombie again (writes the song store; only after step 1)

1. Delete Rockin' (marks its record deleted; nothing is erased; to undo, send it again from
   `import/private/Rockin_in_the_Free_World.mid` via the importer):
   ```bash
   tools/fm1song.sh delete rockin-free-world
   ```
   One line `rockin-free-world: deleted`.
2. Send Zombie (format 2.1; it replaces the copy on the unit):
   ```bash
   tools/fm1song.sh send songs/private/zombie/
   ```
   One line `stored` (perhaps after "making room"). The other songs are not touched.
3. Claude Code runs `fm1song list` → 13 songs, each `ok`.

### Step 3 — Learning mode (a few minutes)

1. On the song list press **FX**: a small **NOT IMPLEMENTED** box; press PLAY: it closes, no song opens. Hold HOME 2 s
   with it open: the mode screen. Back to LEARN.
2. Open **Zombie**: the bass box shows **L2** at once. Play the lead part (bar 168 on: PRESETS to Part 22): is the
   lead right now beside the others?
3. Any other song opens on L1 everywhere, as before.

### Step 4 — SYNTH

1. Hold HOME 2 s → SYNTH: the preset big (NEON PAD), DRUMS ROCK1, 120 BPM. Turn **PRESET** through the 30 sounds;
   try 20 E-PIANO, 22 E-SITAR, 23 POTATO, 24 PARKER, 25 EDS (an arp: hold a chord).
2. Turn **K1..K4** on the main screen: the bottom row shows CUTOFF / RESONANCE / REVERB MIX / DELAY MIX.
3. **EDIT** through the 9 pages (SELECT stays the tempo); on **FX 1**..**FX 4** try tremolo, phaser, fuzz. ENV / FX
   jump to the pages; HOME (a tap) back to the main screen.
4. **PLAY**: the drums (ROCK1, ACOUSTIC). **SEQ**: the menu, SELECT to another pattern, SEQ: it changes on the next
   bar. **GLO, GLO**: the settings: Drum kit 808, then CHIP; Drum volume. GLO back. SELECT: the tempo (the drums, the
   arp and the delay follow).
5. LFO, SCL, SAVE or REC: NOT IMPLEMENTED (SAVE comes with M7.1).
6. Hold HOME 2 s with the drums playing: silence, the mode screen; LEARN: a song plays.

### Step 5 — Power off and on

Splash `v0.7.0-m7`, the mode screen; SYNTH: your drum kit and volume kept, ROCK1 again; LEARN: the 13 songs, your
settings kept.

## If anything goes wrong

- The install fails, or the app misbehaves: install M6 (P7); your songs and settings stay (Zombie 2.1 then shows as
  unreadable on M6: send it again, `fm1song send` sends M6 the 2.0 form):
  ```bash
  .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M6/fm1acc-0.6.0-m6.fwsc
  ```
- A send or the delete fails ("disconnected", "stopped answering"): stop, don't retry; Claude Code runs `--info` and
  reads the crash log (read-only). The store recovery (Settings > Wipe song store, the last row, or
  `tools/fm1song.sh wipe --yes`) is as at M5.1.

## Restore paths (D015, unchanged)

As [step-M3-install.md](step-M3-install.md).

## After the user has run it

Claude Code records the outputs, identities, `fm1song list` output and impressions in `docs/LOG.md`, and the
firmware in `docs/device.md`.
