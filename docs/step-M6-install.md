# Milestone M6: install on the FM-1

**Status: PREPARED, waiting for review** by the planning chat and the user (docs/plan-M6.md, Part 4). Nothing has
been installed or sent. Claude Code ran no device command while preparing it. **The user runs every device command**
(the install, and the optional song sends), one at a time, in their own terminal. Claude Code runs only the read-only
ones (`--info`, `fm1song list`) between steps and records them.

Prepared 2026-11-10 by Claude Code. Rules, pre-flight and restore paths as the approved M5.1 checklist
([step-M5.1-install.md](step-M5.1-install.md)); what differs is here. The early install point after Part 2 was
skipped at the user's request: this is the only M6 build offered.

---

## What is new (docs/controls.md, "M6")

- **Learning:** HOME held 2 s (any screen) or Settings > Switch mode: back to the LEARN / SYNTH screen. QUIET readable.
  The melody roll has a piano keyboard on the left. Imported parts are matched in loudness every 4 bars.
- **Views:** Settings > Chord boxes, tab: GUITAR / UKULELE. Harmony: ukulele boxes (with the setting), a BAR VIEW.
  Melody: a GUITAR TAB or UKULELE TAB view. SELECT zooms the scrolling and staff views; REC then SELECT sets the
  tempo there. The chord boxes show the blinking rhythm icon.
- **SYNTH:** EDIT held = SHIFT (K1 reverb, K2 bit-crush, K3 arp rate, K4 detune); arp 1/8T and RANDOM; the drum jam
  (PLAY on/off, FX style, SCL level, ARP held + SELECT tempo).

## What changes on the unit

| | Now | After |
|---|---|---|
| Firmware | M5.1 **FM-1_46000504**, v0.5.4-m5p4 | **FM-1_46000600**, v0.6.0-m6 |
| Step 1 writes | — | The app area 0x4000–0x92FFF and the update staging area 0xE0000–0xE4FFF, by our update loader, as every install before. fm1-base (incl. the loader) unchanged since 98fc08c: `git diff 98fc08c -- firmware/vendor/fm1-base` is empty. |
| Your 14 songs | stored, format 2.0 | **Kept and read as they are.** The song format did not change: no resend needed. |
| Optional step 5 writes | — | The song store 0x97000–0xDBFFF only (the 5 private songs sent again: their imported parts re-matched in loudness every 4 bars), as the M5.1 sends. |
| Settings | your M5.1 settings | Kept (the same record; the two new rows, Switch mode and Chord boxes, tab, read their defaults: GUITAR). Wipe song store moves to the last row (13 rows). |

## Pre-flight (any ❌ = stop)

