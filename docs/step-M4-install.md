# Milestone M4 — install on the FM-1 (checklist)

**Status: PREPARED, WAITING FOR REVIEW** by the planning chat and the user. This is the final M4 checklist (parts
1–6). It replaces the early one ([step-M4-early-install.md](step-M4-early-install.md)) unless the user has already
run that; then step 1 installs over 0.4.4-m4p4 instead of M3, with the same command and checks. Nothing has been
installed or sent. **The user runs every device command** (install, `fm1song send` / `delete`). Claude Code runs
only the read-only ones (`--info`, `fm1song list`, CLAUDE.md) between steps and records.

Prepared 2026-11-06 by Claude Code. The rules, pre-flight and restore paths are the same as the approved and
completed M3 checklist ([step-M3-install.md](step-M3-install.md)). Emulator report:
[step-M4-emulator-report.md](step-M4-emulator-report.md). Importing songs: [import.md](import.md).

---

## What changes on the unit

| | Now | After |
|---|---|---|
| Firmware | M3, **FM-1_46000400**, v0.4.0-m3 (or 0.4.4-m4p4) | M4, **FM-1_46000500**, v0.5.0-m4 |
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
| P4 | The package: commit + sha256 + emulator | ✅ | `build/step-M4/final/fm1acc-0.5.0-m4.fwsc`, sha256 `c811e0f7f002a2d7573843394e812490dbe3c2709c425d4b457c2351f767195b`, identity FM-1_46000500, from the Part 6 commit (firmware source = Part 4's with the version). Emulator: the **full suite** on this exact package, `build/step-M4/final/report.md`: **258 checks, ALL PASS** (all 9 songs to the end on time, the store with power cuts, flash ids, settings over a restart, M4 parts 1–5 incl. the 5 private songs). Parser fuzz 10 min: 968,000 files, no report |
| P5 | Flash IDs | ✅ | Emulator with 0x856014 and 0xB36014: same screens, sound, songs stored. With the unknown 0xC84017: `NO SONG STORAGE`, the keys sound, a send is refused, no crash. |
| P6 | Unit state before step 1 | (check) | `--info` must print `FM-1_46000400  [running]` (M3), or `FM-1_46000404` after the early checklist. |
| P7 | Fallback package | ✅ | M3, known good on this unit: `build/step-M3/final/fm1acc-0.4.0-m3.fwsc`, sha256 `7bb7e58ad8c27cb8eb4038fef641f02a708fa01344fd856931cf1280d027787d`. M3 cannot read 1.3: after a fallback, send `songs/*/` again; it then gets 1.2 with the first 3 levels (tested in the emulator). |
| P8 | The songs to send | ✅ | `songs/*/`: 9 songs, all regenerated (format 1.3), 168 KB (45 of 69 sectors). **Your 5 private test songs** (`songs/private/*/`, 41–47 KB each) **don't all fit beside them**: about 2 do. Step 2 below. |

## Rules for every step

As M3's ([step-M3-install.md](step-M3-install.md), "Rules for every step"), with one difference: `fm1song list`
is now Claude Code's to run (read-only, CLAUDE.md).

---

## Checklist

### Step 1 — Install M4 over M3

1. Claude Code runs `--info` (read-only) → `FM-1_46000400  [running]`.
2. The package is the reviewed one:
   ```bash
   sha256sum build/step-M4/final/fm1acc-0.5.0-m4.fwsc
   ```
   expect `c811e0f7f002a2d7573843394e812490dbe3c2709c425d4b457c2351f767195b`.
3. Install (you run it; about 1–2 minutes):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M4/final/fm1acc-0.5.0-m4.fwsc
   ```
4. Claude Code runs `--info` → expect `FM-1_46000500  [running]`. The splash shows `v0.5.0-m4`.

If it fails: as M3's step 1 ("If it fails"): install M3 (P7) or stock (path A) and report.

### Step 2 — Send the songs (format 1.3)

1. Claude Code runs `tools/fm1song.sh list` → `FM-1_46000500`, `reads format 1.0..1.3`.
2. You send all nine (writes the song store only):
   ```bash
   tools/fm1song.sh send songs/*/
   ```
   expect nine lines `stored` and no "sending 1.2" line.
3. Claude Code runs `fm1song list` → 9 songs, each `ok`, about 24 sectors free.
4. **Your private test songs** (Private test folder). They don't all fit beside the 9: pick one or two (each takes
   10–12 sectors), for example:
   ```bash
   tools/fm1song.sh send songs/private/let-it-be/ songs/private/true-colors/
   ```
   To try others, make room first, e.g. `tools/fm1song.sh delete happy-birthday` (you run it; it erases that song's
   sectors only). Notes on the imports (docs/LOG.md, M4 Part 5):
   - Creep, Rockin' in the Free World and Zombie are **cut to their first 56, 64 and 80 bars**, so the whole level
     set fits the FM-1.
   - Zombie's "melody" is its lead guitar (the file has no vocal).
   - The chord charts are proposals, worth checking by ear.

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

**Part 5: your imported songs**
15. Open the **Private test** folder (HOME on the list). Play each song you sent:
    - Does the melody follow the file? Are the chords right?
    - Which level is "from the file" (the report)? Compare it with the generated ones, e.g. K3 through L1–L5.
    - Mute the melody (press LFO twice) and play along.

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
