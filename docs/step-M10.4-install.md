# Milestone M10.4: install on the FM-1

**Status: PREPARED, waiting for the user.** Nothing has been installed or sent. Claude Code ran no device command while preparing it.
**The user runs every device command**, one at a time, in their own terminal. Claude Code runs only the read-only ones (`--info`, `fm1song list`) between steps and records them.

Prepared 2026-11-15 by Claude Code (docs/plan-M10.4.md; decisions docs/decisions-M10.4.md). Rules, pre-flight and restore paths as the approved M10 / M10.1 / M10.2 checklists
([step-M10.2-install.md](step-M10.2-install.md)). **Install over M10.2** (the unit runs 0.10.3-m10.2, FM-1_46001003): M10.3's package was built but is superseded and **never installed**; M10.4 carries all of M10.3.
**No factory reset and nothing to send**: no song changed; saved presets and settings stay.

## What is new (docs/reference.md; docs/controls.md "M10.3" and "M10.4")

- **The built-in starter library:** the 15 songs and 42 backtracks of songs/library.txt are inside the firmware (about 26 KB packed). A new FM-1, or one after a factory reset, has them at once. The store wins by slug (a sent song replaces its built-in copy in the lists; deleting it brings the built-in back). The USB commands see only the store: **`fm1song list` no longer shows everything the unit can play**.
- From M10.3: BACKTRACK's indicator leaves LOOP at once; HOME from playing or paused goes straight to SETUP; SEQ steps the band kit while playing (bass and harmony sounds too); SONG LEARN's list footer, the empty screens that point to the website, the keys play the melody's sound, the Start pop-up's bigger section name; POTATO's CRUSH 127; the new splash and mode-screen mascot with a bold "Kick"; open-source files.
- The kit pop-up says **"Band Kit"** again; the knock-knock comment is gone.
- **Fixed on the way (found by the emulator run): M10.3's help screens crashed** (a table row with no text): every help but BACKTRACK's play help showed the crash screen. This is fixed in this package; M10.3 was never installed.
- **Pop-ups is ON again after this update** (as designed).

## What changes on the unit

| | Now | After |
|---|---|---|
| Firmware | M10.2 **FM-1_46001003** | **FM-1_46001005**, v0.10.5-m10.4 (the splash shows "v0.10.5-m10.4") |
| Step 1 writes | | The app area 0x4000-0x92FFF and the update staging area 0xE0000-0xE4FFF, by our update loader, as every install before. fm1-base (the loader) is unchanged since M10.1 (`git diff db81fa8 HEAD -- firmware/vendor` is empty). |
| The settings record | M10.2's | Written once at the first start (the firmware marker; Pop-ups ON); layout unchanged |
| The song store, saved presets | | **Not written** (no step 2); the store already holds the 57 library files, so the store copies win and the lists look the same |

## Pre-flight (any failed row = stop)

