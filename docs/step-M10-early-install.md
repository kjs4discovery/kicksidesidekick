# Milestone M10, early install point: 0.10.1-m10a on the FM-1

**Status: PREPARED, optional.** Nothing has been installed or sent. Claude Code ran no device command while preparing it.
**The user runs every device command**, one at a time, in their own terminal. Claude Code runs only the read-only ones
(`--info`, `fm1song list`) between steps and records them.

Prepared 2026-11-14 by Claude Code (docs/plan-M10.md, "Early install point"). Rules, pre-flight and restore paths as the
approved M9 checklist ([step-M9-install.md](step-M9-install.md)); what differs is here. **This build has no preset-saving code**
(Part 7 is not in it): it is Parts 1 to 6 only, so it writes the flash exactly as M9 does (the app area by the installer; the song
store by `send` / `delete`). The final package (0.10.0-m10, with user presets) has its own checklist, docs/step-M10-install.md.

---

## What is new (Parts 1 to 6; docs/controls.md "M10")

- **BACKTRACK value pop-up**: turn K1, K2, K3, K4, PRESETS, ALGO or SELECT and a pop-up names the knob ("Progression", "Style",
  "Song Complexity", "Melody Sound", "Key", "Structure", "Tempo") with the value in big letters; on the play screen a change that
  waits says "at the next phrase" or "at the next bar". It closes 1.5 s after the last turn; a button press closes it and does its
  job.
- **SIMPLE / MEDIUM / BUSY** everywhere (no more C1..C3); **NEXT:** spelled out (`NEXT: COMPLEXITY BUSY`, `NEXT: PROG 7` ...);
  SETUP's K4 row says **MELODY SOUND**.
- **The section indicator** (play screen, top line): INTRO / VERSE / CHORUS / BRIDGE / ENDING; in the last bar before a different
  section it shows the next one, **blinking**, then steady. In LOOP it never blinks.
- **Tuneka - Song Sidekick**: the new name on the splash (with `m10`) and the mode screen (a title; the mascot a little smaller).
- **42 progressions**: 37 PHRYGIAN, 38 LINE CLICHE, 39 MINOR CADENCE, 40 DESCENT, 41 LYDIAN, 42 JAZZ BLUES (suggested style
  SHUFFLE).
- **Band kits** (SETUP only): **SEQ** steps STUDIO (M9's sound) -> ACOUSTIC -> ELECTRO -> CHIP; the KIT row is under TEMPO.
- **The song info line**: on a song's Settings screen (GLO twice from its play screen) the last line says where the tune is from.
- **Fixes**: a section's crash no longer forces a kick (REGGAE's one-drop keeps its empty beat 1); a change made while PAUSED keeps
  the pause (PLAY goes on from that bar, with the count-in if it is on).

## What changes on the unit

| | Now (M9) | After |
|---|---|---|
| Firmware | M9 **FM-1_46000900**, v0.9.0-m9 | **FM-1_46001001**, v0.10.1-m10a (`--release 0.10.1-m10a`: the identity follows major.minor.patch; M10-10) |
| Step 1 writes | — | The app area 0x4000–0x92FFF and the update staging area 0xE0000–0xE4FFF, by our update loader, as every install before. fm1-base (incl. the loader) unchanged since 98fc08c. |
| Songs and backtracks | 13 songs + 36 backtracks | **Untouched** by step 1. Step 2 sends the new ones. |
| Settings | your M9 settings | Kept: the same record. |
| 0xDE000–0xDFFFF | unused | unused (user presets are Part 7, not in this build) |

## Pre-flight (any ❌ = stop)

