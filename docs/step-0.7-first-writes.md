# Step 0.7 — First writes to the FM-1 (checklist)

**Status: APPROVED** by the planning chat with the user (HANDOFF 2026-11-04 20:40), with the rules it added
below. **The user runs every device command**, one at a time, in this order, pasting each step's output to
Claude Code before starting the next.
Claude Code ran only read-only commands (`lsusb`, kernel log) while preparing it.

Prepared 2026-11-04 by Claude Code. Sources for every claim are given inline (file:line at a pinned commit).

---

## Pre-flight (any ❌ = stop)

| # | Check | State | Evidence / what is missing |
|---|---|---|---|
| P1 | Off-laptop backup copies recorded in `docs/device.md` | ✅ (one copy) | Recorded 2026-11-04: one off-laptop copy on the user's USB thumb drive (kept unplugged), checked by the user; Claude Code did not re-read its sha256. **No cloud copy** (`docs/device.md` asks for three copies: the repo, another disk, cloud). The user chose to go ahead with this one copy; the planning chat may still want the cloud copy before step 1. |
| P2 | A USB port + cable that enumerates cleanly | ✅ (accepted by the user) | Not met as written: every try shows one `device descriptor read/64, error -110` and ~16 s to enumerate (port 1-2 old cable, port 1-1 old cable, port 1-2 new cable; `docs/device.md`, "USB for writes"). Same on two ports and two cables, so it is the unit's own USB start-up; it always recovers, and the installer waits 30 s / 40 s for the two re-enumerations (`fm1_install.py:42-44`). **The user accepted this (option a), 2026-11-04.** Use port **1-2** with the **new cable**. A *second* `-110`, or more than 25 s to appear, during any step = stop. |
| P3 | Restore per D015 ready | ✅ | See "Restore paths". The normal path back to stock needs **no override flag**: the installer we use accepts the unmodified official V15 by its sha256 (`firmware/vendor/fm1-base/bsp/tools/fm1_install.py:36-38,377-381`; ours `db1642b2…` = its `STOCK_V15_SHA256`). Our app's update code stages M-VAVE's V15 package (host test, below). |
| P4 | Round-trip community firmware accepts 0x856014 (app and loader) | ✅ | **Felucca 1.8.1** (identity FM-1_918). App: `firmware/src/project.c:1195` at tag v1.8.1 (f4df296): `flash_ok = FL_FAR(fl_jedec_ram)() == 0x856014u` (else RAM-only). Loader: Felucca's own `firmware/loader/ldr_core.c` (same tag) has **no** flash-ID check: it checks the package (board, chip key 0x980F, CRC16) and writes only the app area 0x4000–0x92FFF (lines 10–22, 113–132). So 0x856014 is accepted by both. |
| P5 | Our prototype `.fwsc`: commit + sha256 of the build that passed Part A | ✅ | Commit `8bc977e`, `firmware/app/build/fm1acc.fwsc` sha256 `58be4537be314bf463dc828c919b18675c2626d01be5a01724560b3cc760d54f`, identity **FM-1_4600**, v0.1.0-proto. Emulator report: `docs/step-0.7-emulator-report.md` (all PASS). Copy kept as `build/step-0.7/fm1acc-proto-0.1.0.fwsc` (+ `.sha256`). |
| P6 | Unit state before step 1 | (check) | `--info` must print `FM-1_015 [running]` (below, "Read-only checks"). |

### P2 procedure (HISTORICAL: P2 was decided 2026-11-04, see P2 above and `docs/device.md`, "USB for writes")

For each candidate port (not 1-2) and, if needed, another **data** USB-C cable:

1. Unplug the FM-1. Note the time. Plug it into the candidate port (unit on, running stock V15).
2. Within ~5 s run:
   ```bash
   sudo dmesg | tail -n 15
   ```
   (or `journalctl -k -n 15 --no-pager` if dmesg needs root). **Clean** = `new full-speed USB device …` followed
   within 1–2 s by `New USB device found, idVendor=4c4a, idProduct=c755` and **no** `error -110` line.
3. ```bash
   lsusb | grep 4c4a
   ```
   expect `ID 4c4a:c755  FM-1`.
4. ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py --info
   ```
   (read-only: sends only the identity query) expect `FM-1_015  [running]`.
5. Repeat 1–4 **three times** on the same port and cable. All three clean → that port + cable is the one.
   Record in `docs/device.md` (new section "USB port for writes"): port (e.g. `1-1`), which physical socket,
   which cable, the three dmesg excerpts. Use only that port and cable for every step below.

If no port is clean: stop; report the dmesg lines to the planning chat (a powered hub or another computer
may be next).

---

## The packages

| Package | Identity | sha256 | Source | Emulator |
|---|---|---|---|---|
| `third_party/firmwares/felucca-1.8.1.fwsc` | FM-1_918 | `187d4a9bc5b555866848d8e2c9964bea134d764cdeb62712a30da1825c5caf15` | Felucca web installer's file, `gh-pages` branch commit 1db38f8 (2026-11-01, "Felucca 1.8.1: the installer, the editor and the emulator"), git blob a85155fb = GitHub's | boots, sound (fm1-live 4 s) |
| `backups/stock-firmware/FM-1_015.fwsc` | FM-1_015 | `db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a` | Step 0.2 (= fm1-linux-update's pinned value) | (stock boots in 0.4) |
| `build/step-0.7/fm1acc-proto-0.1.0.fwsc` (our prototype) | FM-1_4600 | `58be4537be314bf463dc828c919b18675c2626d01be5a01724560b3cc760d54f` | commit `8bc977e`, `make firmware` | Part A, all PASS |
| `build/hello/fm1acc-hello.fwsc` (0.6 hello world, fallback only) | FM-1_7100 | `e191c005ba54542857d48a01a4944956fa3e1c97e0efd418f198142de7121a1c` | `tools/build_hello.sh` (commit 4629478's source) | Step 0.6 |

**Why Felucca 1.8.1 and not a GitHub release:** 1.8.1 has a git tag but **no GitHub release** (the releases page
lists 1.7.0 as the latest stable and 1.8.2-beta as a prerelease). Felucca distributes 1.8.x through its web
installer, whose package file is the one above. The alternative is **1.7.0** (GitHub release, sha256
`bff7c617…817c` = GitHub's digest, already on disk, emulator-tested in 0.4; its app has the same 0x856014 check at
`project.c:1162`). The planning chat picks; this checklist uses 1.8.1.

**Which installer:** `firmware/vendor/fm1-base/bsp/tools/fm1_install.py` (felucca-bsp 773b0b99, vendored in Step
0.6, MPL-2.0). Checked offline (it only parsed the files): it accepts Felucca 1.8.1, stock V15, our prototype and
the hello world **without** `--force`, and **refuses** V16 (`FM-1_016.fwsc`) — D012 is safe by construction. (The
older Felucca `fm1_install.py` in `third_party/fm1-linux-update/.cache/` needs `--force` for stock V15; not used.)

**How an install works** (`bsp/chip/ota.c:3-13`, the installer's docstring): step 1, the *running* firmware checks
the package and stages **the package's own** update loader at 0xE0000, then restarts; step 2, that loader writes
the app area and restarts into the new firmware. So each package is written by its own loader: Felucca's by
Felucca's, stock V15 by M-VAVE's, ours by ours (felucca-bsp's, patched to refuse unknown flash chips). What the
*running* firmware must do is only step 1.

**Our firmware can hand over to stock V15** (step 4 below) — checked on the laptop with fm1-base's own host test
of `bsp/chip/ota.c` (`tests/ota_test.c`, a simulated FM-1 and the real SysEx protocol), our vendored copy:
`FM-1_015.fwsc over another head commits ok` and `... with a damaged boot area -> -5 ok`. (Felucca 1.7.0 in the
"stock" slot of that test fails the damaged-boot-area case only because Felucca's own loader never writes the
boot area, so the test's stock-only rule does not apply; irrelevant here.)

---

## Rules for every step

- **The user runs every command, in their own terminal tab.** Claude Code only reads (`--info`, `lsusb`,
  kernel log) between steps and records.
- Before starting: **disable the laptop's automatic suspend / sleep** for the session; do not close the lid.
- Before starting: **close fm1-live / the emulator, `aplaymidi` and any other MIDI apps** (fm1-live with
  `--midi all` subscribes to ports).
- The FM-1 stays on USB power the whole time; **never switch it off during a write** (only where a step
  says to power-cycle, after the install has finished).
- Stop and report on any unexpected output.
- Use only the port + cable from P2 (port 1-2, the new cable). Expect one `-110` and ~16 s at each
  (re)connect; that is this unit. Laptop on mains power. Do not touch the FM-1 or the cable during a write.
- **One install command per update.** After each, run `--info` before anything else.
- **One loader command per ROM boot session** (ROM boot = the chip's download mode, USB `4c4a:8057`,
  "WL82 UBOOT1.00"): enter it, run exactly one of `fm1_unbrick.py info | backup | restore` or
  `m-flasher restore`, then power the FM-1 off and on. Never chain two.
- Never use `--force`, `--force-write`, `--i-know`, `--skip-check` or any other override.
- Do not install FM-1_016 (D012). The installer refuses it anyway.
- Log every command's full output in the terminal; paste it to Claude Code afterwards for `docs/LOG.md`.

Read-only checks used below:

```bash
.venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py --info
```
```bash
lsusb | grep 4c4a
```

If `--info` finds more than one MIDI port that could be the FM-1, add `--port NAME` (part of the name it prints).

---

## Checklist

### Step 1 — Community firmware: Felucca 1.8.1

1. `--info` → expect `FM-1_015  [running]`.
2. Install (writes the app area 0x4000–0x92FFF through Felucca's loader; asks for confirmation — answer `y`):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py third_party/firmwares/felucca-1.8.1.fwsc
   ```
   You should see: the identity of the unit, the package's identity FM-1_918, a confirmation prompt, step 1
   (the running stock firmware reads parts of the package), "loader", step 2 (the whole package is read; a
   progress count), the restart, and the identity check ending OK. The FM-1's screen shows the update, then
   Felucca. **Time:** about 1–3 minutes (not yet measured on this unit; the installer waits up to 180 s idle
   while writing).
