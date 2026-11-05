# Milestone M1 — install on the FM-1 (checklist)

**Status: PREPARED, WAITING FOR REVIEW** by the planning chat and the user. Nothing has been installed. Claude Code
ran no device command while preparing it (the FM-1 was unplugged). **The user runs every device command**, one at
a time, in their own terminal, pasting each step's output to Claude Code before the next.

Prepared 2026-11-04 by Claude Code (overnight run, HANDOFF 23:02). Same rules, pre-flight and restore paths as the
approved and completed Step 0.7 checklist ([step-0.7-first-writes.md](step-0.7-first-writes.md)); only what
differs is spelled out here.

---

## What changes on the unit

| | Now | After |
|---|---|---|
| Firmware | our Step 0.7 prototype, **FM-1_4600**, v0.1.0-proto | M1, **FM-1_46000200**, v0.2.0-m1 |
| What it does | one song (Amazing Grace), EAS's MIDI file player | song list (5 songs), our player: mutes, levels, tempo, count-in, loops, bar strip |
| Written by | — | our update loader (felucca-bsp's, patched to write only 0x856014 / 0xB36014 flash), staged by the prototype now running |
| Area written | — | the app area 0x4000–0x92FFF and the update staging area 0xE0000–0xE4FFF, as in Step 0.7. No ROM boot, boot head untouched |

The M1 build is a **release build** (`firmware/build.sh --release 0.2.0-m1`) so its identity differs from the
prototype's: the dev builds of both would answer `FM-1_4600`, and `--info` could not tell them apart.

## Pre-flight (any ❌ = stop)

| # | Check | State | Evidence |
|---|---|---|---|
| P1 | Off-laptop backup recorded in `docs/device.md` | ✅ (one copy) | Unchanged since Step 0.7: the user's thumb drive. Cloud copy still recommended (HANDOFF 20:40 item 3); the writes touch only the app area, the dump matters only for path B2. |
| P2 | USB port + cable | ✅ | Port **1-2**, the new cable (Step 0.7). Our firmware and our loader enumerate in < 1 s with no `-110` (Step 0.7 kernel log); only stock V15 shows the single `-110` / ~16 s. A `-110` with **our** firmware or loader = stop and report. |
| P3 | Restore per D015 | ✅ | Path A from our firmware to stock V15 was run on this unit in Step 0.7 step 4. The update code it used (fm1-base / felucca-bsp, `firmware/vendor/`) is **byte-identical** in this build: `git diff 98fc08c -- firmware/vendor` is empty. Paths B1/B2 unchanged (below). |
| P4 | The M1 package: commit + sha256 + emulator | ✅ | `build/step-M1/fm1acc-0.2.0-m1.fwsc`, sha256 `bf8f640f8137efc278fa17f6bcd7c21791eb1adbd68a7b7299c22f851804ff25`, identity FM-1_46000200, from commit `018e5d7`. Emulator: [step-M1-emulator-report.md](step-M1-emulator-report.md), **this exact package**, all PASS. |
| P5 | Flash IDs | ✅ | Emulator with 0x856014 and 0xB36014: same screen, sound; unknown 0xC84017: `RAM ONLY`, sound, no crash (report). Loader: unchanged source (P3), refuses unknown chips (Step 0.6/0.7). |
| P6 | Unit state before step 1 | (check) | `--info` must print `FM-1_4600  [running]`. |
| P7 | Fallback package | ✅ | The prototype, known good on this unit: `build/step-0.7/fm1acc-proto-0.1.0.fwsc`, sha256 `58be4537be314bf463dc828c919b18675c2626d01be5a01724560b3cc760d54f`. |

## Rules for every step (from Step 0.7, approved 20:40)

- **The user runs every command, in their own terminal tab.** Claude Code only reads (`--info`, `lsusb`, kernel
  log) between steps and records.
- Laptop: automatic suspend / sleep off for the session, lid open, on mains power.
- Close fm1-live / the emulator, `aplaymidi` and any other MIDI apps.
- The FM-1 stays on USB power the whole time; never switch it off during a write.
- Port 1-2, the new cable. Do not touch the FM-1 or the cable during a write.
- **One install command per update**; `--info` after each, before anything else.
- One loader command per ROM boot session (only if path B is ever needed).
- No `--force`, `--force-write`, `--i-know`, `--skip-check` or any other override. No FM-1_016 (D012).
- Stop and report on any unexpected output.

Read-only checks:

```bash
.venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py --info
```
```bash
lsusb | grep -i -e 4c4a -e 1209
```

---

## Checklist

### Step 1 — Install M1 over the prototype

1. `--info` → `FM-1_4600  [running]`.
2. Check the package is the reviewed one:
   ```bash
   sha256sum build/step-M1/fm1acc-0.2.0-m1.fwsc
   ```
   expect `bf8f640f8137efc278fa17f6bcd7c21791eb1adbd68a7b7299c22f851804ff25`.
3. Install:
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M1/fm1acc-0.2.0-m1.fwsc
   ```
   **Time:** about 1–2 minutes (Step 0.7: ~55 s of writing with our loader).
4. `--info` → expect `FM-1_46000200  [running]`.

If it fails: exactly as Step 0.7 step 3 ("If it fails"): install refused with `-14` → nothing erased, install
stock V15 (path A) and report; does not boot → fm1-base's SAFE MODE after two failed starts (`--info` answers;
install the prototype, P7, from there), ROM download mode after four → path B.

### Step 2 — On the unit (no command; about 10 minutes)

1. The splash, then **SONGS** and five songs: Amazing Grace, Four Chords in G, Scarborough Fair, Twelve-Bar Blues
   in C, When the Saints Go Marching In; `v0.2.0-m1` top right. **No `RAM ONLY`** (that would mean the flash chip
   was not accepted: stop and report).
2. Turn **SELECT**: the cursor moves. **PLAY** opens the song: "PRESS PLAY", FIRST chord on the right.
3. **Amazing Grace, PLAY:** wood-block count-in (`COUNT-IN 5 4 3 2 1`: one bar and the pickup bar's two empty
   beats), then the pickup note and the band. Top right `bar:beat`. The big chord changes **just before** (never
   after) the change you hear.
4. **Your feedback #1:** in bars 4–5 (G, G) the **bar strip** shows bar 5's G dimmed (held over), and NEXT / the
   countdown (`IN 3`, `IN 2`, `IN 1`) appear only in the last bar before the real change (bar 6, Em). Same in
   bars 15–16.
5. **Mutes:** FX / SEL / ENV / LFO = drums / bass / harmony / melody, any time; a muted layer stops at once (held
   chord too), its box turns amber `MUTE`; unmuting brings it back at its next note.
6. **Levels:** turn K1..K4 (above the layer buttons' row): `L1` ↔ `L2` for drums, bass, harmony (melody has one).
   L2 harmony = inversions with rhythm; L2 bass = root-fifth / walking line.
7. **Tempo:** turn SELECT while playing: 5 % a detent, 50–120 %; the top line shows it (amber when not 100 %).
   The music slows / speeds at once with no jump.
8. **Loops:** SEQ → `LOOP SONG`, SEQ again → `LOOP SECT` (the section now playing repeats: at its end the pickup
   into it is played, then its first bar), SEQ again → off. **ARP** toggles the count-in (`CNT`).
9. **HOME** stops and goes back to the list. Try each song once (the Saints: a 3-beat pickup; the Blues: shuffle
   feel; Scarborough Fair: 3/4).
10. **Record:** the bottom line's `CPU n%` and `V a/b` at the busiest place of **Four Chords in G** and the
    **Blues** (emulator: at most 13 % and 12 voices for Four Chords; see the report for each song), and any
    `SONG ERROR`.
11. **Listen** on the speaker and on headphones: clicks, dropouts, hanging notes after a mute or a stop, the
    count-in level, the tempo steady at 50 % and 120 %. Your impressions of playing along (harmony muted, L1 vs L2).
12. Let one song play to the end: it stops by itself; PLAY plays it again.

If it misbehaves: note what and where (song, bar, which control), then **install the prototype** (P7) to go back
to the known-good firmware:
```bash
.venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-0.7/fm1acc-proto-0.1.0.fwsc
```
(`--info` → `FM-1_4600`), and report.

### Step 3 (optional; the planning chat decides) — back to stock V15 from M1, then M1 again

Not needed to be safe: path A was proven from our firmware in Step 0.7 and the update code is identical (P3). It
would prove it for this build too, at the cost of two more installs (~3 minutes):
```bash
.venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py backups/stock-firmware/FM-1_015.fwsc
```
(`--info` → `FM-1_015`; stock's single `-110` / ~16 s is expected), then step 1 again (`--info` → `FM-1_46000200`).

---

## Restore paths (D015, unchanged)

**Path A — normal return to stock** (any firmware that answers `--info`, or update mode):
```bash
.venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py backups/stock-firmware/FM-1_015.fwsc
```

**Path B — ROM download mode** (the unit does not start, or path A fails): exactly as
[step-0.7-first-writes.md](step-0.7-first-writes.md), "Restore paths" (enter with **REC** held at power-on on our
firmware; B1 `fm1_unbrick.py restore … --verify-v15`; B2 `m-flasher restore` of the full dump). One loader command
per ROM boot session. Neither has been needed so far.

---

## After the user has run it

Claude Code records in `docs/LOG.md`: each command's output, identities, times (kernel log), the CPU / voice
readings, the listening notes and the playing-along impressions; and in `docs/device.md` ("Currently installed"):
the firmware the unit is left on.
