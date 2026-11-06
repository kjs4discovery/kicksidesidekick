# Milestone M3, parts 1–5 — install on the FM-1 (early checklist)

**Status: SUPERSEDED** (2026-11-05, the planning chat: the final M3 checklist,
[step-M3-install.md](step-M3-install.md), is approved; this early one is not needed). Was: prepared, waiting for review by the planning chat and the user (docs/plan-M3.md: the early install
point, so that an installable build with parts 1–5 exists even if the work stops later). Nothing has been installed
or sent. Claude Code ran no device command while preparing it (the FM-1 was not plugged in). **The user runs every
device command**, one at a time, in their own terminal, and pastes each step's output to Claude Code before the
next.

Prepared 2026-11-05 by Claude Code. The rules, pre-flight and restore paths are the same as the approved and
completed M2 checklist ([step-M2-install.md](step-M2-install.md)); what differs is spelled out here. The final M3
checklist (`docs/step-M3-install.md`, parts 1–9) comes later. Run this one only if you want parts 1–5 on the unit
before then.

---

## What changes on the unit

| | Now | After |
|---|---|---|
| Firmware | M2, **FM-1_46000300**, v0.3.0-m2 | M3 parts 1–5, **FM-1_46000305**, v0.3.5-m3p5 |
| Step 1 writes | — | The app area 0x4000–0x92FFF and the update staging area 0xE0000–0xE4FFF, by our update loader, exactly as the M1 and M2 installs. The loader's code is unchanged since Step 0.7: `git diff 98fc08c -- firmware/vendor` is empty. |
| Step 2 writes | — | The **song store, 0x97000–0xDBFFF**, by our firmware when `fm1song send` asks. This is the same kind of write as M2's song send. |
| Step 3 writes (**new kind of write**) | — | The **settings, 0xFC000–0xFDFFF** (two 4 KiB sectors), by our firmware, only when you leave the new settings screen after changing something. |

### The settings save (new), explained

- **What it is.** M3 has a settings screen (GLO, then GLO again): key lights blink, count-in length, count-in
  volume, note names. When you leave it (GLO or HOME) after changing a value, the firmware saves the settings with
  fm1-base's own settings store (`plat_store_save(OBJ_SETTINGS)`). Nothing is saved if nothing changed. No reboot,
  no update mode, no loader.
