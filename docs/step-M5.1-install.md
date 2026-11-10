# Milestone M5.1 — the song-send fix: install on the FM-1

**Status: PREPARED, waiting for review** by the planning chat and the user (docs/plan-M5.1.md, step 4). Nothing has
been installed or sent. Claude Code ran no device command while preparing it. **The user runs every device command**
(install, `fm1song send` / `wipe`), one at a time, in their own terminal; Claude Code runs only the read-only ones
(`--info`, `fm1song list`) between steps and records them.

Prepared 2026-11-09 by Claude Code. Rules, pre-flight and restore paths as the approved M4 checklist
([step-M4-install.md](step-M4-install.md)) and M5's ([step-M5-early-install-2.md](step-M5-early-install-2.md)); what
differs is here.

---

## What went wrong on 2026-11-06, and the fix

- **Cause (reproduced in the emulator):** deleting a song's older copy wrote 4 zero bytes taken from the firmware's
  constants, which live in the flash. The flash driver switches the flash's read window (XIP) off while it writes, so
  the processor waited for bytes it could not get, with interrupts off, until the watchdog restarted the FM-1 (~8 s:
  "the FM-1 was disconnected"). Two such restarts soon after start-up → SAFE MODE. The first send that replaced one
  of your M4 songs hit it.
- **Why the tests missed it:** the emulator read the flash even while it was "off", and its flash writes took no time.
  Now (tools/patches/fm1-emu-flash-timing.patch) a read while it is off stalls the core as on the chip, erases take
  8 ms and page writes 2 ms (Puya P25Q-H typical), and every run reports the longest stretch without a watchdog feed.
  On the old build 0.5.3-m5p3 that emulator gives exactly the incident: `chip reset at 9826 ms (watchdog)`,
  `fm1song: the FM-1 was disconnected`.
- **The fix (0.5.4-m5p4):**
  - every flash write copies its source to RAM first;
  - making room (compacting) no longer happens inside a send: the FM-1 answers "making room" at once, compacts in
    small steps between screen updates ("MAKING ROOM step n of about m"), and `fm1song` sends again by itself;
  - the store keeps its table up to date instead of re-reading all 552 blocks after each step;
  - the watchdog is fed in every longer loop;
  - start-up still never reads the song store (only the song list does, after LEARN), and any content is tolerated;
  - new recovery action **WIPE SONG STORE** (below).

## What changes on the unit

| | Now | After |
|---|---|---|
| Firmware | 0.5.3-m5p3 **FM-1_46000503** (your trial) or M4 **FM-1_46000500** | **FM-1_46000504**, v0.5.4-m5p4 |
| Step 1 writes | — | The app area 0x4000–0x92FFF and the update staging area 0xE0000–0xE4FFF, by our update loader, as every install before. fm1-base (incl. the loader) unchanged since 98fc08c: `git diff 98fc08c -- firmware/vendor/fm1-base` is empty. |
| Steps 2–4 write | — | The **song store, 0x97000–0xDBFFF** only, as M5 (2.0 songs, 512-byte blocks, compacting). Unchanged kinds of write; the compaction now runs from the main loop instead of inside the send. |
| WIPE SONG STORE (new; only if you choose it) | — | Erases the 69 sectors of the song store, 0x97000–0xDBFFF, nothing else (checked before every erase; emulator: everything outside the area byte-identical afterwards). |
| Your songs now | M4's 9 + private ones, maybe leftovers of the crashed send | Read as before; the leftovers (blocks without a head) are taken back by the next compaction. |
| Settings | — | Kept (same record; a new row "Wipe song store" holds no value). |

## Pre-flight (any ❌ = stop)

| # | Check | State | Evidence |
|---|---|---|---|
| P1 | Off-laptop backup in `docs/device.md` | ✅ | Unchanged. |
| P2 | USB port + cable | ✅ | Port **1-2**, the new cable. A `-110` = stop and report. |
| P3 | Restore per D015 | ✅ | `firmware/vendor/fm1-base` unchanged since 98fc08c. |
| P4 | The package: commit + sha256 + emulator | ✅ | `build/step-M5.1/fm1acc-0.5.4-m5p4.fwsc`, sha256 `197b84a552100969ee727d451d018cd83eeed7a0e2bdb744740dd84e1f0a187d`, identity FM-1_46000504, built from commit 902eaa7. `nm`: no flash-resident write source left (`kill.zero` gone; `prog_ram` at 0x01C08DB8, RAM). **Full M5 suite on this exact package: 286 checks, ALL PASS** (`build/step-M5.1/emu-m5/report.md`). **M5.1 suite, timed flash:** **17 checks, ALL PASS** over 116 timed runs (`build/step-M5.1/emu-m51/report.md`): the incident's store, empty, full + fragmented, all 14 in one command and one by one, a power cut at 20 points with a boot after each, junk in the store, wipe three ways; 0 watchdog resets, 0 XIP reads while off. Laptop: `tools/store_test.sh` 35 checks ALL PASS (a power cut at every operation, the room made step by step, wipe, the store's table equal to a full rescan after each of 254 operations); pytest 93 pass; synth test ALL PASS; fuzz 10 min, 1,118,000 files, no sanitizer report. |
| P5 | Flash IDs | ✅ | In the M5 suite on this package: 0x856014 and 0xB36014 the same; unknown 0xC84017: `NO SONG STORAGE`, a send refused, nothing written. |
| P6 | Unit state before step 1 | (check) | `--info`: `FM-1_46000503  [running]` or `FM-1_46000500  [running]`. In SAFE MODE: press PLAY ("try again") or power off/on first. |
| P7 | Fallback package | ✅ | M4, known good: `build/step-M4/final/fm1acc-0.5.0-m4.fwsc`, sha256 `c811e0f7f002a2d7573843394e812490dbe3c2709c425d4b457c2351f767195b`. **Not 0.5.3-m5p3**: it has the bug. |
| P8 | The songs | ✅ | `songs/*/` 9 songs (2.0), `songs/private/*/` 5 (2.0, whole). All 14 fit: about 300 of 552 blocks free. |

