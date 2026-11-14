# Milestone M10.1: install on the FM-1

**Status: PREPARED, waiting for the user.** Nothing has been installed or sent. Claude Code ran no device command while preparing it (docs/plan-M10.1.md Part 8).
**The user runs every device command**, one at a time, in their own terminal: the install, the factory reset and the sends. Claude Code runs only the read-only ones
(`--info`, `fm1song list`) between steps and records them.

Prepared 2026-11-14 by Claude Code (the planning chat's plan docs/plan-M10.1.md; the user's notes on M10). Rules, pre-flight and restore paths as the approved M10 checklist
([step-M10-install.md](step-M10-install.md)); what differs is here. **Delete nothing by hand**: the clean install (step 2, below) erases everything on purpose, and warns first.

---

## What is new (docs/controls.md, "M10.1"; screenshots docs/step-M10.1/)

- The name **"KickSide - Your Musical SideKick"** (splash, mode screen, help); SYNTH's edit pages name the preset ("FX 2 - POTATO"); the bottom instruction row keeps each page **6 s**; BACKTRACK's
  LOOP structure shows **LOOP**; the **value pop-up** in SONG LEARN and SYNTH.
- **Layer buttons**: first press = your part, pressing again = its volume; no hold. **HOME** climbs one level a press and ends on the mode screen (acts when released; 2 s still goes straight there).
  **SONG LEARN's K4 = Melody Sound** (SONG, then the presets; the melody plays on the synth); BACKTRACK's K4 keeps its choice when you save a preset.
- **Factory reset** (`tools/fm1song.sh factory-reset --yes`) and **`tools/fresh_install.sh`**: install, reset, then the whole library (`songs/library.txt`: 15 songs, 42 backtracks) in one run.
- **Presets**: new list order (PARKER, POTATO, EDS, STAB 84 right behind the three E- presets), **E-GUITAR** (attack 12, fuzz 35), the new **EDS** (a fuzz bass), **ZUNJET** (yours) in HAZE PAD's place.
- **Songs**: camptown-races, michael-row, oh-susanna (125 BPM), saints (135 BPM), scarborough-fair (an 18-bar verse); Creep (private, bars 8, 16, 40).

## What changes on the unit

