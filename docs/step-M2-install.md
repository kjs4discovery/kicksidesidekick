# Milestone M2 — install on the FM-1 (checklist)

> 2026-11-05 (M3 Part 1): step 2 now sends `songs/*/` (folders only); `songs/*` also matched
> songs/make_songs.py and README.md, which `fm1song send` refused (HANDOFF 12:45). `fm1song send` now skips
> such arguments with a one-line "skipped" note.

**Status: PREPARED, WAITING FOR REVIEW** by the planning chat and the user. This is the final M2 checklist (parts
1–5); it replaces the early one ([step-M2-part2-install.md](step-M2-part2-install.md)) unless the user has already
run that (then step 1 installs over 0.2.2-m2p2 instead of M1: the same command, the same checks). Nothing has been
installed or sent. Claude Code ran no device command while preparing it (the FM-1 was not plugged in). **The user
runs every device command**, one at a time, in their own terminal, pasting each step's output to Claude Code
before the next.

Prepared 2026-11-05 by Claude Code. Same rules, pre-flight and restore paths as the approved and completed M1
checklist ([step-M1-install.md](step-M1-install.md)); what differs is spelled out here. Emulator report:
[step-M2-emulator-report.md](step-M2-emulator-report.md).

---

## What changes on the unit

| | Now | After |
|---|---|---|
| Firmware | M1, **FM-1_46000200**, v0.2.0-m1 (5 songs built in) | M2, **FM-1_46000300**, v0.3.0-m2: **no songs built in**; songs from the flash's song store |
| Step 1 writes | — | the app area 0x4000–0x92FFF and the update staging area 0xE0000–0xE4FFF, by our update loader, exactly as M1's install (the loader's code is unchanged since Step 0.7: `git diff 98fc08c -- firmware/vendor` is empty) |
| Step 2 writes (**new kind of write**) | — | the **song store, 0x97000–0xDBFFF** (in fm1-base's data area), by our firmware itself, when `fm1song send` asks; nothing else |

### The song send (step 2), explained

- **What it is.** `tools/fm1song.sh send` compiles and checks each song on the laptop, finds the FM-1 on USB (it
  talks only to firmware whose identity starts `FM-1_46`: ours; stock, Felucca or an update loader are refused
  before anything is sent), and asks it to store the file. The FM-1 pulls the whole file into RAM (the update's
  512-byte read requests, as an install does), checks its CRC and the whole song as the player will read it, and
  only then writes it. No reboot, no update mode, no loader: it is our running firmware writing its own data area.
- **Where.** 0x97000–0xDBFFF only (69 sectors of 4 KiB). Every erase and write is checked against that range
  before it is made (`firmware/app/store.c`); an attempt outside would be refused (tested: never happens).
  **Not touched**: the boot head, the app area, the update staging area, fm1-base's panel calibration
  (0xDC000–0xDDFFF), the settings (0xFC000), M-VAVE's own data. In our unit's full dump (stock V15)
  0x97000–0xDFFFF was all 0xFF: stock does not use it (its data is at 0x93000 and 0xE9000–0xFBFFF). Whatever
  Felucca 1.8.1 (Step 0.7, step 1) may have left there is ignored (a song counts only with our record's own CRC)
  and erased where a song goes.
- **If it is cut off** (cable, power): every song already stored stays as it was; the song being sent is either
  stored whole or not at all. Then send it again. (Tested in the emulator: power cuts at 6 points of a send, and
  on the laptop: a cut at every single erase and write.)
- **How to undo.** `tools/fm1song.sh delete SLUG` removes a song (or send it again to replace it). Reinstalling M1
  (fallback below) ignores the song store. Going back to stock V15 (path A) leaves the songs there, unread: stock
  never reads 0x97000–0xDBFFF.
- **Time.** About 4–5 s for all eight songs in the emulator (~78 KB); on the unit likely a few seconds (the
  install moves 470 KB in ~55 s).

## Pre-flight (any ❌ = stop)

| # | Check | State | Evidence |
|---|---|---|---|
| P1 | Off-laptop backup recorded in `docs/device.md` | ✅ (one copy) | Unchanged (M1's P1). |
| P2 | USB port + cable | ✅ | Port **1-2**, the new cable. Our firmware and loader enumerate in < 1 s with no `-110` (M1 install, 2026-11-05). A `-110` with our firmware or loader = stop and report. |
| P3 | Restore per D015 | ✅ | As M1's P3: path A proven from our firmware (Step 0.7 step 4); `firmware/vendor` unchanged since 98fc08c. |
| P4 | The package: commit + sha256 + emulator | ✅ | `build/step-M2/final/fm1acc-0.3.0-m2.fwsc`, sha256 `0294089be99c6e07aeaecba8aedc749097642aa8522b48ff597ff1c6177ac52d`, identity FM-1_46000300, from commit `2e470ed`. Emulator: `build/step-M2/final/report.md`, **this exact package**, all PASS. |
| P5 | Flash IDs | ✅ | Emulator with 0x856014 and 0xB36014: same screens, sound, songs stored; unknown 0xC84017: `NO SONG STORAGE`, keys sound, a send refused, no crash. Loader: unchanged (P3). |
| P6 | Unit state before step 1 | (check) | `--info` must print `FM-1_46000200  [running]` (M1). |
| P7 | Fallback package | ✅ | M1, known good on this unit: `build/step-M1/fm1acc-0.2.0-m1.fwsc`, sha256 `bf8f640f8137efc278fa17f6bcd7c21791eb1adbd68a7b7299c22f851804ff25` (its songs are built in: it works whatever is in the song store). |
| P8 | The songs to send | ✅ | `songs/*/` (8 songs, format 1.1, ~78 KB; Four Chords' melody is now the clarinet; 3 new songs). The laptop checks each before anything is sent. |

## Rules for every step (as M1's)

- **The user runs every command, in their own terminal tab.** Claude Code only reads (`--info`, `lsusb`, kernel
  log) between steps and records. (`fm1song list` sends a SysEx question; CLAUDE.md lets Claude Code send no SysEx,
  so the user runs it too.)
- Laptop: suspend / sleep off, lid open, mains power. Close fm1-live / the emulator, `aplaymidi` and other MIDI apps.
- The FM-1 stays on USB power the whole time; never switch it off during a write.
- Port 1-2, the new cable. Do not touch the FM-1 or the cable during a write.
- One install command per update; `--info` after it, before anything else.
- No `--force`, `--i-know`, `--skip-check`, `--no-validate` or any other override. No FM-1_016 (D012).
- Stop and report on any unexpected output.

---

## Checklist

### Step 1 — Install M2 over M1

1. `--info` → `FM-1_46000200  [running]`:
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py --info
   ```
2. The package is the reviewed one:
   ```bash
   sha256sum build/step-M2/final/fm1acc-0.3.0-m2.fwsc
   ```
   expect `0294089be99c6e07aeaecba8aedc749097642aa8522b48ff597ff1c6177ac52d`.
3. Install (about 1–2 minutes; M1's took ~55 s of loader):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M2/final/fm1acc-0.3.0-m2.fwsc
   ```
4. `--info` → expect `FM-1_46000300  [running]`.
5. On the unit: the splash (a cartoon note, "FM-1 Accompaniment", `v0.3.0-m2`) for about 3 s, then **SONGS**:
   most likely **NO SONGS YET** and the send command (or, if Felucca left something the store cannot read: the
   same). `NO SONG STORAGE` = the flash chip was not accepted: stop and report.

If it fails: exactly as M1's step 1 ("If it fails"): refused with `-14` → nothing erased, install stock V15
(path A) and report; does not start → fm1-base's SAFE MODE after two failed starts (`--info` answers; install M1,
P7, from there), ROM download mode after four → path B.

### Step 2 — The first song send (the new kind of write)

1. What the FM-1 holds now (sends only a question; writes nothing):
   ```bash
   tools/fm1song.sh list
   ```
   expect `FM-1: FM-1_46000300 ...` and `0 song(s); 69 of 69 sectors free` (or fewer free if old data was found:
   fine, it will be erased where songs go).
2. Send the eight songs (writes the song store only; a few seconds):
   ```bash
   tools/fm1song.sh send songs/*/
   ```
   expect eight lines `songs/<name>: stored (<n> bytes; <k> song(s) on the FM-1, <f> sectors free)`. The FM-1
   shows `RECEIVING SONG` / `STORING SONG` / `SONG STORED` and then the list.
3. Check:
   ```bash
   tools/fm1song.sh list
   ```
   expect the eight slugs, each `ok`, and `8 song(s)`.

If a song is `NOT stored`: note the reason; send it again once. `the transfer stopped`, `no FM-1 found` or a
second failure: stop and report (the songs stored so far stay; the FM-1 keeps working).

### Step 3 — On the unit (no command; about 25 minutes)

Every item, part by part (docs/controls.md has the map; **GLO** shows every control on the unit):

1. **Songs from the store** (items 11, 18): the eight songs in the list, by title (SELECT scrolls). Switch the FM-1
   off and on: still there.
2. **Tempo in BPM** (item 1): SELECT, 1 BPM a click, 5 when turned quickly, 30–250; amber when not the song's own.
   Open another song: it starts at its own BPM. Try 30 and 250 on the Blues.
3. **Splash** (item 2): the logo for ~3 s at power-on.
4. **Count-in** (item 4): louder than M1, beat 1 accented; the PLAY light flashes with it.
5. **Loops and start points** (items 5, 15): PRESETS = start from the next section; ALGORITHM = start from a bar;
   SEQ = loop SECTION → BARS A–B (PRESETS moves A, ALGORITHM B) → SONG → off; the strip shows the loop (amber line,
   A and B). While playing a section loop, PRESETS moves it to the next section: it goes there at the next bar
   line.
6. **Pause** (item 13): PLAY pauses; PLAY again goes on (count-in on: a bar of clicks, then the paused bar from its
   beat 1; off: right where it paused). HOME stops (back to the start point); HOME again: the list.
7. **Bar strip** (item 6): a chord held over into the next bar is no longer dimmed.
8. **Four Chords** (item 7): the melody is now a clarinet; listen for the crackle (speaker and headphones).
9. **The keys' octave** (item 9): OCT+ / OCT− shift the keys (−3..+3), shown for a moment, the button lit.
10. **Beat light** (B): PLAY flashes on every beat, brighter on beat 1.
11. **Help and legends**: GLO on the list and while playing; the bottom line on every screen.
12. **Key** (item 8): SAVE, then SELECT: the key moves (top line `KEY: D (+2)`), names re-spelled; play: the band
    in the new key (not the drums); change it while playing: no note hangs. Open another song: its own key.
13. **Views** (item 12): EDIT: the keyboard (the chord's keys lit, the next outlined), EDIT: the chart, EDIT: back.
14. **Rhythm cue** (item 10): harmony muted: a note symbol left of the chord, flashing at each chord; L1 vs L2 (K3):
    longer vs shorter notes.
15. **Bass names** (F): bass muted: `BASS C3  then G2` under the layers.
16. **Key lights** (G): harmony muted: the FM-1's keys for the chord light (the next chord's dimly in the last bar
    before it); bass muted (harmony on): the bass note's key; with OCT+ / OCT−: the lights follow. Keys you press
    light as ever. Both muted: harmony's (a proposal: say if you would rather choose).
17. **The new songs** (E): Greensleeves (minor, 3/4), Danny Boy (slow, slash chords: the bass plays the note after
    the slash), Oh! Susanna (fast); drums L1 (just the beat) vs L2 (full groove) clearly different.
18. **Record** CPU and voices (GLO screen) at the busiest place of the Blues and Oh! Susanna at 250 BPM, and your
    impressions.

If it misbehaves: note what and where (song, bar, control), then install M1 (P7) to go back (or a part build,
`build/step-M2/part2/` or `part3/`, each tested: tell the planning chat which you would use):
```bash
.venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M1/fm1acc-0.2.0-m1.fwsc
```
(`--info` → `FM-1_46000200`), and report. The stored songs stay in the store, unused by M1.

---

## Restore paths (D015, unchanged)

**Path A — normal return to stock** (any firmware that answers `--info`, or update mode):
```bash
.venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py backups/stock-firmware/FM-1_015.fwsc
```
The song store is left as it is; stock never reads it.

**Path B — ROM download mode**: exactly as [step-0.7-first-writes.md](step-0.7-first-writes.md), "Restore paths".
B2 (the full dump) also brings 0x97000–0xDBFFF back to all 0xFF.

## After the user has run it

Claude Code records in `docs/LOG.md`: each command's output, identities, times (kernel log), the `fm1song list`
output, the readings and impressions; and in `docs/device.md` ("Currently installed"): the firmware and the songs
the unit is left with.
