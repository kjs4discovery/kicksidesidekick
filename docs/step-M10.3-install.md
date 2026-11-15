# Milestone M10.3: install on the FM-1

**Status: PREPARED, waiting for the user.** Nothing has been installed or sent. Claude Code ran no device command while preparing it (docs/plan-M10.3.md).
**The user runs every device command**, one at a time, in their own terminal. Claude Code runs only the read-only ones (`--info`, `fm1song list`) between steps and records them.

Prepared 2026-11-15 by Claude Code (docs/plan-M10.3.md; your notes on M10.2). Rules, pre-flight and restore paths as the approved M10 / M10.1 / M10.2 checklists
([step-M10-install.md](step-M10-install.md), [step-M10.1-install.md](step-M10.1-install.md), [step-M10.2-install.md](step-M10.2-install.md)); what differs is here.
**No factory reset is needed and nothing is sent: no song changed.** Your saved presets and settings stay.

## What is new (docs/controls.md "M10.3", docs/reference.md; decisions docs/decisions-M10.3.md)

- BACKTRACK: the section indicator leaves LOOP at once; HOME from playing or paused goes straight to SETUP; **SEQ steps the band kit while it plays** (the bass and harmony sounds change too); the pop-up says **"Band Drum Kit"**.
- SONG LEARN: a footer at the end of the song list; the empty screens point to the website; **the keys play the melody's sound**; the Start pop-up names the section in a larger type.
- POTATO: CRUSH 127.
- The new splash (your logo, the version at the lower right) and the mode screen's mascot with a bold "Kick".
- Open source files (LICENSE, LICENSES/, SPDX lines, REUSE.toml, NOTICE): no effect on the unit.
- **Pop-ups is ON again after this update** (as M10.2 designed: the settings record keeps a marker of the firmware version).

## What changes on the unit

| | Now | After |
|---|---|---|
| Firmware | M10.2 **FM-1_46001003** | **FM-1_46001004**, v0.10.4-m10.3 (the splash shows your logo and "v0.10.4-m10.3") |
| Step 1 writes | — | The app area 0x4000–0x92FFF and the update staging area 0xE0000–0xE4FFF, by our update loader, as every install before. fm1-base (the loader) is **unchanged** since M10.1 (`git diff db81fa8 HEAD -- firmware/vendor` is empty). |
| The settings record (0xFC000 / 0xFD000) | M10.2's | Written **once at the first start** (the firmware marker; Pop-ups set ON); layout and size unchanged. |
| The song store | — | **Not written** (no step 2). |
| Your saved presets | U-slots | Kept (no preset code changed; only the stock POTATO row has CRUSH 127: a copy you saved earlier stays as it was). |

## Pre-flight (any ❌ = stop)

