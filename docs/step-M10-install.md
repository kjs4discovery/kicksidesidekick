# Milestone M10: install on the FM-1

**Status: PREPARED, waiting for review** by the planning chat and the user (docs/plan-M10.md, Part 9). Nothing has been
installed or sent. Claude Code ran no device command while preparing it. **The user runs every device command** (the install,
the sends), one at a time, in their own terminal. Claude Code runs only the read-only ones (`--info`, `fm1song list`) between
steps and records them.

Prepared 2026-11-14 by Claude Code. Rules, pre-flight and restore paths as the approved M9 checklist
([step-M9-install.md](step-M9-install.md)); what differs is here. The early build ([step-M10-early-install.md](step-M10-early-install.md))
is optional: this checklist installs over **M9 or over M10a**. **Delete nothing.**

---

## What is new (docs/controls.md, "M10")

Everything in the early build (BACKTRACK's value pop-up, SIMPLE / MEDIUM / BUSY, the spelled-out NEXT line, the blinking section
indicator, "Tuneka - Song Sidekick", 42 progressions, the song info line, four band kits, the crash without a kick, a pause kept
through a change; see the early checklist), and:

- **Saving your own sounds (SYNTH)**: **SAVE** opens **SAVE TO Unn**: SELECT the slot (U01..U30), K1 / K2 edit the name (it starts as
  the current preset's), SAVE again saves; a taken slot asks "REPLACE ...? SAVE = yes, HOME = no". The saved sound is then selected as
  U05 (for instance). **PRESETS** goes through the 30 stock presets and then your saved ones; BACKTRACK's K4 (melody sound) uses the
  same list. There is no delete (overwrite only). The stock presets can never be overwritten.

## What changes on the unit

| | Now | After |
|---|---|---|
| Firmware | M9 **FM-1_46000900** (or M10a FM-1_46001001) | **FM-1_46001000**, v0.10.0-m10 |
| Step 1 writes | — | The app area 0x4000–0x92FFF and the update staging area 0xE0000–0xE4FFF, by our update loader, as every install before. fm1-base (incl. the loader) is unchanged since the M10 start (only M5.1's flash-ID patch, as installed). |
| Step 2 writes (the song store 0x97000–0xDBFFF only) | — | Only `send` (new songs and backtracks, below). **Nothing is deleted.** |
| The preset object, 0xDE000–0xDFFFF (an A/B pair of 4 KiB sectors) | unused | **Written only when you press SAVE in SYNTH** (an erase and 9 page writes of the copy not in use; torn-write safe). Never at start-up, never by a send, a delete or WIPE SONG STORE. |
| Settings | your settings | Kept: the same record. |

## Pre-flight (any ❌ = stop)

| # | Check | State | Evidence |
|---|---|---|---|
| P1 | Off-laptop backup in `docs/device.md` | ✅ | Unchanged. |
| P2 | USB port + cable | ✅ | Port **1-2**. A `-110` beyond the usual one = stop and report. |
| P3 | Restore per D015 | ✅ | `firmware/vendor/fm1-base` unchanged since the M10 start. |
| P4 | The package: commit + sha256 + emulator | ✅ | `build/step-M10/fm1acc-0.10.0-m10.fwsc`, sha256 `27128989d16660ae7f40fee55b4b0c561540fa4c6661fe162d631080dcbf60c8`, identity FM-1_46001000, built from commit f9fcc40 (a clean tree: only the user's untracked `import/` beside it, which no build or test reads), the same sha256 in two builds. The full (trimmed) suite ran once on it: **Test results**. |
| P5 | Flash IDs | ✅ | m5's `flash` group ran on THIS package: 0x856014 and 0xB36014 accepted, an unknown ID refused without a crash (result below). |
| P6 | Unit state before step 1 | (check) | `--info`: `FM-1_46000900  [running]` (or `FM-1_46001001` with M10a). In SAFE MODE: press PLAY ("try again") or power off/on first. |
| P7 | Fallback package | ✅ | M9, known good on this unit: `build/step-M9/fm1acc-0.9.0-m9.fwsc`, sha256 `8a1c341d28eff7ba112463f8e633bc6828a4e703f7fa1e21313fdc8bcde3cf91`. **A preset object left behind by M10 is ignored by M9** (and by M10a): M9's object map is {0xFC000, 0xDC000}, its song store ends at 0xDBFFF and its WIPE erases only that area (checked in M9's app.h / store.h / store.c: nothing there refers to 0xDE000..0xDFFFF), and the update loader's staging area starts at 0xE0000. After going back to M9 the saved presets are simply not offered; installing M10 again finds them. |
| P8 | The songs | ✅ | The 36 old backtracks' files are byte-identical to M9's. The 9 older public songs' files changed by one INFO chunk (events identical; tests/test_info.py). New: 6 backtracks (37-42), 5 songs. |

## Budget

| | 0.9.0-m9 | 0.10.0-m10 |
|---|---|---|
| App image (slot 585,728 B) | 507,440 B | 516,036 B: **69,692 B free** (rule: at least 16 KB) |
| RAM .data+.bss (of 98,304) | 82,456 | 88,928 (the presets' buffers and SYNTH's SAVE screen: about 5.9 KB); 9,376 B free |
| RAM pool (of 344,064) | 291,464 | 291,464 |
| BACKTRACK CPU, the worst case (emulated; FULL, BUSY in a chorus, MIAMI PAD x 4, a live switch) | 44 % | STUDIO 44 %, ACOUSTIC 41 %, ELECTRO 57 %, CHIP 47 % (limit 60 %): see results |
| The preset object | — | 1,928 B (an 8-byte header + 30 x 64-byte records; the store's limit 3,840 B); 30 slots; a save is 10 flash operations |
| The song store (552 blocks) | 13 songs + 36 backtracks: 282 | 13 songs + 5 new + 42 backtracks: about **304 blocks**, 248 free (tools/store_blocks.py) |
| Longest stretch without a watchdog feed (timed emulator) | 79 ms | see results (30 preset saves in a row: 34 ms; goal under 1000 ms) |

## Test results (the full suite, once, on this package: plan Part 9, trimmed, D064)

Run by the test-runner subagent in lanes (build/step-M10/run_lanes.sh serial: lanes A and B at once, lane C after B, because the timed emulator is
load-sensitive; the plan's fallback), times in build/step-M10/times.txt, reports in build/step-M10/emu-*/.

| Item | Result |
|---|---|
| A: store test (simulated flash, a cut at every operation), upreset test (the preset object, a cut at every operation), vsynth, guitar / ukulele | rc 0 each |
| A: pytest (every song's goldens, the INFO-only difference, C = Python for 42 backtracks x 11 styles x 4 tiers) | **298 passed** (231 s) |
| A: song_fuzz 600 s | clean: 228,000 files, 10,034 accepted, no sanitizer report |
| B: m5 (trimmed; the flash IDs 0x856014 and 0xB36014 play and look the same, the unknown ID shows NO SONG STORAGE without a crash) | 255 PASS, 2 FAIL in the first run (below) |
| B: m6 --quick / m8 | 21/0 and 37/0 |
| B: m7 (m7p1, p2, p4, p5, p6, cpu) | 49 PASS, 1 FAIL (below) |
| B: m9 (m9p1, p2, p4, census, cpu; the sweep trimmed with M9_SWEEP_TRIM=1: 132 runs) | **39/0**; worst CPU 44 % (m9cpu), the sweep's worst block 30 % |
| B: m10 (m10p1 to m10p7) | **38/0**; CPU per kit: STUDIO 44 %, ACOUSTIC 41 %, ELECTRO 57 %, CHIP 47 % |
| C: m51 (the song store on the timed flash) | **17/0** |
| C: m10_flash (30 saves; a power cut at each of 20 operations; junk; sends, deletes and WIPE leave the object alone) | **6/0**; longest unfed stretch 34 ms, 0 watchdog resets, 0 XIP reads with a flash command running |

**Failures and what I did (no check relaxed):**
1. m5 `store`, "a damaged stored song": a STALE expectation: the test flips a byte 40 bytes from the file's end, which is now the INFO chunk; that
   breaks the list's peek (the song is not listed) instead of the open. The test now flips a byte in the middle (events); re-run on the same
   package: PASS.
2. m7 `m7p2`: SAVE no longer shows "NOT IMPLEMENTED" in SYNTH (it has a job now): expectation updated; re-run on the same package: ALL PASS.
3. m5 `m5p1`, "the controls hint shown 10 times in 11 openings": **got 9, still failing** (also with 900 ms between openings and with no other run
   going; the same on the M9 package in Part 0). Not caused by M10 and not understood: reported, not relaxed. The hint is cosmetic (settings:
   shown up to 10 times).


## Known

- **camptown-races is NOT in the send list** (it fails two older checks, decisions M10-03); its files are committed unchanged.
- **The six new songs' credits, rights text and info lines are placeholders** (decisions M10-01: the planning chat's source files were lost
  and rebuilt from the compiled files; every note and chord, the form, the style and the tempo are exact, the song.mid files are
  byte-identical). Replace them when the planning chat re-sends the originals.
- **ELECTRO is the heaviest kit** (57 % of a block in the worst case): tell me if you hear dropouts with a heavy melody sound.
- **BACKTRACK keeps its melody sound as a position in the list** until power-off: saving a preset in SYNTH can shift what that position
  means by one (turn K4 once).
- **No delete for saved presets** (overwrite only): a later milestone.
- **The check I did not run**: Zombie's drums level 1 (1.68x quieter, the old known item) is not re-measured (m6 --quick).
- **Decisions for review**: docs/decisions-M10.md, M10-01 to M10-14 (every judgment call the plan did not spell out).
- **Sends**: only in SONG LEARN (BACKTRACK and SYNTH refuse: the synth holds part of the song buffer).

---

## Checklist

### Step 1 — Install 0.10.0-m10

1. Claude Code runs `--info` (read-only) → `FM-1_46000900  [running]` (or `FM-1_46001001`).
2. Check the package:
   ```bash
   sha256sum build/step-M10/fm1acc-0.10.0-m10.fwsc
   ```
   expect `27128989d16660ae7f40fee55b4b0c561540fa4c6661fe162d631080dcbf60c8`.
3. Install (you run it; 1–2 minutes):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M10/fm1acc-0.10.0-m10.fwsc
   ```
4. Claude Code runs `--info` → `FM-1_46001000  [running]`. The splash shows `Tuneka - Song Sidekick` and `m10`.

If it fails: install M9 (P7) and report.

### Step 2 — Send the new songs and backtracks (writes the song store; only after step 1, in SONG LEARN)

Switch the FM-1 off and on and choose **SONG LEARN** first. **Delete nothing.** If you already did step 2 of the early checklist, skip to
step 3 (nothing here changed since).

1. The six new backtracks:
   ```bash
   tools/fm1song.sh send songs/backtracks/bt37-phrygian songs/backtracks/bt38-line-cliche songs/backtracks/bt39-minor-cadence songs/backtracks/bt40-descent songs/backtracks/bt41-lydian songs/backtracks/bt42-jazz-blues
   ```
   Six lines `stored`.
2. The five new songs (camptown-races is held back):
   ```bash
   tools/fm1song.sh send songs/clementine songs/auld-lang-syne songs/drunken-sailor songs/blow-the-man-down songs/michael-row
   ```
3. Optional (the info line on the older songs; each file is the old one plus one chunk):
   ```bash
   tools/fm1song.sh send songs/amazing-grace songs/twelve-bar-blues songs/saints songs/four-chords songs/scarborough-fair songs/greensleeves songs/happy-birthday songs/danny-boy songs/oh-susanna
   ```
4. Claude Code runs `fm1song list`: 13 + 5 songs and 42 backtracks, each `ok` (no song is `UNREADABLE` or `DAMAGED`).

(The store may answer "making room" once: send again, as M5.1 taught.)

### Step 3 — Saving a preset: the test you do by hand (a few minutes)

1. Hold HOME 2 s → the mode screen → **SYNTH**, PLAY. Turn **PRESETS** to a sound you like and turn K1-K4 to change it.
2. Press **SAVE**: **SAVE TO U01**, the name of the sound under it, a cursor. Turn **K2** to change a letter, **K1** to move the cursor.
   Press **SAVE**: "SAVED U01" for a moment, the sound goes on. (Nothing else sounds a click: the output fades for the write.)
3. **Power the FM-1 off and on.** SYNTH again: turn PRESETS past the 30th stock preset: your sound is next, named "U01 ...". It sounds as saved.
4. **Replace it**: choose it, change a knob, SAVE: the slot (its own) is chosen; SAVE: "REPLACE U01 <name>?  SAVE = yes  HOME = no": press **HOME**: back, nothing
   written. Do it again and say yes (SAVE): "SAVED U01". Power off and on: the new sound is there.
5. In BACKTRACK, turn **K4** (melody sound) past the 30 stock sounds: your saved sound is in the list.
6. Claude Code runs `--info` and `fm1song list` (read-only): the unit answers, the songs are all `ok` (a save never touches them).

### Step 4 — BACKTRACK and SONG LEARN (a few minutes; see the early checklist's steps 3 and 4)

The pop-ups, the section indicator, the kits, the info line, 42 progressions; a SONG LEARN song plays as before. Which kits and styles sound
right?

## If anything goes wrong

- The install fails, or the app misbehaves: install M9 (P7); your songs and settings stay, and saved presets are ignored by M9 (P7):
  ```bash
  .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M9/fm1acc-0.9.0-m9.fwsc
  ```
- A send fails ("disconnected", "stopped answering"): stop, don't retry; Claude Code runs `--info` and reads the crash log (read-only). The store
  recovery (Settings > Wipe song store, or `tools/fm1song.sh wipe --yes`) is as at M5.1 and never touches the presets.
- A save fails or the FM-1 misbehaves right after one: switch it off and on. A torn write leaves the previous copy in charge (tested at every
  flash operation); if SYNTH then shows no saved presets, the object was unreadable and the slots are empty: nothing else is affected.

## Restore paths (D015, unchanged)

As [step-M3-install.md](step-M3-install.md).

## After the user has run it

Claude Code records the outputs, identities, `fm1song list` output and impressions in `docs/LOG.md`, and the firmware in
`docs/device.md`.