## Budget

| | 0.5.3-m5p3 | 0.5.4-m5p4 |
|---|---|---|
| App image (slot ~581 KB) | 448,736 B | 451,744 B |
| RAM .data+.bss (of 98,304) | 76,032 | 76,344 |
| RAM pool (of 344,064) | 289,312 | 289,864 |
| Longest stretch without a watchdog feed while sending (timed emulator; limit 8,000 ms) | ~8,000 → reset | **71 ms** (longest over all 116 runs; fm1-base's own boot: 590 ms) |

---

## Checklist

### Step 1 — Install 0.5.4-m5p4

1. Claude Code runs `--info` (read-only) → `FM-1_46000503` or `FM-1_46000500`, `[running]`.
2. Check the package:
   ```bash
   sha256sum build/step-M5.1/fm1acc-0.5.4-m5p4.fwsc
   ```
   expect `197b84a552100969ee727d451d018cd83eeed7a0e2bdb744740dd84e1f0a187d`.
3. Install (you run it; 1–2 minutes):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M5.1/fm1acc-0.5.4-m5p4.fwsc
   ```
4. Claude Code runs `--info` → `FM-1_46000504  [running]`. The splash shows `v0.5.4-m5p4`, then the mode screen:
   PLAY for **LEARN**.
5. Your current songs are listed and play.

If it fails: install M4 (P7) and report.

### Step 2 — ONE song first

1. On the FM-1: **LEARN**, the song list. Claude Code runs `tools/fm1song.sh list` (read-only).
2. You send one song:
   ```bash
   tools/fm1song.sh send songs/amazing-grace
   ```
   expect `stored` (perhaps after "the FM-1 is making room ...": the FM-1 shows MAKING ROOM for a few seconds, then
   the laptop sends again by itself). **Stop** if it says "disconnected" or "stopped answering": don't retry;
   Claude Code runs `--info` and reads the crash log (read-only).
3. Open Amazing Grace on the FM-1 and play it a few bars (close the controls hint with any button first).

### Step 3 — The rest of ours

```bash
tools/fm1song.sh send songs/*/
```
Nine lines `stored` (Amazing Grace again: replaced). Claude Code runs `fm1song list`.

### Step 4 — The private songs

```bash
tools/fm1song.sh send songs/private/*/
```
Five lines `stored`, some after "making room". Claude Code runs `fm1song list` → **14 songs, each `ok`**, about 300 of
552 blocks free.

### Step 5 — On the unit (a few minutes)

1. Open a few songs, including a private one (Zombie), and play them; turn a level knob while playing.
2. Switch off and on: splash, mode screen, LEARN, the 14 songs.
3. **WIPE SONG STORE, look only:** GLO → Settings, SELECT to the last row "Wipe song store", PLAY: the question
   appears. Press **HOME**: back, nothing erased. (Holding PLAY 2 s there would erase every song: don't, unless you
   want to.)
4. Optional: switch off, hold **GLO + HOME** while switching on, release: the same question before anything else.
   Press HOME: the mode screen, the songs kept.

### If anything goes wrong with the store

- The songs look damaged or a send keeps failing: wipe and send again. On the FM-1 (Settings > Wipe song store,
  hold PLAY 2 s) or from the laptop (it asks for `--yes`; erases the song area 0x97000–0xDBFFF only):
  ```bash
  tools/fm1song.sh wipe --yes
  ```
  then steps 2–4 again.
- To go back: install M4 (P7), then send our songs again (M4 gets 1.3; the private songs whole don't fit M4):
  ```bash
  .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M4/final/fm1acc-0.5.0-m4.fwsc
  ```
  ```bash
  tools/fm1song.sh send songs/*/
  ```

## Restore paths (D015, unchanged)

As [step-M3-install.md](step-M3-install.md).

## After the user has run it

Claude Code records the outputs, identities, `fm1song list` output and impressions in `docs/LOG.md`, and the
firmware and songs in `docs/device.md`.