| # | Check | State | Evidence |
|---|---|---|---|
| P1 | Off-laptop backup in `docs/device.md` | ok | Unchanged. |
| P2 | USB port + cable | ok | Port **1-2**. A `-110` beyond the usual one = stop and report. |
| P3 | Restore per D015 | ok | `firmware/vendor/fm1-base` unchanged since M10.1. |
| P4 | The package | ok | `build/step-M10.4/fm1acc-0.10.5-m10.4.fwsc` (`firmware/build.sh --release 0.10.5-m104`, copied to this name), sha256 `769c87bfdf746dcb1aa39571ca40dbf4d58fbcfae66f1cd8a3f278cc3559cc43`, identity **FM-1_46001005**, from commit 6adc7ee (a clean tree: only your untracked `import/`), the same sha256 in two builds. |
| P4b | **Emulator (CLAUDE.md)** | ok | The trimmed run of plan Part 5 on this firmware (details below). Everything passed; **it found one real crash (M10.3's help tables) which is fixed in this package**. |
| P5 | Flash IDs | ok | The app's and the loader's flash-ID code is fm1-base's (0x856014 and 0xB36014 accepted, others refused), unchanged since M10.1; m5's flash group (856014, b36014, unknown c84017) passed on this firmware's first build. |
| P6 | Unit state before step 1 | (check) | `--info`: `FM-1_46001003  [running]`. In SAFE MODE: press PLAY ("try again") or power off/on first. |
| P7 | Fallback package | ok | M10.2, known good on this unit: `build/step-M10.2/fm1acc-0.10.3-m10.2.fwsc`, sha256 `0c38fc04c4cd7653fc0212101a4782c6f5869be7a132e10811a92fd476e0c320`. Behind it M10.1, M10, M9. Back to stock: `fm1_install.py backups/stock-firmware/FM-1_015.fwsc`. |
| P8 | Size | ok | App 554,980 B, **30,748 B free** of the app area (M10.2: 66,444 B; the pack is 25,874 B); RAM .data+.bss 89,936 B of 98,304 (8,368 B free). |

### P4b: the trimmed run, in numbers (docs/decisions-M10.4.md M10.4-05 / -06; times in build/step-M10.4/times.txt)

- Laptop tools (all pass): pytest tools/fm1song 298 passed; make_songs.py for all slugs changed no file; store_test, upreset_test, vsynth_test LEVELS=1 (POTATO after CRUSH 127: LEVEL 104 -> 105, **no change made**), builtin_test (57 built-ins identical to the compiled files, CRC and fields right, 2.2 million corrupt packs: errors only, no sanitizer report).
- Emulator: m104p1-p5 (a new FM-1 lists 15 songs / 42 backtracks, plays greensleeves, auld-lang-syne and amazing-grace sample-identically to the store's copies, two backtracks changed while playing; the store wins and delete brings the built-in back; M10.3's BACKTRACK and SONG LEARN changes), m5 store / flash / boot, m10 p2 / p3 / p5, m101 p3 / p6 (the whole fresh_install.sh on the emulator: 57 sent, the list still 15 songs and 42 backtracks, no duplicates). First run on package 4ccaea14: m5 store, m10p5, m101p3 failed (the crash and one stale expectation of mine). After the fix, **only the failed groups were re-run on this package: all pass**, plus m104p5 (its new help-screen check).
- Not run, and why: m51 (store and commands unchanged; m101p6 sends all 57 through the same path); m6-m9; m10p1/p4/p6/p7; m10_flash and m101_flash (presets untouched); song_fuzz (the parser is unchanged; the decoder has builtin_test); every-song play-throughs (pytest's goldens show no song changed).
- Screenshots looked at (docs/step-M10.4/): the splash (logo, no name line, "v0.10.5-m10.4" lower right); the mode screen (mascot, "KickSide - Your Musical SideKick" with Kick bold twice, the three modes); BACKTRACK's help (ten rows, SEQ "band kit", all text readable); the list footer.

---

## Checklist

### Step 1 - Install 0.10.5-m10.4 over M10.2

1. Claude Code runs `--info` (read-only) -> `FM-1_46001003  [running]`.
2. Check the package:
   ```bash
   sha256sum build/step-M10.4/fm1acc-0.10.5-m10.4.fwsc
   ```
   expect `769c87bfdf746dcb1aa39571ca40dbf4d58fbcfae66f1cd8a3f278cc3559cc43`.
3. Install (you run it; 1-2 minutes). It writes the app and the update staging area only:
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M10.4/fm1acc-0.10.5-m10.4.fwsc
   ```
   To undo: install M10.2's package (P7).
4. Claude Code runs `--info` -> `FM-1_46001005  [running]`. The splash shows your logo and `v0.10.5-m10.4`.

If it fails: install M10.2 again (P7) and report. **There is no step 2 to send.** `fm1song list` (read-only) can confirm the store is as it was.

### Step 2 - By hand: what to look at

| Your note | Where | What you should see |
|---|---|---|
| The LOOP label | BACKTRACK, structure LOOP, playing | Turn ALGORITHM to VERSE/CHORUS or FULL: the label leaves "LOOP" at once; the switch itself still happens at the phrase boundary |
| HOME from playing / paused | BACKTRACK's play screen | One tap of HOME while playing, in the count-in or paused: SETUP is up. SONG LEARN's HOME as before. HOME held 2 s: the mode screen |
| SEQ while playing | BACKTRACK, playing | SEQ steps STUDIO, ACOUSTIC, ELECTRO, CHIP at once; the **bass and harmony sounds change too** (at their next notes); the legend says "SEQ band kit" |
| "Band Kit" | BACKTRACK, SEQ | The pop-up title is "Band Kit" |
| The help screens (new check) | GLO on the song list, a song, BACKTRACK's SETUP and play screen | Each shows the CONTROLS table (ten rows) and no crash screen; GLO again: Settings |
| The list footer | SONG LEARN, the song list | After the last song: "Download and install more / songs from our website", small grey, never selected |
| The keys with a preset / with SONG | SONG LEARN, an open song, K4 | A preset: the keys play it; SONG: the keys sound like the song's melody instrument |
| The Start pop-up | SONG LEARN, ALGORITHM | "Start / BAR 17", and on a section's first bar its name under it in larger white type |
| POTATO | SYNTH, preset 5 | A crushed, lo-fi sound (CRUSH 127) |
| The splash and the version | Power on | Your logo, "v0.10.5-m10.4" small at the lower right |
| The mode screen | After the splash | The mascot and "KickSide - Your Musical SideKick" with "Kick" bold in both words |
| Pop-ups ON | Settings > Pop-ups | ON after this install |
| **The built-ins (optional)** | SONG LEARN | You run `tools/fm1song.sh delete oh-susanna`: **Oh! Susanna is still in the list** (now the built-in copy), but `tools/fm1song.sh list` no longer shows it (it shows only the store). `tools/fm1song.sh send songs/oh-susanna/` puts the store copy back. (This writes the store: undo = the send.) |

The empty screens ("Please visit our website / to download songs.") appear **only if the built-in pack fails** (or the store's flash is unusable); if you ever see them on this build, tell the planning chat.

**STOP.** The user installs; Claude Code runs only `--info` / `fm1song list` and records.

## Known

- Decisions for review: docs/decisions-M10.4.md, M10.4-01 to M10.4-06 (notably -03 the compression and the dictionary rule; -06 the crash found and fixed).
- `fm1song list` shows only the store, by design (the USB commands are unchanged); WIPE and the factory reset leave the built-ins.