| # | Check | State | Evidence |
|---|---|---|---|
| P1 | Off-laptop backup in `docs/device.md` | ✅ | Unchanged. |
| P2 | USB port + cable | ✅ | Port **1-2**. A `-110` beyond the usual one = stop and report. |
| P3 | Restore per D015 | ✅ | `firmware/vendor/fm1-base` unchanged since 98fc08c. |
| P4 | The package: commit + sha256 + emulator | ✅ | `build/step-M10/early/fm1acc-0.10.1-m10a.fwsc`, sha256 `832c0bcf4ad18dae9785f6241805fade13adbdcafd597685f229d1ed9bdfa03a`, identity FM-1_46001001, from commit 417b311, the same sha256 in two builds. Tests: see **Test results**. |
| P5 | Flash IDs | n/a | The M5 flash-ID check (0x856014 / 0xB36014 / unknown) is part of the FINAL package's suite (Part 9); no flash, store or flash-ID code changed in Parts 1 to 6 (store.c, library.c, the settings store untouched; song.c's parser got an optional-chunk check). |
| P6 | Unit state before step 1 | (check) | `--info`: `FM-1_46000900  [running]`. |
| P7 | Fallback package | ✅ | M9, known good on this unit: `build/step-M9/fm1acc-0.9.0-m9.fwsc`, sha256 `8a1c341d28eff7ba112463f8e633bc6828a4e703f7fa1e21313fdc8bcde3cf91`. An M9 FM-1 plays every file M10 makes (an INFO chunk is skipped: tested; the 6 new backtracks are ordinary 2.2 files). |
| P8 | The songs | ✅ | The 36 old backtracks' files are **byte-identical** to M9's (so none needs sending again). The 9 older public songs' files **changed**: each is the M9 file plus one INFO chunk (test_info.py: byte for byte after the chunk is removed; every event stream identical). Sending them again only adds the info line on the Settings screen. |

## Budget

| | 0.9.0-m9 | 0.10.1-m10a |
|---|---|---|
| App image (slot 585,728 B) | 507,440 B (78,288 free) | 511,412 B: **74,316 B free** |
| RAM .data+.bss (of 98,304) | 82,456 | 83,048 |
| BACKTRACK CPU, worst case (emulated; FULL, BUSY chorus, MIAMI PAD x 4, a live switch) | 44 % | STUDIO 44 %, ACOUSTIC 41 %, **ELECTRO 57 %**, CHIP 47 % (limit 60 %) |
| The song store (552 blocks) | 13 songs + 36 backtracks: 282 | 13 songs + 5 new + 42 backtracks: about 305 blocks (tools/store_blocks.py counts 19 songs + 42 backtracks as 307) |

## Test results (build/step-M10/early/)

Run once on this package, build/step-M10/early/ (reports in emu-m10/ and emu-m9/; logs, times.txt):

| Item | Result |
|---|---|
| m10 (tools/m10_test.py: m10p1 to m10p6) | **ALL PASS**, 29 checks, 816 s: the pause kept (starts 1, resume at bar 3 of the verse), the crash without a kick, the pop-up (titles, values, the 1.5 s close, a press that acts), NEXT, the indicator's blink within 2 frames of the line, the mode screen and splash, K1 to 42, the INFO line (the M9 package opens a file with INFO), every kit in all 11 styles at FULL, BUSY (notes identical across kits), CPU per kit |
| m9 (m9p1, m9p4, m9census) | **ALL PASS**, 34 checks, 466 s (the count-in rules, the knob map and its boundaries, the button census with SEQ's new job on SETUP) |
| pytest (tools/fm1song, C = Python for 42 backtracks x 11 styles x 4 tiers, the INFO goldens, the parser tests) | **298 passed**, 292 s |
| the full suite, the timed-flash tests, the flash-ID check | the FINAL package's, once (Part 9) |

## Known

- **camptown-races is NOT in the send list.** It fails two of the older suite's checks (its harmony level 4 plays 4-note attacks;
  harmony level loudness 1.64x > 1.6): decisions M10-03. The planning chat decides. The other five new songs are fine.
- **The six new songs' credits, rights and info lines are placeholders** (decisions M10-01: the planning chat's source files were
  lost and rebuilt; the notes and chords are exact).
- **ELECTRO is the heaviest kit** (57 % of a block): if you hear dropouts with a heavy melody sound, say so.
- **Decisions for review**: docs/decisions-M10.md.

---

## Checklist