- **Where.** Two copies at 0xFC000 and 0xFD000 (fm1-base's A/B scheme, `firmware/vendor/fm1-base/device/storage.c`).
  A save goes to the copy that is not current: erase that one sector, write the data, then the header last. On
  start-up the newer valid copy wins, so a save that is cut off leaves the previous settings in charge. These two
  sectors are in our app's store map (app.h `APP_STORE_MAP`, unchanged since M1).
  - **Not touched:** everything else, including the song store, the panel calibration (0xDC000), the update staging
    area and M-VAVE's data.
  - **Stock never uses these sectors:** in our unit's full stock dump (backups/flash/FM-1_015_full-flash_856014_2026-11-04.bin)
    0xFC000–0xFFFFF is all 0xFF.
- **If it is cut off:** the previous settings (or the defaults) are used. Nothing else can be affected.
- **How to undo.** Change the values back on the settings screen. M2 (the fallback) and stock never read these
  sectors. Path B2 (the full dump) brings them back to 0xFF.
- **Tested** in the emulator: the settings are saved once, used, and still in use after a restart from the same
  flash. The flash ids 856014, b36014 and an unknown one were tested. With an unknown chip the screen says
  `RAM ONLY: not kept` and nothing is written.

## Pre-flight (any ❌ = stop)

| # | Check | State | Evidence |
|---|---|---|---|
| P1 | Off-laptop backup recorded in `docs/device.md` | ✅ (one copy) | Unchanged (M1's P1). |
| P2 | USB port + cable | ✅ | Port **1-2**, the new cable. A `-110` with our firmware or loader = stop and report. |
| P3 | Restore per D015 | ✅ | As M2's P3; `firmware/vendor` unchanged since 98fc08c. |
| P4 | The package: commit + sha256 + emulator | ✅ | `build/step-M3/part5/fm1acc-0.3.5-m3p5.fwsc`, sha256 `becbc129d639e9645279429589dfb3816713e968961cc44e594d981aef71501d`, identity FM-1_46000305, built from Part 4's firmware source (commit `a37ae26`, version 0.3.5) with the songs of the Part 5 commit. Emulator: the **full suite** on this exact package, `build/step-M3/part5/report.md`: **219 checks, ALL PASS** (all 9 songs played to the end, the store with power cuts, flash ids, and every check of parts 1–5). |
| P5 | Flash IDs | ✅ | Emulator with 0x856014 and 0xB36014: same screens, sound, songs stored. With the unknown 0xC84017: `NO SONG STORAGE`, the keys sound, a send is refused, no crash. Loader: unchanged (P3). |
| P6 | Unit state before step 1 | (check) | `--info` must print `FM-1_46000300  [running]` (M2). |
| P7 | Fallback package | ✅ | M2, known good on this unit: `build/step-M2/final/fm1acc-0.3.0-m2.fwsc`, sha256 `0294089be99c6e07aeaecba8aedc749097642aa8522b48ff597ff1c6177ac52d`. It reads the same song store (format 1.1) and ignores the settings. |
| P8 | The songs to send | ✅ | `songs/*/`: 9 songs, format 1.1 (unchanged). Greensleeves, Danny Boy and Oh! Susanna are updated (level-1 drums) and Happy Birthday is new. Sending a song with the same slug replaces the stored one. |

## Rules for every step (as M2's)

- **The user runs every command, in their own terminal tab.** Claude Code only reads (`--info`, `lsusb`, kernel
  log) between steps and records. `fm1song list` is a read-only SysEx question (HANDOFF 12:50); the user runs it in
  this checklist anyway, since CLAUDE.md's text is not updated yet.
- Laptop: suspend / sleep off, lid open, mains power. Close fm1-live / the emulator, `aplaymidi` and other MIDI apps.
- The FM-1 stays on USB power the whole time; never switch it off during a write.
- Port 1-2, the new cable. Do not touch the FM-1 or the cable during a write.
- One install command per update; `--info` after it, before anything else.
- No `--force`, `--i-know`, `--skip-check`, `--no-validate` or any other override. No FM-1_016 (D012).
- Stop and report on any unexpected output.

---

## Checklist

### Step 1 — Install M3 parts 1–5 over M2

1. `--info` → `FM-1_46000300  [running]`:
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py --info
   ```
2. The package is the reviewed one:
   ```bash
   sha256sum build/step-M3/part5/fm1acc-0.3.5-m3p5.fwsc
   ```
   expect `becbc129d639e9645279429589dfb3816713e968961cc44e594d981aef71501d`.
3. Install (about 1–2 minutes):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M3/part5/fm1acc-0.3.5-m3p5.fwsc
   ```
4. `--info` → expect `FM-1_46000305  [running]`.
5. On the unit: the splash with `v0.3.5-m3p5`, then **SONGS** with the 8 songs already stored. The legend at the
   bottom now has amber button names.

If it fails: as M2's step 1 ("If it fails"):
- refused with `-14`: nothing was erased; install M2 (P7) or stock V15 (path A) and report;
- does not start: fm1-base's SAFE MODE after two failed starts (`--info` answers; install M2, P7, from there);
- ROM download mode after four: path B.

### Step 2 — Send the updated and new songs

1. What the FM-1 holds now (sends only a question; writes nothing):
   ```bash
   tools/fm1song.sh list
   ```
   expect `FM-1: FM-1_46000305 ...` and the 8 songs from M2, each `ok`.
2. Send all nine (writes the song store only; a few seconds):
   ```bash
   tools/fm1song.sh send songs/*/
   ```
   expect nine lines `songs/<name>: stored (...)`. The same slugs replace the old copies.
3. Check:
   ```bash
   tools/fm1song.sh list
   ```
   expect the nine slugs, each `ok`, and `9 song(s)`.

If a song is `NOT stored`: note the reason and send it again once. On `the transfer stopped`, `no FM-1 found` or a
second failure, stop and report.

### Step 3 — On the unit (no command; about 30 minutes)

Parts 1–5, ordered so you can test one layer at a time. docs/controls.md has the map, and **GLO** shows every
control on the unit.

**Part 1 (bugs)**
1. **L1 drums:** Greensleeves and Oh! Susanna at drums L1 now have a backbeat (snare); L2 is busier.
2. **Beat light:** PLAY flashes clearly on **every** beat, longer on beat 1.
3. **Count-in after a pause:** in the keyboard and chart views (EDIT), pause, then PLAY. The count replaces the
   view.

**Part 2 (your part): mute one layer at a time, in Amazing Grace, then Danny Boy**
4. **Harmony muted (ENV):**
   - the top line says `HARM - YOURS` and the HARM box says YOURS;
   - the chord and its rhythm cue (now right of the chord, smaller) are shown;
   - the FM-1's keys **blink** with the chords: bright at each chord, dimmer between. Is the dimmer level visible on
     the real keys?
   - EDIT: the keyboard view blinks the same way.
5. **Bass muted (SEL), harmony on:** the big name is the bass note (`E2`), with the next note and the chord. The
   strip and chart show the bars' bass notes, the keyboard view and the FM-1's keys show the bass note, and the
   cue follows the bass.
6. **Melody muted (LFO):** the same for the melody.
7. **Drums muted (FX):** the hit now (`KICK`, `SNARE`) and the next; small grids in the strip; no key lights. EDIT
   goes to the chart (no keyboard view for drums).
8. **Two muted:** mute harmony, then bass. Bass is followed (YOURS on BASS, MUTE on HARM). Unmute bass and the
   harmony is followed again. Is "the last one muted is yours" a good rule?
9. **Nothing muted:** the rhythm cue still shows, right of the chord.

**Part 3 (loops), on the Blues**
10. SEQ: `LOOP: SET A`; turn ALGORITHM; SEQ. Then `SET B`; turn ALGORITHM; SEQ. The loop is on: its bars are
    filled, there is one A and one B, and PLAY plays from A to the end of B and back. Try PRESETS while setting
    (it jumps to section edges) and HOME (one step back). SEQ again: the loop is off. Setting it while playing also
    works.

**Part 4 (screen, count-in, settings)**
11. The legend on every screen: amber names, gray actions, dividers. The section row is gone and the views are
    taller.
12. **Inversion line** under the strip: `C/E, 1st inversion` and so on. Change the harmony level (K3): at L1 always
    root position, at L2 often an inversion.
13. **Keyboard view zoomed:** about 1.5 octaves, following the chord or note.
14. **Count-in:** a loud beep (C6 on beat 1, G5 on the others), on every view.
15. **Settings (GLO, GLO):**
    - change the count-in to 2 BARS and the volume to LOW;
    - turn the key lights' blinking OFF (the keys stay steady);
    - leave with GLO, switch the FM-1 off and on: the settings are kept;
    - set them back the way you like them.

**Part 5**
16. **Happy Birthday to You:** in the list, 3/4 with a two-note pickup. Play it with each layer muted in turn.

17. **Record** CPU and voices (GLO screen) at the busiest place of the Blues at 250 BPM, and your impressions.

If it misbehaves: note what and where (song, bar, control). To go back, install M2 (P7):
```bash
.venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M2/final/fm1acc-0.3.0-m2.fwsc
```
(`--info` → `FM-1_46000300`), and report. M2 plays the stored songs as they are and ignores the settings.

---

## Restore paths (D015, unchanged)

**Path A — normal return to stock:** as [step-M2-install.md](step-M2-install.md). The song store and the
settings are left as they are; stock never reads either.

**Path B — ROM download mode:** exactly as [step-0.7-first-writes.md](step-0.7-first-writes.md), "Restore paths".
B2 (the full dump) also brings 0x97000–0xDBFFF and 0xFC000–0xFDFFF back to all 0xFF.

## After the user has run it

Claude Code records in `docs/LOG.md` each command's output, the identities, the times (kernel log), the
`fm1song list` output, the readings and the impressions. In `docs/device.md` ("Currently installed") it records
the firmware and songs the unit is left with.