| # | Check | State | Evidence |
|---|---|---|---|
| P1 | Off-laptop backup in `docs/device.md` | ✅ | Unchanged. |
| P2 | USB port + cable | ✅ | Port **1-2**, the new cable. A `-110` beyond the usual one = stop and report. |
| P3 | Restore per D015 | ✅ | `firmware/vendor/fm1-base` unchanged since 98fc08c. |
| P4 | The package: commit + sha256 + emulator | ✅ | `build/step-M6/fm1acc-0.6.0-m6.fwsc`, sha256 `8944fb02a725978306754601e053e7c1f8b96f0280e13deed23201335888abec`, identity FM-1_46000600, built from commit 920fa67 (two builds, the same sha256). The full suite ran on the package before the one fix below (6451c638…, from 6f8ffe0): store, pytest (93), vsynth, guitar/ukulele, the fuzz (210,000 files), m51 (sends on the timed emulator, cuts, wipe) all pass; m5 and m6 found the HOME bug below plus one open loudness point (see *Known*). After the fix the groups it touches ran again on this package: m5 part3 + m5p1 (incl. the public songs' loudness) + boot, m6 (all but the long loudness runs, with the new tap check), m51 wipe: **ALL PASS**. |
| P5 | Flash IDs | ✅ | In the M5 suite on this package: 0x856014 and 0xB36014 the same; an unknown 0xC84017: `NO SONG STORAGE`, a send refused, nothing written; SYNTH plays. |
| P6 | Unit state before step 1 | (check) | `--info`: `FM-1_46000504  [running]`. In SAFE MODE: press PLAY ("try again") or power off/on first. |
| P7 | Fallback package | ✅ | M5.1, known good on this unit: `build/step-M5.1/fm1acc-0.5.4-m5p4.fwsc`, sha256 `197b84a552100969ee727d451d018cd83eeed7a0e2bdb744740dd84e1f0a187d` (reads the same songs and settings). Behind it M4: `build/step-M4/final/fm1acc-0.5.0-m4.fwsc`, sha256 `c811e0f7f002a2d7573843394e812490dbe3c2709c425d4b457c2351f767195b`. |
| P8 | The songs | ✅ | Format unchanged (2.0); the 14 on the unit stay. The 5 private ones re-imported (only the velocities of their imported parts changed). |

## Budget

| | 0.5.4-m5p4 | 0.6.0-m6 |
|---|---|---|
| App image (slot ~581 KB) | 451,744 B | 460,496 B |
| RAM .data+.bss (of 98,304) | 76,344 | 78,888 |
| RAM pool (of 344,064) | 289,864 | 289,864 |
| SYNTH, the busiest case (emulated CPU) | 8 voices, chorus + delay: 24–28 % of a block | 8 voices SYNC SCREAM, chorus + delay + reverb 127 + the drum jam L3: 34 % of a block |
| The song buffer in SYNTH (192 KiB) | delay + chorus 135,168 B | + reverb 14,080 B = 149,248 B; the drum jam after it (each pattern 364 B in the image) |
| Longest stretch without a watchdog feed (timed emulator; limit 8,000 ms) | 71 ms | 67 ms (m51: every send scenario) |

## Known (found by the final suite)

- **Fixed: a HOME tap could open the mode screen.** The hold timer's start (`now | 1`) could be 1 ms ahead of the
  clock, and a second pass in that millisecond read it as a hold of 49 days. The suite caught it twice (a loop cancelled
  with HOME, the hint's 11 openings). Fixed with `held_ms()` (a negative time is 0), plus a new m6p1 check
  (24 quick HOME / PLAY taps: never the mode screen).
- **The same timer bug was in M5.1's WIPE question (installed now):** on the WIPE screen, a short PLAY tap could (when it
  began on an even millisecond and the main loop came round within it) wipe the song store at once instead of needing the 2 s hold. Fixed in this build.
  **Until it is installed: do not open Settings > Wipe song store / GLO+HOME at power-on on the unit.**
- **Open: the generated levels of two private songs differ in loudness** (emulator, the first 12 s, each layer
  alone): rockin, drums L1 (the file's drums simplified) about half of L2/L3 (2.15x); harmony L1 (held block
  chords, distortion guitar) about 2x L3–L5 (2.06x); zombie drums 1.68x. The public songs, Let It Be and True Colors are
  within 1.6x (the m6p1 check fails on these three only). This is the generated levels, which M6 did not
  change. M5's check never measured the private songs, so this is not new. A question for the planning chat.

---

## Checklist

### Step 1 — Install 0.6.0-m6

1. Claude Code runs `--info` (read-only) → `FM-1_46000504  [running]`.
2. Check the package:
   ```bash
   sha256sum build/step-M6/fm1acc-0.6.0-m6.fwsc
   ```
   expect `8944fb02a725978306754601e053e7c1f8b96f0280e13deed23201335888abec`.
3. Install (you run it; 1–2 minutes):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M6/fm1acc-0.6.0-m6.fwsc
   ```
4. Claude Code runs `--info` → `FM-1_46000600  [running]`. The splash shows `v0.6.0-m6`, then the mode screen.
5. LEARN: your 14 songs are listed (Claude Code may run `tools/fm1song.sh list`, read-only: 14 songs, each `ok`).

If it fails: install M5.1 (P7) and report.

### Step 2 — Learning mode (a few minutes)

1. Open a song, play it, **hold HOME 2 s**: the song stops, the mode screen (cursor on SYNTH). SELECT to LEARN, PLAY:
   the list. Also GLO, GLO (settings) > **Switch mode**.
2. Press a layer button once: **QUIET** readable on its box?
3. Melody your part (hold LFO), EDIT to the **MELODY ROLL**: the piano keys on the left, the sounding note's key lit.
4. **Let It Be** and **True Colors**: harmony alone (press the other three layer buttons twice: MUTED), turn K3
   through L1–L5 while it plays. Is **L3** still louder than the others? (Emulator: Let It Be's five levels within
   4 %; your earlier note came from the M5 trial, when the unit still held M4's imports. Step 5 gives the newer
   4-bar matching.)

### Step 3 — The views

1. Harmony, EDIT to **GUITAR** while playing: the rhythm icon under the x / o lines blinks at each chord attack.
2. GLO, GLO > **Chord boxes, tab** → UKULELE, GLO: the boxes have 4 strings (G C E A); the melody's tab view becomes
   **UKULELE TAB**. Set it back to GUITAR if you like.
3. Harmony: EDIT to **BAR VIEW** (after NOTATION). Melody: EDIT to **GUITAR TAB** (after NOTATION).
4. In a scrolling view (a roll, a tab, the drum grid, a notation): turn **SELECT** — zoom out / in. Press **REC**,
   then turn SELECT — the tempo (the top line shows it); after 4 s SELECT zooms again.

### Step 4 — SYNTH

1. Hold HOME 2 s → SYNTH. Hold **EDIT**: SHIFT, the knobs read REVERB, CRUSH, ARP RATE, DETUNE; turn K1 (reverb)
   up on a pad, K2 (crush) on a lead.
2. ARP on, **SEQ** until RANDOM; **GLO** to 1/8T.
3. **PLAY**: the drums (POP L2). **FX**: another style; **SCL**: another level; ARP held + SELECT: the tempo. Play
   over it. PLAY again: the drums stop.
4. Hold HOME 2 s with the drums playing: silence, the mode screen; LEARN: a song plays.

### Step 5 — Optional: the private songs again (their 4-bar loudness matching)

Only if you want it (writes the song store, as at M5.1):
```bash
tools/fm1song.sh send songs/private/*/
```
Five lines `stored` (replaced), some perhaps after "making room". Claude Code runs `fm1song list` → 14 songs, each
`ok`. Then Step 2.4 again.

### Step 6 — Power off and on

Splash `v0.6.0-m6`, the mode screen, LEARN, the 14 songs; your settings kept (the ukulele setting too).

## If anything goes wrong

- The install fails, or the app misbehaves: install M5.1 (P7); your songs and settings stay:
  ```bash
  .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M5.1/fm1acc-0.5.4-m5p4.fwsc
  ```
- A send fails ("disconnected", "stopped answering"): stop, don't retry; Claude Code runs `--info` and reads the
  crash log (read-only). The store recovery (Settings > Wipe song store, the last row now, or `tools/fm1song.sh wipe
  --yes`) is as at M5.1.

## Restore paths (D015, unchanged)

As [step-M3-install.md](step-M3-install.md).

## After the user has run it

Claude Code records the outputs, identities, `fm1song list` output and impressions in `docs/LOG.md`, and the
firmware in `docs/device.md`.