### Step 1 — Install 0.10.1-m10a

1. Claude Code runs `--info` (read-only) → `FM-1_46000900  [running]`.
2. Check the package:
   ```bash
   sha256sum build/step-M10/early/fm1acc-0.10.1-m10a.fwsc
   ```
   expect `832c0bcf4ad18dae9785f6241805fade13adbdcafd597685f229d1ed9bdfa03a`.
3. Install (you run it; 1–2 minutes):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M10/early/fm1acc-0.10.1-m10a.fwsc
   ```
4. Claude Code runs `--info` → `FM-1_46001001  [running]`. The splash shows `Tuneka - Song Sidekick` and `m10`, then the mode
   screen with the title across the top.

If it fails: install M9 (P7) and report.

### Step 2 — Send the new backtracks and songs (writes the song store; only after step 1, in SONG LEARN)

Switch the FM-1 off and on and choose **SONG LEARN** first (BACKTRACK and SYNTH refuse sends). Delete nothing.

1. The six new backtracks (the other 36 are unchanged: do not send them again):
   ```bash
   tools/fm1song.sh send songs/backtracks/bt37-phrygian songs/backtracks/bt38-line-cliche songs/backtracks/bt39-minor-cadence songs/backtracks/bt40-descent songs/backtracks/bt41-lydian songs/backtracks/bt42-jazz-blues
   ```
   Six lines `stored`.
2. The five new songs (camptown-races is held back, M10-03):
   ```bash
   tools/fm1song.sh send songs/clementine songs/auld-lang-syne songs/drunken-sailor songs/blow-the-man-down songs/michael-row
   ```
   Five lines `stored`.
3. Optional, to see the info line on the older songs (each file changed by one INFO chunk; nothing else): resend them. The
   store replaces a song by its slug.
   ```bash
   tools/fm1song.sh send songs/amazing-grace songs/twelve-bar-blues songs/saints songs/four-chords songs/scarborough-fair songs/greensleeves songs/happy-birthday songs/danny-boy songs/oh-susanna
   ```
4. Claude Code runs `fm1song list`: 13 + 5 songs and 42 backtracks, each `ok`.

(The store may answer "making room" once: send again, as M5.1 taught.)

### Step 3 — BACKTRACK (a few minutes)

1. SETUP shows a **KIT** row under TEMPO. Turn the knobs: each opens the pop-up. K1 goes to **42** (37-39 MINOR, 40-41 MAJOR,
   42 JAZZ BLUES). **SEQ** steps the kit; PLAY.
2. On the play screen: turn K2 (the pop-up says "at the next phrase" and the top line `NEXT: STYLE ...`), K3 (SIMPLE / MEDIUM /
   BUSY), the key. Watch the **section name** in the top line: it blinks in the last bar before the next section (use ALGO to FULL
   to hear all the sections). PAUSE (PLAY), turn K2, PLAY: the music goes on from the same bar.
3. Hear each kit (STUDIO, ACOUSTIC, ELECTRO, CHIP) in two styles; REGGAE at SIMPLE should leave beat 1 empty in the chorus's first
   bar.
4. Which kits and styles sound right, which do not? Any dropout with ELECTRO?

### Step 4 — SONG LEARN

Open a song you sent again (saints): GLO, GLO: the Settings screen's last line says "Traditional spiritual, c. 1896". A private
song shows no line. Open one of the five new songs and play it with the layers.

## If anything goes wrong

- The install fails, or the app misbehaves: install M9 (P7); your songs and settings stay:
  ```bash
  .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M9/fm1acc-0.9.0-m9.fwsc
  ```
- A send fails ("disconnected", "stopped answering"): stop, don't retry; Claude Code runs `--info` and reads the crash log
  (read-only). The store recovery is as at M5.1.

## Restore paths (D015, unchanged)

As [step-M3-install.md](step-M3-install.md).

## After the user has run it

Claude Code records the outputs, identities, `fm1song list` output and impressions in `docs/LOG.md`, and the firmware in
`docs/device.md`.