| # | Check | State | Evidence |
|---|---|---|---|
| P1 | Off-laptop backup in `docs/device.md` | ✅ | Unchanged. |
| P2 | USB port + cable | ✅ | Port **1-2**. A `-110` beyond the usual one = stop and report. |
| P3 | Restore per D015 | ✅ | `firmware/vendor/fm1-base` unchanged since M10.1. |
| P4 | The package: commit + sha256 | ✅ | `build/step-M10.3/fm1acc-0.10.4-m10.3.fwsc` (built with `firmware/build.sh --release 0.10.4-m103`, copied to this name), sha256 `59ea0e6d012cd4715067664b70a9bfd226234ea024f2968dba752cefce3c3234`, identity **FM-1_46001004**, from commit b13125b (a clean tree: only your untracked `import/`), the same sha256 in two builds. |
| P4b | **Emulator** | ⚠️ **not run** | CLAUDE.md says every firmware runs in the emulator before it goes near hardware. **You chose no test cycle for M10.3** (plan, D072): this package has only been **built** (no new warnings: the two in the log, bt.c:234 and EAS's unused label, are older), and `pytest tools/fm1song` (298 passed) and the song check (make_songs.py for all slugs changes no file) ran. The emulator is the check to run first if you want one, or accept the risk knowingly. |
| P5 | Flash IDs | ✅ | The app's and the loader's flash-ID code is fm1-base's (0x856014 and 0xB36014 accepted, others refused), **unchanged since M10.1**; M10.3 changes nothing there. |
| P6 | Unit state before step 1 | (check) | `--info`: `FM-1_46001003  [running]`. In SAFE MODE: press PLAY ("try again") or power off/on first. |
| P7 | Fallback package | ✅ | M10.2, known good on this unit (installed and used): `build/step-M10.2/fm1acc-0.10.3-m10.2.fwsc`, sha256 `0c38fc04c4cd7653fc0212101a4782c6f5869be7a132e10811a92fd476e0c320`. Behind it M10.1, M10, M9. Back to stock any time: `fm1_install.py backups/stock-firmware/FM-1_015.fwsc`. |
| P8 | Size | ✅ | App 523,444 B (M10.2: 519,284 B), 62,284 B free (M10.2: 66,444 B; the new artwork is about +5.5 KB); RAM .data+.bss 89,152 B of 98,304. |

---

## Checklist

### Step 1 — Install 0.10.4-m10.3 over M10.2

1. Claude Code runs `--info` (read-only) → `FM-1_46001003  [running]`.
2. Check the package:
   ```bash
   sha256sum build/step-M10.3/fm1acc-0.10.4-m10.3.fwsc
   ```
   expect `59ea0e6d012cd4715067664b70a9bfd226234ea024f2968dba752cefce3c3234`.
3. Install (you run it; 1–2 minutes). It writes the app and the update staging area only:
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M10.3/fm1acc-0.10.4-m10.3.fwsc
   ```
4. Claude Code runs `--info` → `FM-1_46001004  [running]`. The splash shows your logo and `v0.10.4-m10.3` at the lower right.

If it fails: install M10.2 again (P7) and report.

**There is no step 2**: no song changed, so nothing is sent. (`fm1song list`, read-only, can confirm the store is as it was.)

### Step 2 — By hand: what to look at (one line per note of your M10.2 notes)

| Your note | Where | What you should see |
|---|---|---|
| The LOOP label | BACKTRACK, structure LOOP, playing | Turn ALGORITHM to VERSE/CHORUS or FULL: the top-right label leaves "LOOP" at once and names the section playing (blinking in the last bar before a change); the switch itself still happens at the phrase boundary |
| HOME from playing / paused | BACKTRACK's play screen | One tap of HOME while playing, in the count-in, or paused: the music stops and SETUP is up. SONG LEARN's HOME is as before (playing: back to the start point; stopped: the list). HOME held 2 s: the mode screen |
| SEQ while playing | BACKTRACK, playing | SEQ steps STUDIO, ACOUSTIC, ELECTRO, CHIP at once: the drums change from the next hit, the bass and harmony sounds change at their next notes; the legend says "SEQ band kit"; the help (GLO) has a SEQ row (the button list is now ten rows) |
| "Band Drum Kit" | BACKTRACK, SEQ | The pop-up title is "Band Drum Kit" (the SETUP row still says KIT) |
| The list footer | SONG LEARN, the song list | After the last song: "Download and install more / songs from our website", grey, small, never selected; with the cursor on the last song it is fully visible; a short list shows it under the last song. The folders screen has none; Pop-ups OFF does not hide it |
| The empty screens | SONG LEARN with no songs; BACKTRACK with no backtracks | "Please visit our website / to download songs." — **only seen after a factory reset (or a wipe), so you will probably not see them now** |
| The keys with a preset | SONG LEARN, an open song, K4 to a preset | The keys play that preset (four voices shared with the melody); releasing a key after switching K4 back to SONG does not leave a note hanging |
| The keys with SONG | SONG LEARN, K4 on SONG | The keys sound like the song's melody instrument (it changes with the song); in the list and the folders: Electric Piano 1 as before; BACKTRACK as before |
| The Start pop-up | SONG LEARN, ALGORITHM | "Start / BAR 9", and on a section's first bar the section's name under it in the larger white type ("Chorus 1"); other pop-ups ("Track Selected") keep their small line |
| POTATO | SYNTH, preset 5 | A crushed, lo-fi sound (CRUSH 127, on FX 2's fourth value); nothing else in the row changed |
| The splash and the version | Power on | Your logo on the cream background, no name line (it is in the logo), "v0.10.4-m10.3" small at the lower right |
| The mode screen | After the splash | The mascot (full size, 1 : 1) and the title "KickSide - Your Musical SideKick" with "Kick" bold in both words; "modes:" and the three choices under it |
| Pop-ups ON | Settings > Pop-ups | ON after this install (as after every update) |

**STOP.** The user installs; Claude Code runs only `--info` / `fm1song list` and records.

## Known

- **No emulator run and no test run** on this package (your choice, D072). The tests that name changed text or keys were updated without running them (decisions M10.3-05 and the list at the end of docs/decisions-M10.3.md).
- **Scope**: docs/plan-M10.3.md (the newest) was followed: the built-in starter library and the trimmed final test run named in your prompt and in an older HANDOFF note are **not** in this build (decision M10.3-01).
- **Decisions for review**: docs/decisions-M10.3.md, M10.3-01 to M10.3-08.
