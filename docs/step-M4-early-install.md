# Milestone M4, parts 1–4 — install on the FM-1 (early checklist)

**Status: PREPARED, WAITING FOR REVIEW** by the planning chat and the user (docs/plan-M4.md: the early install
point, so that an installable build with parts 1–4 exists even if the work stops later). Nothing has been installed
or sent. Claude Code ran no device command while preparing it. **The user runs every device command** (install,
`fm1song send`), one at a time, in their own terminal. Claude Code runs only the read-only ones (`--info`,
`fm1song list`, CLAUDE.md) between steps and records.

Prepared 2026-11-06 by Claude Code. The rules, pre-flight and restore paths are the same as the approved and
completed M3 checklist ([step-M3-install.md](step-M3-install.md)); what differs is spelled out here. The final M4
checklist (`docs/step-M4-install.md`, parts 1–6, with the MIDI import and your private test songs) comes later.

---

## What changes on the unit

| | Now | After |
|---|---|---|
| Firmware | M3, **FM-1_46000400**, v0.4.0-m3 | M4 parts 1–4, **FM-1_46000404**, v0.4.4-m4p4 |
| Step 1 writes | — | The app area 0x4000–0x92FFF and the update staging area 0xE0000–0xE4FFF, by our update loader, exactly as before. The loader is unchanged since Step 0.7: `git diff 98fc08c -- firmware/vendor` is empty. |
| Step 2 writes | — | The **song store, 0x97000–0xDBFFF**, by our firmware when `fm1song send` asks. The songs are now **format 1.3**: harmony 5 levels, bass 4, drums 3. The 9 songs take about 168 KB (45 of 69 sectors). |
| Settings | — | The same two sectors as M3 (0xFC000–0xFDFFF), written only when you leave the settings after a change. **M4's settings record is larger** (it adds the hidden views), so the M3 copy is not read and **the defaults are used once**: count-in MEDIUM, quiet level 50 %, key lights ON. Set yours again if you had changed them. |

Nothing here is a new kind of write.

## Pre-flight (any ❌ = stop)

| # | Check | State | Evidence |
|---|---|---|---|
| P1 | Off-laptop backup recorded in `docs/device.md` | ✅ | Unchanged. |
| P2 | USB port + cable | ✅ | Port **1-2**, the new cable. A `-110` = stop and report. |
| P3 | Restore per D015 | ✅ | `firmware/vendor` unchanged since 98fc08c. |
| P4 | The package: commit + sha256 + emulator | ✅ | `build/step-M4/part4/fm1acc-0.4.4-m4p4.fwsc`, sha256 `521a5e9dad668266bc16dff39cdc15c0650213ba4d8e959dc565d180bdc4bb7c`, identity FM-1_46000404, from the Part 4 commit. Emulator: the **full suite** on this exact package, `build/step-M4/part4/report.md`: **246 checks, all pass** (245 in the run; one M3-era Part 1 loop script, still pressing SEQ in M3's order, was rewritten for M4's cycle and then passed on this same package, `build/step-M4/part4/emu-part1/`). |
| P5 | Flash IDs | ✅ | Emulator with 0x856014 and 0xB36014: same screens, sound, songs stored. With the unknown 0xC84017: `NO SONG STORAGE`, the keys sound, a send is refused, no crash. |
| P6 | Unit state before step 1 | (check) | `--info` must print `FM-1_46000400  [running]` (M3). |
| P7 | Fallback package | ✅ | M3, known good on this unit: `build/step-M3/final/fm1acc-0.4.0-m3.fwsc`, sha256 `7bb7e58ad8c27cb8eb4038fef641f02a708fa01344fd856931cf1280d027787d`. M3 cannot read 1.3: after a fallback, send `songs/*/` again; it then gets 1.2 with the first 3 levels (tested in the emulator). |
| P8 | The songs to send | ✅ | `songs/*/`: 9 songs, all regenerated (format 1.3). |

## Rules for every step