3. `--info` → expect `FM-1_918  [running]`.
4. **Boots:** Felucca's screen; a key plays a sound (speaker).
5. **A setting survives a power cycle** (non-blocking): save something with Felucca's **SAVE** button (or change
   the palette preference), note what and its old/new value, wait 5 s, switch the FM-1 off, wait 10 s, switch
   it on. Expect the new value. Record what was tried.

If it fails:
- **The installer stops before "loader"** (refused, timeout, port lost): nothing was written (step 1 writes only
  the staging area, which is cleared on failure: `ota.c:9-13`). Run `--info`; expect FM-1_015. Note the message;
  stop and report.
- **Cut off during step 2** (cable, power): the update record stays, so the loader runs again at the next
  power-on; re-run **the same** install command, which finishes the write (installer docstring: "If the FM-1 is
  still in update mode … the install finishes the write"). Then `--info`.
- **Installed but does not boot / no sound:** power-cycle once. Still not: go to "Restore paths", path A
  (stock V15 via the normal path, if the unit answers `--info` in any mode), else path B.
- **Setting not kept:** not a stop for us (Felucca's storage, not the write path), but record it; continue to
  step 2.

### Step 2 — Back to stock V15 (from Felucca)

1. `--info` → `FM-1_918  [running]`.
2. Install stock (M-VAVE's own loader writes it; no override needed):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py backups/stock-firmware/FM-1_015.fwsc
   ```
   Expect the same sequence as step 1 with package identity FM-1_015. **Time:** about 1–3 minutes.
3. `--info` → expect `FM-1_015  [running]`. The stock screen and sound as before.

If it fails: as step 1. If the unit stays in update mode and re-running the command does not finish: "Restore
paths", path B.

### Step 3 — Our prototype

1. `--info` → `FM-1_015  [running]`.
2. Check the package is the reviewed one:
   ```bash
   sha256sum build/step-0.7/fm1acc-proto-0.1.0.fwsc
   ```
   expect `58be4537be314bf463dc828c919b18675c2626d01be5a01724560b3cc760d54f`.
3. Install (our loader: felucca-bsp's, patched to write only to 0x856014 / 0xB36014 flash):
   ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/step-0.7/fm1acc-proto-0.1.0.fwsc
   ```
   **Time:** about 1–3 minutes.
4. `--info` → expect `FM-1_4600  [running]`.
5. On the unit: the splash, then "PRESS PLAY", `v0.1.0-proto`, "FIRST G". No "RAM ONLY" in the top line (that
   would mean the flash chip was not accepted: stop and report).
6. **Play:** press PLAY. Expect: two beats of silence, the pickup note (flute), then the band (drums, bass, electric
   piano, flute); top right `bar:beat` counting (`1:1`, `1:2`, …), the current chord large (`G`, `G7`, `C` …),
   the next chord below it. PLAY lights green.
7. **Mutes:** FX = drums, SEL = bass, ENV = harmony, LFO = melody. Each press toggles; lit = playing. Mute
   harmony (ENV): the piano stops at once (held chord too) and the HARM box turns amber "MUTE". Unmute: it comes
   back at the next chord. Try each layer.
8. **Chord on time:** watch the chord against the bass: the new chord should appear just before (never after) the
   change you hear. (Emulator: 20–80 ms before; see the report.)
9. **MASTER** changes the volume. The keys play an electric piano over the song (channel 16).
10. **Record:** the `CPU n%` and `V a/b` (voices now / most) shown in the top line, at the busiest moment (around
    bar 16–17, the turnaround into verse 2); the emulator measured ~14 % and 14 voices there.
11. **Listen** on the **speaker** and on **headphones**: clicks, crackles, dropouts, distortion at high MASTER,
    hum or noise in quiet parts; tempo steady; is the drum feel tight or loose (notes start on 5.8 ms
    boundaries)? Note your impressions of playing along (piano with harmony muted).
12. Play to the end (~1 min 12 s): it stops by itself; PLAY plays it again.

If it fails:
- **Install refused with code -14** after the chip-key check: our loader did not recognise the flash chip;
  nothing was erased, the unit stays in update mode. Do not retry. Install stock V15 (step 4 command) — stock's
  own loader writes it — and report.
- **Does not boot, or crashes:** fm1-base's rescue: two failed starts in the first 30 s → **SAFE MODE** (no sound,
  USB on; `--info` answers): go to step 4 from there. Four failed starts → the ROM download mode by itself →
  "Restore paths", path B.
- **Boots but misbehaves** (no sound, wrong screen, noise): install the **hello world** to tell hardware from
  prototype problems:
  ```bash
  .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py build/hello/fm1acc-hello.fwsc
  ```
  (`--info` → FM-1_7100; a held key plays its note on Electric Piano 1 and shows its name.) Hello world fine →
  a prototype problem (report; continue with step 4). Hello world also bad → hardware/platform problem (report;
  continue with step 4).

### Step 4 — Back to stock V15 from OUR firmware

This proves we can always leave our firmware by the normal path.

1. `--info` → `FM-1_4600  [running]` (or FM-1_7100 if the hello world is on).
2. ```bash
   .venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py backups/stock-firmware/FM-1_015.fwsc
   ```
   Our app does step 1 (checks the package, stages M-VAVE's loader); M-VAVE's loader writes. **Time:** about
   1–3 minutes.
3. `--info` → expect `FM-1_015  [running]`.

If it fails: re-run the same command once if it was cut off. If our app refuses the package (an error before
"loader"), nothing was written: hold **REC** at power-on (our firmware's rescue: the chip's ROM download mode) →
path B. Report the exact message.

### Step 5 — Our prototype again, left installed

1. `--info` → `FM-1_015  [running]`.
2. Same as step 3, items 2–7 (sha256, install, `--info` → `FM-1_4600`, play once, one mute).
3. Leave it installed. Done.

If it fails: as step 3. If in doubt, leave the unit on stock V15 (step 4) and report.

---

## Restore paths (D015)

**Path A — normal return to stock** (the unit runs any firmware that answers `--info`, or is in update mode):
```bash
.venv/bin/python -I firmware/vendor/fm1-base/bsp/tools/fm1_install.py backups/stock-firmware/FM-1_015.fwsc
```

**Path B — through the ROM download mode** (the unit does not start, or path A fails). One loader command per
session; power-cycle between sessions.

1. Enter ROM download mode:
   - our firmware: hold **REC** while switching on; or let it fail 4 times;
   - stock or Felucca, running: the soft key (sends `F0 22 24 35 7D F7`, a reboot into the ROM mode):
     ```bash
     .venv/bin/python -I third_party/MvaveFM1Unbricker/fm1_unbrick.py softkey --port FM-1 --log-dir backups/flash/logs
     ```
   Then wait until `lsusb | grep 4c4a` shows `4c4a:8057` (up to ~20 s; on port 1-2 it took 15–20 s with `-110`).
2. **B1 — stock V15 package through ROM** (writes the firmware region at 0x4000 after its own backup and a typed
   confirmation; boot head never touched):
   ```bash
   .venv/bin/python -I third_party/MvaveFM1Unbricker/fm1_unbrick.py restore backups/stock-firmware/FM-1_015.fwsc --verify-v15 --log-dir backups/flash/logs
   ```
   Then power off, on; `--info` → FM-1_015.
3. **B2 — last resort, bit-exact: the full dump** (M-Flasher 0.8.2, `third_party/m-flasher` 55da95c; writes only
   the sectors that differ from the dump, after showing them and a typed `yes`; brings back calibration, Bluetooth
   address and settings exactly):
   ```bash
   third_party/m-flasher/target/release/m-flasher restore backups/flash/FM-1_015_full-flash_856014_2026-11-04.bin
   ```
   Then power off, on; `--info` → FM-1_015.

Neither B1 nor B2 has been run on this unit yet; B2 is the bit-exact way back (`docs/device.md`, Restore
options). The udev rule from Step 0.3 (`70-m-flasher.rules`) gives both tools access without sudo.

---

## After the user has run it

Claude Code records in `docs/LOG.md`: each step's command output, identities, times, the device's CPU load and
voices, the listening notes (speaker, headphones) and the user's impressions of playing along; and in
`docs/device.md`: the USB port + cable, the backup copies, the firmware the unit is left on.
