# Milestone M5, parts 1–3 — install on the FM-1 (early checklist 2)

**Status: PREPARED, waiting for review** by the planning chat and the user (docs/plan-M5.md: early install point 2,
after Part 3, trimmed as HANDOFF 2026-11-06 17:15 asks). **It supersedes [step-M5-early-install-1.md](step-M5-early-install-1.md)**
unless that one has been run; then step 1 installs over 0.5.2-m5p2 (FM-1_46000502) instead of M4, with the same
command, and step 2 is only needed if you want the private songs (already there). Nothing has been installed or sent. Claude Code ran no device command while preparing it. **The user
runs every device command** (install, `fm1song send` / `delete`), one at a time, in their own terminal. Claude Code
runs only the read-only ones (`--info`, `fm1song list`, CLAUDE.md) between steps and records.

Prepared 2026-11-06 by Claude Code. The rules, pre-flight and restore paths are the same as the approved M4
checklist ([step-M4-install.md](step-M4-install.md)); what differs is spelled out here. This is the last checklist of M5: parts 4 (views)
and 5 move to the next milestone.

---

## What changes on the unit

| | Now | After |
|---|---|---|
| Firmware | M4, **FM-1_46000500**, v0.5.0-m4 (or 0.5.2-m5p2) | M5 parts 1–3, **FM-1_46000503**, v0.5.3-m5p3 |
| Step 1 writes | — | The app area 0x4000–0x92FFF and the update staging area 0xE0000–0xE4FFF, by our update loader, exactly as before. The loader is unchanged since Step 0.7: `git diff 98fc08c -- firmware/vendor` is empty. |
| Step 2 writes | — | The **song store, 0x97000–0xDBFFF**, by our firmware when `fm1song send` asks. **New in M5:** songs are **format 2.0** (the FM-1 makes the harmony, bass and drum levels itself), **received straight into the flash** (no RAM copy), and the store is in **512-byte blocks**, several songs a sector. A delete writes the record's first 4 bytes to zero instead of erasing. When room is short the FM-1 **compacts**: it may move songs within the store and erase sectors with none left in them. All of this stays inside 0x97000–0xDBFFF (checked before every write; emulator and laptop tests: never outside). |
| Your M4 songs | 9 songs (1.3) + up to 2 private ones | **Still read and played** after step 1 (M5 reads M4's records). Step 2 replaces them with the 2.0 copies. |
| SYNTH mode | — | Writes **nothing** to the flash (its presets are in the firmware; knob changes are not saved). |
| Settings | — | The same two sectors (0xFC000–0xFDFFF). M5's record is larger; **your M4 settings are carried over** (the new ones at their defaults: bass sound +1 OCTAVE, controls hint ON). |

Nothing here is a new *area*; the new kinds of write are inside the song store (writing over a record's magic,
moving songs). Rollback is unaffected (below).

## Pre-flight (any ❌ = stop)

| # | Check | State | Evidence |
|---|---|---|---|
| P1 | Off-laptop backup recorded in `docs/device.md` | ✅ | Unchanged. |
| P2 | USB port + cable | ✅ | Port **1-2**, the new cable. A `-110` = stop and report. |
| P3 | Restore per D015 | ✅ | `firmware/vendor/fm1-base` unchanged since 98fc08c (M5 adds `firmware/vendor/felucca-engines`, MIT synth code compiled into the app only). |
| P4 | The package: commit + sha256 + emulator | ✅ | `build/step-M5/part3/fm1acc-0.5.3-m5p3.fwsc`, sha256 `ee11165d869a543818d0529bbf4007547f3509330507787dada05b4e6a90d4ce`, identity FM-1_46000503, from the Part 3 commit. Emulator: the **full suite** on this exact package (`build/step-M5/part3/emu/report.md`): **286 checks, ALL PASS** (M5 parts 1–3, every M3/M4 check that still applies, the store with power cuts, flash ids, the 5 private songs, the mode screen and SYNTH). SYNTH on the laptop (`tools/vsynth_test.sh`, ASan/UBSan): all 21 presets sound, none clips, every voice ends, the arpeggiator steps. Laptop: pytest 93 pass (incl. the C/Python golden test: identical events for all 14 songs, every level); `tools/store_test.sh` ALL PASS (a power cut at every one of 139 operations while compacting, 0 wrong); fuzz 10 min: 768,000 files, no sanitizer report. |
| P5 | Flash IDs | ✅ | Emulator with 0x856014 and 0xB36014: same screens, sound, songs stored. Unknown 0xC84017: `NO SONG STORAGE`, keys sound, a send refused, nothing written, no crash. |
| P6 | Unit state before step 1 | (check) | `--info` must print `FM-1_46000500  [running]` (M4), or `FM-1_46000502` after checklist 1. |
| P7 | Fallback package | ✅ | M4, known good on this unit: `build/step-M4/final/fm1acc-0.5.0-m4.fwsc`, sha256 `c811e0f7f002a2d7573843394e812490dbe3c2709c425d4b457c2351f767195b`. **M4 cannot read 2.0 songs nor M5's store layout** (it sees an empty store: it reads only whole-sector `FM1R` records). After a fallback: `tools/fm1song.sh send songs/*/` again; `fm1song` sees an M4 and sends **1.3** (every level as events, as M4 had them). The private songs whole don't fit 1.3 (over 16,384 events): they can't go back to M4. |
| P8 | The songs to send | ✅ | `songs/*/`: 9 songs in 2.0, 11,312 bytes together (M4: 168 KB). Your 5 private songs, **whole** now (no bars cut), 2.0: 8.7–46 KB each. **All 14 fit together**: 252 of 552 blocks, 300 free (emulator). |

## Budget (M5 parts 1–3)

| | M4 | M5 parts 1–3 | Room |
|---|---|---|---|
| App image (slot ~581 KB) | 419,056 B | **448,736 B** | ~132 KB |
| RAM .data+.bss (of 98,304) | 74,836 | **76,032** (the synth's 8 voices) | 22 KB |
| RAM pool (of 344,064) | 284,936 | **289,312**: the song buffer 192 KiB (one level a layer; was 128 KiB for every level), no 64 KiB receive buffer (songs go straight to flash); **SYNTH's delay and chorus lines (132 KiB) reuse the song buffer** | 54 KB |
| Song store (552 blocks of 512 B) | 9 songs (1.3), 45 of 69 sectors | 9 songs 28 blocks; with the 5 private songs 252 of 552 | 300 blocks |
| CPU, SYNTH | — | 8 voices of SYNC SCREAM with chorus and delay: the longest render 24 % of its block (emulated) | |

## Rules for every step

As M4's ([step-M4-install.md](step-M4-install.md), "Rules for every step"): one command at a time, you run it, Claude
Code checks with `--info` / `fm1song list` before the next.

---

## Checklist

### Step 1 — Install M5 parts 1–3 over M4

1. Claude Code runs `--info` (read-only) → `FM-1_46000500  [running]` (or `FM-1_46000502`).
2. The package is the reviewed one:
   ```bash
   sha256sum build/step-M5/part3/fm1acc-0.5.3-m5p3.fwsc
   ```
   expect `ee11165d869a543818d0529bbf4007547f3509330507787dada05b4e6a90d4ce`.
3. Install (you run it; about 1–2 minutes):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M5/part3/fm1acc-0.5.3-m5p3.fwsc
   ```
4. Claude Code runs `--info` → expect `FM-1_46000503  [running]`. The splash (now 4 s, with a short start-up
   melody) shows `v0.5.3-m5p3`, then the **mode screen**: press PLAY for LEARN.
5. Your M4 songs are still in the list and play (M5 reads M4's store).

If it fails: as M4's step 1: install M4 (P7) or stock (path A) and report.

### Step 2 — Send all the songs (format 2.0)

1. Choose **LEARN** on the FM-1 (sends are refused in SYNTH mode). Claude Code runs `tools/fm1song.sh list` → `FM-1_46000503`, `reads format 1.0..1.3 and 2.0`, your M4 songs.
2. You send ours (writes the song store only; each M4 copy is replaced):
   ```bash
   tools/fm1song.sh send songs/*/
   ```
   expect nine lines `stored`.
3. You send the five private ones, whole:
   ```bash
   tools/fm1song.sh send songs/private/*/
   ```
   expect five lines `stored`. (If the FM-1 shows `MAKING ROOM` for a few seconds: it is compacting.)
4. Claude Code runs `fm1song list` → 14 songs, each `ok`, about 300 of 552 blocks free.

### Step 3 — On the unit (no command; about 30 minutes)

**Part 1: fixes**
1. **Splash:** 4 s, a 5-note start-up melody, not louder than the count-in at MEDIUM.
2. **Controls hint:** opening a song first shows the controls hint; any button closes it. It comes up to 10 times,
   then stops (Settings → "Controls hint" turns it off or on). The stopped legend alternates with "FX-LFO press
   volume / hold your part".
3. **Fresh start:** change a song's view, key and tempo, go HOME, open it again: its own view, key and tempo.
4. **Loudness:** K3 (harmony) 1 → 5, K2 (bass) 1 → 4, K1 (drums) 1 → 3: each level about as loud as the next.
5. **Let It Be** (private) harmony L3: the piano part only (no organ holding every chord).
6. **Bass:** in its real octave (E1–D#2), sounding an octave up by default; Settings → "Bass sound" REAL / +1
   OCTAVE. Bass tab: real notes, labels and a fingering that stays in one hand position. Bass notation: an octave up
   (the bass-guitar convention).
7. **Harmony L5:** the arpeggio's notes light bright as played, the rest of the chord at the middle level.
8. **Layout:** PLAY / STOP no longer overlaps; the track boxes read `L1 FULL` with the level in white and
   FULL / QUIET in gray.

**Part 2: storage**
9. **Every song opens and plays** at every level. Turn a level knob while playing: that layer changes at once, the
   others don't stop or stutter.
10. **The private songs play to their real end** (Creep 90 bars, Rockin' 147, Zombie 213), at harmony L5, bass L4,
    drums L3.
11. **Compare one song's levels with M4** by ear (e.g. Amazing Grace): they should sound the same (the FM-1 now
    generates them by the same rules; the emulator found identical notes).
12. **Record** CPU and voices (GLO) on Zombie (213 bars, 165 BPM) at the top levels, and your impressions.

**Part 3: the mode screen and SYNTH** (switch off and on; controls in [controls.md](controls.md), "Part 3")
13. After the splash: the mascot, LEARN / SYNTH, the cursor on LEARN. SELECT to SYNTH, PLAY.
14. **Presets:** turn SELECT through all 21; play chords. Do they sound like synthwave? Note any too loud, too
    soft, or harsh. (RISER has a slow 6 s attack: hold the keys.)
15. **Knobs:** K1 cutoff, K2 resonance, K3 attack, K4 release, ALGORITHM chorus, PRESET delay.
16. **Arpeggiator:** ARP on, hold a chord; SEQ the pattern, GLO the rate; hold ARP and turn SELECT for the tempo.
    OCTAVE BOUNCE, CHIP BLIP, LASER ARP and GRID PULSE start with their own arpeggio.
17. **8 voices:** hold 8 keys on SYNC SCREAM: no clicks or dropouts? OCT- / OCT+ shift the keys.
18. **Back to learning:** switch off and on, choose LEARN: the songs as before.

If it misbehaves: note what and where. To go back, you install M4 (P7):
```bash
.venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-M4/final/fm1acc-0.5.0-m4.fwsc
```
Then you send our songs again; M4 gets 1.3 (M4 sees M5's store as empty):
```bash
tools/fm1song.sh send songs/*/
```

---

## Restore paths (D015, unchanged)

As [step-M3-install.md](step-M3-install.md).

## After the user has run it

Claude Code records the outputs, identities, the `fm1song list` output, readings and impressions in `docs/LOG.md`,
and the firmware, songs and settings in `docs/device.md`.
