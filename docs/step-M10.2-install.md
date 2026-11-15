# Milestone M10.2: install on the FM-1

**Status: PREPARED, waiting for the user.** Nothing has been installed or sent. Claude Code ran no device command while preparing it (docs/plan-M10.2.md).
**The user runs every device command**, one at a time, in their own terminal. Claude Code runs only the read-only ones (`--info`, `fm1song list`) between steps and records them.

Prepared 2026-11-14 by Claude Code (docs/plan-M10.2.md; your notes on M10.1). Rules, pre-flight and restore paths as the approved M10 / M10.1 checklists
([step-M10-install.md](step-M10-install.md), [step-M10.1-install.md](step-M10.1-install.md)); what differs is here. **No factory reset is needed**: your saved presets and settings stay.

## What is new (docs/controls.md, "M10.2"; decisions docs/decisions-M10.2.md)

- SONG LEARN: "Complexity" in the K1-K4 pop-ups; **"Track Selected"** (+ "(press again to control volume level)"); the stopped screen without the song title and a more readable start line; the
  "THE LAYER BUTTONS" screen is gone.
- **ALGORITHM alone sets the start** (the pop-up names the section); **PRESETS is the key** (the SAVE / SELECT key mode is gone); a start changed while paused: PLAY goes on from the new start.
- **Settings > Pop-ups** ON / OFF (the old "Controls hint" row). **Pop-ups is ON after this update and after every later one.**
- The mode screen ignores the knobs; PLAY is lit on BACKTRACK's SETUP; EDS and NIGHT DRIVE swapped their sounds; Oh! Susanna's chorus bars 2 and 6 are "D A7".
- The count-in shows the beat in the bar (1 2 3 4 | 1 2 3), not the beats left.

## What changes on the unit

| | Now | After |
|---|---|---|
| Firmware | M10.1 **FM-1_46001002** | **FM-1_46001003**, v0.10.3-m10.2 (the splash shows "KickSide - Your Musical SideKick" and "m10.2") |
| Step 1 writes | — | The app area 0x4000–0x92FFF and the update staging area 0xE0000–0xE4FFF, by our update loader, as every install before. fm1-base (the loader) is **unchanged** since M10.1 (`git diff db81fa8 HEAD -- firmware/vendor` is empty). |
| The settings record (0xFC000 / 0xFD000) | M10.1's | Written **once at the first start** (the firmware marker; Pop-ups set ON); the record's layout and size are unchanged. |
| Step 2 writes | — | One song in the song store (oh-susanna replaces its old copy; the store may say "making room" once: send again). |
| Your saved presets | U-slots | Kept (no preset code changed; only the stock EDS and NIGHT DRIVE rows swapped their sounds). |

## Pre-flight (any ❌ = stop)

