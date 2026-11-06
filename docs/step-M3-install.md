# Milestone M3 — install on the FM-1 (checklist)

**Status: PREPARED, WAITING FOR REVIEW** by the planning chat and the user. This is the final M3 checklist (parts
1–9). It replaces the early one ([step-M3-early-install.md](step-M3-early-install.md)) unless the user has
already run that; then step 1 installs over 0.3.5-m3p5 instead of M2, with the same command and checks. Nothing
has been installed or sent. Claude Code ran no device command while preparing it (the FM-1 was not plugged in).
**The user runs every device command**, one at a time, in their own terminal, and pastes each step's output to
Claude Code before the next.

Prepared 2026-11-05 by Claude Code. The rules, pre-flight and restore paths are the same as the approved and
completed M2 checklist ([step-M2-install.md](step-M2-install.md)); what differs is spelled out here. Emulator
report: [step-M3-emulator-report.md](step-M3-emulator-report.md).

---

## What changes on the unit

| | Now | After |
|---|---|---|
| Firmware | M2, **FM-1_46000300**, v0.3.0-m2 (or 0.3.5-m3p5) | M3, **FM-1_46000400**, v0.4.0-m3 |
| Step 1 writes | — | The app area 0x4000–0x92FFF and the update staging area 0xE0000–0xE4FFF, by our update loader, exactly as the M1 and M2 installs. The loader's code is unchanged since Step 0.7: `git diff 98fc08c -- firmware/vendor` is empty. |
| Step 2 writes | — | The **song store, 0x97000–0xDBFFF**, by our firmware when `fm1song send` asks: the same kind of write as M2's. The songs are now format 1.2 (with their category), so they are all sent again. |
| Step 3 writes (**new kind of write**) | — | The **settings, 0xFC000–0xFDFFF** (two 4 KiB sectors), by our firmware, only when you leave the settings screen after changing something. |

### The settings save (new), explained

- **What it is.** M3 has a settings screen (GLO, then GLO again) with these rows: key lights blink, count-in
  length, count-in volume, note names, scrolling views zoom, guide volume. When you leave it (GLO or HOME) after
  changing a value, the firmware saves the settings with fm1-base's own settings store
  (`plat_store_save(OBJ_SETTINGS)`). Nothing is saved if nothing changed. No reboot, no update mode, no loader.