As M3's ([step-M3-install.md](step-M3-install.md), "Rules for every step"), with one difference: `fm1song list`
is now Claude Code's to run (read-only, CLAUDE.md).

---

## Checklist

### Step 1 — Install M4 parts 1–4 over M3

1. Claude Code runs `--info` (read-only) → `FM-1_46000400  [running]`.
2. The package is the reviewed one:
   ```bash
   sha256sum build/step-M4/part4/fm1acc-0.4.4-m4p4.fwsc
   ```
   expect `521a5e9dad668266bc16dff39cdc15c0650213ba4d8e959dc565d180bdc4bb7c`.
3. Install (you run it; about 1–2 minutes):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M4/part4/fm1acc-0.4.4-m4p4.fwsc
   ```
4. Claude Code runs `--info` → expect `FM-1_46000404  [running]`. The splash shows `v0.4.4-m4p4`.

If it fails: as M3's step 1 ("If it fails"): install M3 (P7) or stock (path A) and report.

### Step 2 — Send the songs (format 1.3)

1. Claude Code runs `tools/fm1song.sh list` → `FM-1_46000404`, `reads format 1.0..1.3`.
2. You send all nine (writes the song store only):
   ```bash
   tools/fm1song.sh send songs/*/
   ```
   expect nine lines `stored` and no "sending 1.2" line.
3. Claude Code runs `fm1song list` → 9 songs, each `ok`.

### Step 3 — On the unit (no command; about 40 minutes)

**Part 1**
1. **Count-in:** MEDIUM by default.
2. **Loops:** SEQ once: `LOOP SONG`, and at the end the song goes round again. SEQ: set A; SEQ: set B; SEQ: off.
3. **Help on the list:** GLO says `HOME folders`.

**Part 2: your part (D027)**
4. A song opens with HARM as your part: the amber bar on the HARM box. Its keys light while it plays, **not
   muted**.
5. **Press** ENV (harmony): `L1 QUIET` (half volume); again: `MUTE`; again: `L1 FULL`.
6. **Hold** SEL (bass) about a second: `BASS - YOURS`. The screen, cue and keys follow the bass, even unmuted.
   Hold FX: drums yours, no key lights.
7. **Settings:** "Quiet level" 25 % / 50 %; "Key lights" OFF / ON.

**Part 3: views**
8. Harmony, EDIT to the **notation**: a grand staff with the chords stacked and chord names above. Try K3 at each
   level.
9. Drums yours: only the **drum grid** and the **drum notation** (kick low, snare in the middle, cymbals as x).
10. **Settings → Views:** hide, e.g., harmony KEYBOARD and CHART. EDIT now skips them. A layer's last view can't be
    hidden. The setting is kept over power off.

**Part 4: levels**
11. **K3 (harmony) 1 to 5,** on Amazing Grace (3/4) and Danny Boy (slow 4/4): held, rhythm, inversions, broken
    chords, arpeggios.
    - At L4 and L5 the FM-1's keys light each note as it's played, with the chord around it dimmer.
    - Is each level clearly a step up?
12. **K2 (bass) 1 to 4:** held, rhythm, root–fifth, walking. **K1 (drums) 1 to 3:** pulse, groove, with fills.
13. **Amazing Grace** is regenerated by the new generators; its melody is unchanged.
14. **Record** CPU and voices (GLO) on Oh! Susanna at 250 BPM with harmony L5, bass L4, drums L3, and your
    impressions.

If it misbehaves: note what and where. To go back, you install M3 (P7):
```bash
.venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M3/final/fm1acc-0.4.0-m3.fwsc
```
Then you send the songs again; M3 gets 1.2 with 3 levels:
```bash
tools/fm1song.sh send songs/*/
```

---

## Restore paths (D015, unchanged)

As [step-M3-install.md](step-M3-install.md).

## After the user has run it

Claude Code records the outputs, identities, the `fm1song list` output, readings and impressions in `docs/LOG.md`,
and the firmware, songs and settings in `docs/device.md`.