| # | Check | State | Evidence |
|---|---|---|---|
| P1 | Off-laptop backup in `docs/device.md` | ✅ | Unchanged. |
| P2 | USB port + cable | ✅ | Port **1-2**. A `-110` beyond the usual one = stop and report. |
| P3 | Restore per D015 | ✅ | `firmware/vendor/fm1-base` unchanged since M10.1. |
| P4 | The package: commit + sha256 | ✅ | `build/step-M10.2/fm1acc-0.10.3-m10.2.fwsc` (built with `firmware/build.sh --release 0.10.3-m102`, copied to this name), sha256 `0c38fc04c4cd7653fc0212101a4782c6f5869be7a132e10811a92fd476e0c320`, identity **FM-1_46001003**, from commit abbfc72 (a clean tree: only the planning chat's HANDOFF.md / plan-M10.2.md and your untracked `import/`), the same sha256 in two builds. |
| P4b | **Emulator** | ⚠️ **not run** | CLAUDE.md says every firmware runs in the emulator before it goes near hardware. **You chose no test cycle for M10.2** (no suite, no fast lane, no emulator runs): this package has only been **built** (no new warnings; app 519,284 B, 66,444 B free; RAM .data+.bss 89,120 B of 98,304), and `pytest tools/fm1song` (298 passed) and the song check ran. The emulator is the check to run first if you want one (`tools/m101_test.py` / a short manual run), or accept the risk knowingly. |
| P5 | Flash IDs | ✅ | The app's and the loader's flash-ID code is fm1-base's (0x856014 and 0xB36014 accepted, others refused), **unchanged since M10.1**; M10.2 changes nothing there. |
| P6 | Unit state before step 1 | (check) | `--info`: `FM-1_46001002  [running]`. In SAFE MODE: press PLAY ("try again") or power off/on first. |
| P7 | Fallback package | ✅ | M10.1, known good on this unit (installed and used): `build/step-M10.1/fm1acc-0.10.2-m10.1.fwsc`, sha256 `7ff839308734740be83175a204f204ab46702a9fbb4a943aa63548c9db77b5af`. The settings record is the same layout (M10.1 reads it; it will just see a different marker bytes in the spare fields: harmless). Behind it M10, M9. Back to stock any time: `fm1_install.py backups/stock-firmware/FM-1_015.fwsc`. |
| P8 | The song | ✅ | Only oh-susanna's files differ (`make_songs.py` for all slugs changes nothing else; pytest 298 passed). |

---

## Checklist

### Step 1 — Install 0.10.3-m10.2 over M10.1

1. Claude Code runs `--info` (read-only) → `FM-1_46001002  [running]`.
2. Check the package:
   ```bash
   sha256sum build/step-M10.2/fm1acc-0.10.3-m10.2.fwsc
   ```
   expect `0c38fc04c4cd7653fc0212101a4782c6f5869be7a132e10811a92fd476e0c320`.
3. Install (you run it; 1–2 minutes). It writes the app and the update staging area only:
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M10.2/fm1acc-0.10.3-m10.2.fwsc
   ```
4. Claude Code runs `--info` → `FM-1_46001003  [running]`. The splash shows `KickSide - Your Musical SideKick` and `m10.2`.

If it fails: install M10.1 again (P7) and report.

### Step 2 — Send Oh! Susanna again (writes the song store; you run it)

In SONG LEARN (a send is refused in SYNTH and BACKTRACK):

```bash
tools/fm1song.sh send songs/oh-susanna/
```

It replaces the old copy (to undo: send the old version from git, or run a clean install). Then Claude Code runs `fm1song list` (read-only): every song `ok`.

**Pop-ups is ON after this (and every) update**: the first start after an install stores the firmware's marker and sets Pop-ups ON; a choice of OFF in Settings lasts until the next update.

### Step 3 — By hand: what to look at (one line per item of your M10.1 notes)

| Your note | Where | What you should see |
|---|---|---|
| Complexity | SONG LEARN, K1-K4 | "Drums / Bass / Harmony / Melody Complexity Level", value "2 of 3" ...; the title whole, in a smaller type if it must |
| Track Selected | SONG LEARN, FX / SEL / ENV / LFO | First press on another layer: "Track Selected", the layer name, and "(press again to control volume level)" under it; press again: "Bass Volume QUIET" ... as before. BACKTRACK's LFO: "Track Selected / Melody", no second line |
| Stopped play screen | SONG LEARN, an opened song | No title above PRESS PLAY (the title is at the top); PRESS PLAY higher; "from bar 9 (Chorus 1)" under it, larger and brighter |
| THE LAYER BUTTONS | Opening a song | The screen no longer appears: the play screen at once |
| Start = ALGORITHM | SONG LEARN | ALGO turns the start a bar a click: "Start / BAR 9", and "Chorus 1" under it when it is a section's first bar; the screen says "from bar 9 (Chorus 1)" |
| Key = PRESETS | SONG LEARN | PRESETS turns the key: "Key / E (+2)", at 0 "D (song key)"; SAVE does nothing but open the controls pop-up (no key mode); SELECT: tempo / zoom |
| Loop A / B | SONG LEARN, SEQ | While setting a loop PRESETS still snaps A / B to a section |
| Paused, new start | SONG LEARN, PLAY then PLAY (pause), ALGO | The pause screen says "PLAY: from bar N"; PLAY starts there, not where it paused. Without ALGO: PLAY goes on as before |
| Help and legends | GLO; the stopped bottom row | PRESETS "key", ALGORITHM "start: bar; loop A/B", no SAVE; the stopped row: three pages, 6 s each ("PLAY\|ALGO start\|PRESETS key\|GLO help", the layer buttons, "SEQ loop\|SELECT tempo\|EDIT view\|GLO help") |
| Pop-ups ON / OFF | Settings (GLO, then the row "Pop-ups") | ON after this install. OFF: no value box in any of the three modes; SYNTH's bottom row shows the turned value; "SAVED U05" still appears; a button with no job does nothing. After the next update it is ON again |
| Mode screen knobs | The mode screen | Turn K1-K4, PRESETS, ALGO: nothing happens afterwards in SYNTH (K3 no longer moves the reverb) |
| PLAY on SETUP | BACKTRACK's setup page | PLAY is lit (full) when there are backtracks |
| EDS / NIGHT DRIVE | SYNTH, no. 6 and no. 21 | EDS sounds as NIGHT DRIVE did and NIGHT DRIVE as EDS did (names and positions unchanged) |
| Oh! Susanna | SONG LEARN, after step 2 | The chorus's 2nd and 6th bars play D then A7 (no G clash) |
| Count-in | Any song, ARP (count-in on), PLAY | The number is the beat in the bar: 1 2 3 4 | 1 2 3 for Auld Lang Syne / Saints, 1 2 3 4 for a song without a pickup, 1 2 3 for waltzes; the clicks as before |

**STOP.** The user installs; Claude Code runs only `--info` / `fm1song list` and records.

## Known

- **No emulator run and no test run** on this package (your choice, D069). The tests that name changed text or keys were updated without running them (decisions M10.2-01, -04, -06); the older tools/m2_test.py and m4_test.py still contain the removed SAVE + SELECT key sequences.
- **Decisions for review**: docs/decisions-M10.2.md, M10.2-01 to M10.2-08 (your wish for "Pop-ups ON after every update" is a marker of FW_VERSION in the settings record's spare bytes, M10.2-05).