- **Where.** Two copies at 0xFC000 and 0xFD000 (fm1-base's A/B scheme, `firmware/vendor/fm1-base/device/storage.c`).
  A save goes to the copy that is not current: erase that one sector, write the data, then the header last. On
  start-up the newer valid copy wins, so a save that is cut off leaves the previous settings in charge. These two
  sectors are in our app's store map (app.h `APP_STORE_MAP`, unchanged since M1).
  - **Not touched:** everything else, including the song store, the panel calibration (0xDC000), the update staging
    area and M-VAVE's data.
  - **Stock never uses these sectors:** in our unit's full stock dump (backups/flash/FM-1_015_full-flash_856014_2026-11-04.bin)
    0xFC000–0xFFFFF is all 0xFF.
- **If it is cut off:** the previous settings (or the defaults) are used. Nothing else can be affected.
- **How to undo.** Change the values back. M2 (the fallback) and stock never read these sectors. Path B2 (the full
  dump) brings them back to 0xFF.
- **Tested** in the emulator: saved once, used, and still in use after a restart from the same flash. With an
  unknown flash chip the screen says `RAM ONLY: not kept` and nothing is written.

### The songs (step 2), what is new

- **Format 1.2** adds each song's category (its folder in the list). The M3 firmware reads 1.0, 1.1 and 1.2.
- **`fm1song send` checks the FM-1's firmware first:** it asks which formats the FM-1 reads. For an M2 FM-1 it sends
  1.1 without the category, so falling back to M2 only needs the songs sent again (below).
- **`fm1song send` skips non-songs:** `songs/*/` sends the 9 song folders; anything that is not a song folder is
  skipped with a one-line note.

## Pre-flight (any ❌ = stop)

| # | Check | State | Evidence |
|---|---|---|---|
| P1 | Off-laptop backup recorded in `docs/device.md` | ✅ (one copy) | Unchanged (M1's P1). |
| P2 | USB port + cable | ✅ | Port **1-2**, the new cable. A `-110` with our firmware or loader = stop and report. |
| P3 | Restore per D015 | ✅ | As M2's P3; `firmware/vendor` unchanged since 98fc08c. |
| P4 | The package: commit + sha256 + emulator | ✅ | `build/step-M3/final/fm1acc-0.4.0-m3.fwsc`, sha256 `7bb7e58ad8c27cb8eb4038fef641f02a708fa01344fd856931cf1280d027787d`, identity FM-1_46000400, from the Part 9 commit (the one adding this file). Emulator: the **full suite** on this exact package, `build/step-M3/final/report.md`: **235 checks, ALL PASS** (all 9 songs to the end on time, the store with power cuts, flash ids, every check of parts 1–8). Parser fuzz 10 min: 1,788,000 files, no report. |
| P5 | Flash IDs | ✅ | Emulator with 0x856014 and 0xB36014: same screens, sound, songs stored. With the unknown 0xC84017: `NO SONG STORAGE`, the keys sound, a send is refused, no crash. Loader: unchanged (P3). |
| P6 | Unit state before step 1 | (check) | `--info` must print `FM-1_46000300  [running]` (M2), or `FM-1_46000305` if the early checklist was run. |
| P7 | Fallback package | ✅ | M2, known good on this unit: `build/step-M2/final/fm1acc-0.3.0-m2.fwsc`, sha256 `0294089be99c6e07aeaecba8aedc749097642aa8522b48ff597ff1c6177ac52d`. M2 cannot read 1.2 songs: after a fallback, send `songs/*/` again (it then gets 1.1, tested in the emulator). It ignores the settings. |
| P8 | The songs to send | ✅ | `songs/*/`: 9 songs, format 1.2, each with a category. Sending a song with the same slug replaces the stored one. |

## Rules for every step (as M2's)

- **The user runs every command, in their own terminal tab.** Claude Code only reads (`--info`, `lsusb`, kernel
  log) between steps and records. `fm1song list` is a read-only SysEx question (HANDOFF 12:50); the user runs it in
  this checklist anyway, since CLAUDE.md's text is not updated yet (see LOG, Part 1).
- Laptop: suspend / sleep off, lid open, mains power. Close fm1-live / the emulator, `aplaymidi` and other MIDI apps.
- The FM-1 stays on USB power the whole time; never switch it off during a write.
- Port 1-2, the new cable. Do not touch the FM-1 or the cable during a write.
- One install command per update; `--info` after it, before anything else.
- No `--force`, `--i-know`, `--skip-check`, `--no-validate` or any other override. No FM-1_016 (D012).
- Stop and report on any unexpected output.

---

## Checklist

### Step 1 — Install M3

1. `--info` → `FM-1_46000300  [running]` (or `FM-1_46000305`):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py --info
   ```
2. The package is the reviewed one:
   ```bash
   sha256sum build/step-M3/final/fm1acc-0.4.0-m3.fwsc
   ```
   expect `7bb7e58ad8c27cb8eb4038fef641f02a708fa01344fd856931cf1280d027787d`.
3. Install (about 1–2 minutes):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M3/final/fm1acc-0.4.0-m3.fwsc
   ```
4. `--info` → expect `FM-1_46000400  [running]`.
5. On the unit: the splash with `v0.4.0-m3`, then the list, **ALL SONGS**, with the 8 (or 9) songs already
   stored. The 1.1 copies have no category yet; step 2 replaces them.

If it fails: as M2's step 1 ("If it fails"):
- refused with `-14`: nothing was erased; install M2 (P7) or stock V15 (path A) and report;
- does not start: fm1-base's SAFE MODE after two failed starts (`--info` answers; install M2, P7, from there);
- ROM download mode after four: path B.

### Step 2 — Send the songs (format 1.2)

1. What the FM-1 holds now (sends only a question; writes nothing):
   ```bash
   tools/fm1song.sh list
   ```
   expect `FM-1: FM-1_46000400 ...`, the stored songs each `ok`, and `reads format 1.0..1.2`.
2. Send all nine (writes the song store only; a few seconds):
   ```bash
   tools/fm1song.sh send songs/*/
   ```
   expect nine lines `songs/<name>: stored (...)` and no "sending 1.1" line (that would mean the FM-1 is not on M3).
3. Check:
   ```bash
   tools/fm1song.sh list
   ```
   expect the nine slugs, each `ok`, and `9 song(s)`.

If a song is `NOT stored`: note the reason and send it again once. On `the transfer stopped`, `no FM-1 found` or a
second failure, stop and report.

### Step 3 — On the unit (no command; about 45 minutes)

Ordered so you can test **one layer at a time, view by view**. EDIT steps through your part's views. docs/controls.md
has the map, and **GLO** shows every control.

**A. The list, the settings, the basics**
1. **Folders:** the list opens in ALL SONGS. HOME shows the folders (All songs, Blues, Celebrations, Folk, Hymns,
   Practice forms), each with its count. Open Folk: 4 songs. HOME, then All songs.
2. **Legend:** amber button names, gray actions and dividers on every screen.
3. **Settings (GLO, GLO):** six rows. Leave with GLO, switch the FM-1 off and on, and check that changed values are
   kept. Set them back as you like.
4. **Count-in:** a loud beep (C6 on beat 1, G5 on the others). Try 2 BARS and LOW in the settings. The count
   replaces every view while it counts.
5. **Beat light:** PLAY flashes clearly on every beat, longer on beat 1.

**B. Harmony (mute ENV): Amazing Grace, then Danny Boy**
6. **Main view:**
   - the top line says `HARM - YOURS`, the HARM box YOURS;
   - the chord, with the rhythm cue on its right (smaller, no level label);
   - the inversion line under the strip (`C/E, 1st inversion`): at L1 always root position, at L2 (K3) often an
     inversion.
7. **Key lights:** the FM-1's keys **blink** with the chords, bright at each attack and dimmer between. Is the
   dimmer level visible? With the setting "Key lights blink" OFF they stay steady.
8. **EDIT, the keyboard view:** zoomed (about 1.5 octaves, following the chord), the keys blinking the same way, the
   next chord outlined, the inversion under it.
9. **EDIT, the chart view:** the section's bars.
10. **EDIT, the guitar view:**
    - the chord box (dots with fingers, o / x, a barre for F), the next chord small;
    - change the key (SAVE, SELECT -3: A): barre shapes follow;
    - is the box readable?

**C. Bass (mute SEL, harmony on): Danny Boy**
11. **Main view:** the bass note with its octave (`E2`), the next, and the chord. The strip shows the bars' bass
    notes. The key lights show the bass note, blinking.
12. **EDIT, keyboard:** the bass note.
13. **EDIT, chart:** the bars' bass notes.
14. **EDIT, bass tab:** 4 strings, fret numbers scrolling to the amber line, note names under the tab.
15. **EDIT, notation (bass clef):** the bass notes on the staff.

**D. Melody (mute LFO): Amazing Grace, Happy Birthday**
16. **Main view:** the melody note, the next, the chord. Keys: the melody note, blinking.
17. **EDIT, keyboard and chart:** as for bass.
18. **EDIT, melody roll:** bars sliding to the line, names above them.
19. **EDIT, notation (treble clef):**
    - clef, key signature (G: one sharp), noteheads, stems, beams for eighth pairs, dots;
    - chord names above, note names under (setting "Note names");
    - the sounding note amber.
    - The setting "Scrolling views show 3 BARS" halves the scale.
    - A staff reader should judge it (D025).
20. **Guide:** settings, "Guide (your part)" 15 % then 30 %. The melody plays quietly (the box says GUIDE). OFF:
    silent again.

**E. Drums (mute FX): Saints, Oh! Susanna**
21. **Main view:** the hit now (`KICK`, `SNARE`) and the next; small grids in the strip; no key lights.
22. **EDIT, chart:** each bar's grid.
23. **EDIT, drum grid:** kick, snare, hats and other rows scrolling.
24. **L1 drums** on Greensleeves and Oh! Susanna have a backbeat; L2 is busier.

**F. Loops (the Blues)**
25. SEQ: `LOOP: SET A`; turn ALGORITHM; SEQ. Then `SET B`; turn ALGORITHM; SEQ. The loop is on: its bars are
    filled, there is one A and one B, and PLAY plays from A to the end of B and back. Try PRESETS while setting
    (it jumps to section edges), HOME (one step back), and setting it while playing. SEQ with the loop on turns it
    off.

**G. Several muted, other songs**
26. Mute harmony, then bass: bass is followed (YOURS on BASS). Unmute bass: harmony again. Is the rule "the last one
    muted is yours" right?
27. **Happy Birthday:** 3/4, the two-note pickup; play it with each layer muted.
28. **Record** CPU and voices (GLO screen) at the busiest place of the Blues at 250 BPM in the notation view, and
    your impressions.

If it misbehaves: note what and where (song, bar, control, view). To go back, install M2 (P7):
```bash
.venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M2/final/fm1acc-0.3.0-m2.fwsc
```
(`--info` → `FM-1_46000300`). Then send the songs again for M2; `fm1song send` sends 1.1, which M2 reads:
```bash
tools/fm1song.sh send songs/*/
```
Report it. The part builds `build/step-M3/part1/` … `part8/` are each tested too: tell the planning chat if one of
them is preferred.

---

## Restore paths (D015, unchanged)

**Path A — normal return to stock:** as [step-M2-install.md](step-M2-install.md). The song store and the
settings are left as they are; stock never reads either.

**Path B — ROM download mode:** exactly as [step-0.7-first-writes.md](step-0.7-first-writes.md), "Restore paths".
B2 (the full dump) also brings 0x97000–0xDBFFF and 0xFC000–0xFDFFF back to all 0xFF.

## After the user has run it

Claude Code records in `docs/LOG.md` each command's output, the identities, the times (kernel log), the
`fm1song list` output, the readings and the impressions. In `docs/device.md` ("Currently installed") it records
the firmware, the songs and the settings the unit is left with.