| | Now | After |
|---|---|---|
| Firmware | M10 **FM-1_46001000** | **FM-1_46001002**, v0.10.2-m10.1 (the splash shows "KickSide - Your Musical SideKick" and "m10.1") |
| Step 1 writes | — | The app area 0x4000–0x92FFF and the update staging area 0xE0000–0xE4FFF, by our update loader, as every install before. fm1-base (the loader) is **unchanged** since M10 (`git diff 8d2f517 HEAD -- firmware/vendor` is empty). |
| Step 2 writes (**the clean install**, optional) | — | `factory-reset`: the song store 0x97000–0xDBFFF erased, the preset object 0xDE000–0xDFFFF erased (**all your saved presets, U01-U30, and ZUNJET's U slot**), the settings back to the defaults; then the sends (the song store again). The panel calibration (0xDC000) and the firmware stay. |
| Your saved presets | U-slots kept | **Gone after step 2** (ZUNJET is a stock preset now: no. 19). Without step 2 they stay; a preset object left by M10 is read by M10.1. |

## Pre-flight (any ❌ = stop)

| # | Check | State | Evidence |
|---|---|---|---|
| P1 | Off-laptop backup in `docs/device.md` | ✅ | Unchanged. |
| P2 | USB port + cable | ✅ | Port **1-2**. A `-110` beyond the usual one = stop and report. |
| P3 | Restore per D015 | ✅ | `firmware/vendor/fm1-base` unchanged since M10 (empty `git diff`). |
| P4 | The package: commit + sha256 + emulator | ✅ | `build/step-M10.1/fm1acc-0.10.2-m10.1.fwsc`, sha256 `7ff839308734740be83175a204f204ab46702a9fbb4a943aa63548c9db77b5af`, identity **FM-1_46001002**, built from commit db81fa8 (a clean tree: only the user's untracked `import/`), the same sha256 in two builds (`firmware/build.sh --release 0.10.2-m101`, copied to the plan's file name). The fast lane ran once on it: **Test results**. |
| P5 | Flash IDs | ✅ | The app's and the loader's flash-ID code is fm1-base's (0x856014 and 0xB36014 accepted, others refused: `firmware/vendor/fm1-base/device/main_fm1.c`), **unchanged since M10**, whose m5 `flash` group passed on the M10 package; M10.1 changes nothing there. (The fast lane does not run m5; see Test results for the one flash group run I added.) |
| P6 | Unit state before step 1 | (check) | `--info`: `FM-1_46001000  [running]`. In SAFE MODE: press PLAY ("try again") or power off/on first. |
| P7 | Fallback package | ✅ | M10, known good on this unit (installed and used, no problems): `build/step-M10/fm1acc-0.10.0-m10.fwsc`, sha256 `27128989d16660ae7f40fee55b4b0c561540fa4c6661fe162d631080dcbf60c8`. The preset object M10.1 leaves is read by M10 (the record format is unchanged, version 1). Behind it M9. Back to stock any time: `fm1_install.py backups/stock-firmware/FM-1_015.fwsc`. |
| P8 | The songs | ✅ | The five changed songs' files differ (their notes, chords, tempos); every other song's file is byte-identical (`make_songs.py` for all slugs changes nothing else; pytest 298 passed). |

## Budget

| | 0.10.0-m10 | 0.10.2-m10.1 |
|---|---|---|
| App image (slot 585,728 B) | 516,036 B | **519,092 B: 66,636 B free** (rule: at least 16 KB) |
| RAM .data+.bss (of 98,304) | 88,928 | see the build line below (the SONG LEARN melody synth, the factory reset and the test texts) |
| RAM pool (of 344,064) | 291,464 | 291,464 (the synth's memory is the top of the song buffer, as in BACKTRACK) |
| CPU, SONG LEARN with the melody on the heaviest preset (MIAMI PAD) at the busiest levels, EAS + synth | — | **35 %** of a block (limit 60 %); a sweep of all 30 presets: 28-34 % |
| CPU, BACKTRACK (M10's worst case) | STUDIO 44 %, ACOUSTIC 41 %, ELECTRO 57 %, CHIP 47 % | unchanged code: m10p6 re-ran in the fast lane (see results) |
| The song store (552 blocks) | about 304 | the library 15 + 42: about the same (tools/store_blocks.py) |

## Test results (the fast lane, once, on this package: plan Part 8; the user's choice: no full suite)

(see the end of this file: filled in from build/step-M10.1/times.txt)

---

## Checklist

### Step 1 — Install 0.10.2-m10.1 over M10

1. Claude Code runs `--info` (read-only) → `FM-1_46001000  [running]`.
2. Check the package:
   ```bash
   sha256sum build/step-M10.1/fm1acc-0.10.2-m10.1.fwsc
   ```
   expect `7ff839308734740be83175a204f204ab46702a9fbb4a943aa63548c9db77b5af`.
3. Install (you run it; 1–2 minutes):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M10.1/fm1acc-0.10.2-m10.1.fwsc
   ```
4. Claude Code runs `--info` → `FM-1_46001002  [running]`. The splash shows `KickSide - Your Musical SideKick` and `m10.1`.

If it fails: install M10 again (P7) and report.

### Step 2 — The clean install, like a new user (writes: song store, saved presets, settings), OR just re-send the five songs

**Either** (a), the clean install. It does steps 1 to 3 again for you (so you can skip step 1 above and run only this):

```bash
tools/fresh_install.sh build/step-M10.1/fm1acc-0.10.2-m10.1.fwsc
```

It prints each step and asks y/n before each write: (1) `--info`; (2) the install; (3) waits for the FM-1 to come back as FM-1_46001002; (4) **a warning: this erases ALL songs, saved presets (U01-U30) and
settings** (y/n); (5) `fm1song factory-reset --yes`; (6) **you switch the FM-1 off and on, choose SONG LEARN, press Enter**; (7) sends every entry of `songs/library.txt` (the songs, then the backtracks);
(8) `fm1song list` and a count: 57 entries, each `ok`. At the first failure it stops and prints the fallback line (the M10 package). **No preset read-out exists** (you withdrew it): write down any saved preset
you want to keep before you say y at (4). ZUNJET is a stock preset now.

**Or** (b), without the clean install (your presets and settings stay): in SONG LEARN, send the five changed songs (it replaces the old copies; the store may say "making room" once: send again):

```bash
tools/fm1song.sh send songs/camptown-races songs/michael-row songs/oh-susanna songs/saints songs/scarborough-fair
```

Then Claude Code runs `fm1song list` (read-only): every song `ok`.

(The private Creep is not in the library; if you want it on the unit: `tools/fm1song.sh send songs/private/creep`, after the clean install.)

### Step 3 — By hand: what to look at (one line per note of yours)

| Your note | Where | What you should see |
|---|---|---|
| The name | Splash, mode screen | "KickSide - Your Musical SideKick" on one line, `m10.1` under it on the splash |
| Edit page titles | SYNTH, EDIT or ENV / FX | "ENVELOPE - E-PIANO", "FX 2 - POTATO", a saved one "FX 1 - U05 NAME"; a long name in smaller type, never cut |
| Hint timing | Any stopped screen with the bottom row | Each page stays about 6 s (it was 3 s) |
| LOOP | BACKTRACK, structure LOOP | The section box says LOOP and never blinks; FULL and VERSE/CHORUS as before |
| Pop-ups | SONG LEARN K1-K3, SELECT, PRESETS, ALGO; SYNTH PRESET, SELECT, K1-K4 | A box with the title and the value ("Drums Level 2 of 3", "Tempo 112 BPM", "Preset NEON PAD", "CUTOFF 90") |
| Layer buttons | SONG LEARN and BACKTRACK, FX / SEL / ENV / LFO | First press: "Your Part"; again: "Bass Volume QUIET" ... MUTED ... FULL; each layer keeps its own volume; the row says "FX-LFO: pick part | again: volume" |
| HOME | Everywhere | One press up one level; from SONG LEARN's folders / BACKTRACK's SETUP / SYNTH's main screen the next press is the mode screen; 2 s still goes there at once; REPLACE ... ? and HOME = no |
| K4 melody sound | SONG LEARN (open a song) | K4 steps SONG, E-PIANO, E-GUITAR ...; the melody plays in that sound; opening a song again is back to SONG; BACKTRACK's K4 unchanged (and keeps its sound after you save a preset) |
| Preset order | SYNTH PRESET knob | E-PIANO, E-GUITAR, E-SITAR, PARKER, POTATO, EDS, STAB 84, NEON PAD ... |
| ZUNJET | The PRESET knob, no. 19 | In HAZE PAD's place (after DRIVE BASS, before LASER ARP); your values (LEVEL 97: its loudness is +1.6 dB over the median; say if you want 94) |
| E-GUITAR | no. 2, ENVELOPE and FX 3 pages | ATTACK 12, FUZZ 35 (LEVEL 100 as before; -1.6 dB under the median; 103 would match) |
| EDS | no. 6 | A fuzz bass: a saw and a strong sub, a slight warble; low notes (OCT- helps). I judged it by numbers, not by ear: tell me what to change |
| The five songs | SONG LEARN | camptown-races (chorus bar 3 rhythm, A7 "doo-dah" bars), michael-row (the A on bar 3 moved on), oh-susanna 125 BPM, saints 135 BPM, scarborough-fair an 18-bar verse |

**STOP.** The user installs; Claude Code runs only `--info` / `fm1song list` and records.

## Known

- **The melody synth of SONG LEARN needs room in the song buffer**: a song whose levels need more than 112,896 B (only the private Zombie, 172 KB) keeps its own instrument ("no room: keeps SONG").
- **Sends and the factory reset are refused in SYNTH and BACKTRACK** (result 9: switch off and on, choose SONG LEARN); in SONG LEARN a melody sound is switched off first.
- **After a factory reset the FM-1 reads nothing** (the screen says "FACTORY RESET DONE / SWITCH OFF AND ON"): switch it off and on.
- **ZUNJET's and E-GUITAR's LEVELs are the values you gave** (not matched): see the table above.
- **No delete for saved presets** (overwrite only) and **no read-out**: unchanged.
- **Decisions for review**: docs/decisions-M10.1.md, M10.1-01 to M10.1-15.
