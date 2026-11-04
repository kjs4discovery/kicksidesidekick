# Step 0.5 — What to build our firmware on

Prepared by Claude Code, 2026-11-04. Read the source, built where possible, ran builds in fm1-emu only.
**Nothing was installed on the FM-1.** This is input for the decision; it does not make it.

## What we need from a base

Play a compiled song (drums, bass, harmony, melody, variants) in time, mute one layer, draw the chord
and a keyboard diagram, take songs from the laptop over USB, keep a USB rescue, accept flash
0x856014 **and** 0xB36014 in the app **and** the update loader and refuse unknown IDs without
crashing (CLAUDE.md), run in fm1-emu, and leave room for a second mode later.

## The candidates

All cloned into `third_party/` (git-ignored). Commits as read:

| | Repo | Commit | Licence |
|---|---|---|---|
| A | joakimeriksson/fm1-dock | 94f8087 (2026-11-02), v0.7.1 | GPL-3.0-only |
| B | hugelton/Felucca (+ felucca-bsp, Felucca-Engines) | a4f007c (2026-11-04), v1.8.1 / 1.8.2-beta | GPL-3.0-only (bsp MPL-2.0, engines MIT) |
| B′ | charlesvestal/fm1-base (on felucca-bsp) | 2d8ed74 (2026-11-01), no release (beta) | MPL-2.0 |
| C | m-vave/fm1_open | 4c7496d (2026-10-21), V14 | GPL-3.0 + FM1 SDK linking exception; binary engine under separate terms |
| D | charlesvestal/fm1-polly (on fm1-base) | 853ebd7 (2026-11-01), v0.6 | GPL-3.0-only (base MPL-2.0, Sonivox EAS Apache-2.0) |
| — | konsumer/foma | 69a2375 (2026-10-31), no release | GPL-3.0 |
| ref | Reytian/fm1-trio (on fm1-base) | 7c92b7a (2026-10-20), v0.1 | MPL-2.0 |
| ref | math0ne/fm1-chord, FicusFicusov/CoralFlower (Felucca forks) | 5117c27, 272cad9 | GPL-3.0-only |

B′ is listed because "Felucca with felucca-bsp" today mostly means fm1-base: the MPL-2.0 platform
layer cut out of Felucca/X0X/FoMni with the authors' permission (felucca-bsp#3, #4), which Polly,
TRIO, LOOM-8, CTL-1, Erik37, J6 and others are built on.

## Toolchain (this laptop: Linux Mint 22.3, x86-64)

Shared by A, B, B′, D, foma, TRIO, fm1-chord, CoralFlower:
- JieLi Linux toolchain, clang 4.0.1 for pi32v2: `https://pkgman.jieliapp.com/s/linux-toolchain` →
  `jieli-linux-toolchains-20260730.1.tar.xz`, 26,027,508 B, sha256
  `a1545cddbc451a06bac43eb5cd96d831470c42a97c5c0b1f98879a20fc7647bd` (no project pins a hash). In
  `third_party/jieli/toolchain`. Runs natively here; **no Docker needed on Linux x86-64**.
- JieLi AC79 SDK `AC79NN_SDK_V1.2.1_2023-12-13` from gitee (1.1 GB checkout; the build uses three
  files: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin`, Apache-2.0). In `third_party/jieli/`.
- Python with Pillow and fontTools (installed in `.venv`).
- Nothing needs sudo or system packages.

| | Builds on Linux? | Time | Notes |
|---|---|---|---|
| B Felucca | **yes** (built 2026-11-04) | **31 s** | app 517,616 B, loader `ota.bin` 7,130 B, `felucca.fwsc` 610,286 B (FM-1_900). RAM .data+.bss 62,324 of 73,728 B; pool 326,236 of 368,640 B. **Boots and plays in fm1-emu** (C4: same level as the 1.7.0 release). |
| B′ fm1-base | **yes** | **16 s** | template: `template.fwsc` 611,496 B (FM-1_1100). Image 221,896 B; RAM .data+.bss 76,388 of 98,304 B; pool 93,960 of 344,064 B (the most room left). Boots and plays in fm1-emu (2.5× real time). |
| D Polly | **yes** | **31 s** | `polly.fwsc` 611,496 B (FM-1_300). Image **574,704 B** (≈99 % of the ~581 KB app slot); RAM **95,840 of 98,304 B**; pool 289,540 of 344,064 B. Boots and plays in fm1-emu (3.3×; its styles live in the separate resource region, not loaded in this test). |
| A fm1-dock | **yes** | **52 s** | `fm1-dock.fwsc` 610,130 B (FM-1_921). Host 61,400 of 196,320 B; RAM 45,508 of 98,304 B; pool 46,080 of 65,536 B, then the module RAM. Boots and plays in fm1-emu (1.15×). |
| TRIO (ref) | **yes** | 19 s | `trio.fwsc` (FM-1_5200). Image 377,584 B; RAM 84,824 of 98,304 B. Boots and plays in fm1-emu. |
| fm1-chord / CoralFlower (ref) | **yes** | 49 s / 53 s | images 536,396 / 366,452 B; RAM 94,996 / 92,268 of 98,304 B. Both boot and play in fm1-emu (CoralFlower makes sound at idle). |

All of these were built by Claude Code with `tools/build_bases.sh` after the user asked it to
(2026-11-04); logs in `build-logs/`. Emulator test: `fm1-live --bench 4000`, C4 held 2.0–2.6 s,
audio level and panel checked.
| C FM1 Open | **no**: Windows x64 only | — | PowerShell build, JieLi **Windows** toolchain 2.5.3, 22 toolchain files checked against `sdk/manifest.json`. Its web preview builds from C with WASI SDK. Not attempted. |
| foma | not here | — | `./fm1` always runs Docker (image `konsumer/foma-toolchain`); Docker is not installed. A native build would need its Dockerfile's steps by hand. Not attempted. |

## Flash chip: 0x856014 and 0xB36014

| | App | Update loader | To add 0xB36014 |
|---|---|---|---|
| A fm1-dock | `FL_JEDEC_OK(id) == 0x856014` (`firmware/hal/fm1_flash.h:47`; SMK-37: also 0xEB6014) | **refuses** unknown chips: `ldr_flash_known()` → `FL_JEDEC_OK` (`firmware/loader/loader.c:86`, `ldr_core.c:131`) | easy in source (one macro used by both), but our own build of the loader must then be the one installed |
| B Felucca 1.8.2-beta | **accepts both**: `flash_id == 0x856014u \|\| flash_id == 0xB36014u` (`firmware/src/project.c:1196`); else RAM-only | felucca-bsp loader: **no flash-ID check at all**; it checks the package decrypts with this chip's key, writes only 0x4000–0x92FFF, never the head | done in the app (beta; "testing on the revised hardware is still pending", issue #320). Loader: accepts any chip, so "refuse unknown" would need a check added |
| B′ fm1-base | `== 0x856014u` only (`device/main_fm1.c:897`), else RAM-only (no crash) | felucca-bsp loader, as B | one line in the app; a loader check to add for "refuse unknown" |
| C FM1 Open | binary engine; no ID constant found in `libfm1_engine.a` | M-VAVE's binary `sdk/tools/pack/ota.bin` (Apache-2.0); no ID constant found | **unknown, can't fix**: no source. The unbricker's README reports 0xB36014 units "locked into firmwares they can't flash out of" with the stock update path |
| D Polly | as B′ (its `base/device/main_fm1.c:897`) | felucca-bsp loader | as B′ |
| foma | `== 0x856014u` (`platform/fm1.c:545`, `src/recovery.c:59`) | refuses: `ldr_flash_known()` `== 0x856014u` (`platform/loader/loader.c:73`) | easy in source |
| TRIO | `== 0x856014u` (`base/device/main_fm1.c:708`) | felucca-bsp loader | as B′ |
| fm1-chord, CoralFlower | `== 0x856014u` | Felucca's loader of their time | — |

Our unit has 0x856014, so every candidate runs on it today. The 0xB36014 rule matters for other
units and for our own tests (fm1-emu needs the JEDEC patch planned for Step 0.6).

## Rescue

| | What it keeps |
|---|---|
| A fm1-dock | **OCT+ at power-on**: safe start (sine only, no radio). **OCT− at power-on**: USB rescue (no sound, only the updater). **HOME at power-on**: stay in fm1-dock. A module that crashes twice is skipped at boot; one slower than real time is stopped and the sine plays. After a dual-core crash it runs on one core. Crash record and log in the browser console. |
| B Felucca | Update entry over USB-MIDI; the web installer offers a way back to V15. BUILDING.md: if an install fails and the FM-1 no longer starts, "recovery needs FM-1-transporter". felucca-bsp's `tools/fm1_rescue.sh` (ROM boot, stock firmware back). |
| B′ fm1-base / D Polly / TRIO | felucca-bsp's loader (resumes a cut-off update after power loss), crash record (`plat_last_crash`), safe-mode / crash / update / restore screens, `tools/fm1_rescue.py` (downloads jl-uboot-tool's loader and V15, checked by hash) |
| C FM1 Open | M-VAVE's standard update protocol and workbench (back to V15, migration from some third-party firmwares) |
| foma | Crash: red screen with the address, restart; **two crashes at boot → USB rescue by itself**; **OCT− at power-on**: USB rescue; Rescue tab finishes a cut-off install and installs V15 (hash-checked). On its `mvave` platform: no OCT− rescue. |

## Display API (240×240 RGB565)

| | API | Fonts | Keyboard diagram |
|---|---|---|---|
| A fm1-dock | the host's `lcd.c`/`gfx.c` (Felucca lineage) for modules; modules draw their own screens | Felucca's | easy (rects + text) |
| B Felucca | own UI layer (`gfx.c`, `ui_*.c`), host-side UI tests render every screen to PNG and check clipped/overlapping text | UI font and icons rasterised at build time (Pillow, fontTools) | easy; RAM is the limit |
| B′ fm1-base / D / TRIO | canvas (`device/gfx.c`: `cv_rect`, `cv_line`, `cv_text`, text boxes, palette), drawn in bands if RAM is short; same code in the host simulator (screenshots) and the browser | Terminus 12/16/24/32 px (BDF), Barlow Semi Condensed (TTF, rasterised at build); `FONT_M`, `FONT_L` | easy: rectangles and labels |
| C FM1 Open | `app/screen.c` (154 KB, one file) | its own | possible, harder to work in |
| foma | `clear`, `rect`, `line`, `pixel`, `text` (`platform/fm1.h`) | FONT_BIG etc. | easy |

## Audio: instruments, voices, second core

| | Drums | Bass | E-piano | Lead | Voices / cores |
|---|---|---|---|---|---|
| A fm1-dock | per module | per module | per module | per module | each module brings its own engine; 272 KiB RAM a module; second core used by CS-1, FM-106 |
| B Felucca | DRUM (+ samples) | ANALOG | FM6 (DX7 conversion) / SAMPLE (CC0 piano) | ANALOG, FM6, PHYS | 4 parts; second core optional (off by default); RAM nearly full |
| B′ fm1-base | — | — | — | — | **no sound engine**: bring one (`app_render`); second core API `plat_cpu2_*` |
| C FM1 Open | 808, sample drums | 303 | DX7 (msfa, 128 voices, M-VAVE's V15 factory set) | DX7, PSG, pluck | 8 tracks; stock msfa up to 12 voices; binary engine |
| D Polly | **General MIDI** (Sonivox EAS, 200 KB GM set, Apache-2.0): every GM instrument + kits on ch 10 | GM | GM (EP1/EP2) | GM | `MAX_SYNTH_VOICES 32`, EAS at 22.05 kHz doubled to 44.1; Status page shows load and voices; one core |
| foma | — | — | — | — | sine helpers only |
| TRIO | kit (network-driven) | upright | piano | — | one core; networks in the main loop |

## Sequencing / timing

- **D Polly**: `app/dsp/arr.c` (2,713 lines). 480 PPQ; runs in the audio render between EAS blocks
  (sample-accurate: EAS modified so a note-on starts at its sample within a block); tempo 40–240;
  MIDI clock in (with catch-up rules); lock-free command queue from the UI (`arr_play`,
  `arr_set_chord`, `arr_section`, **`arr_track_mute`**); 8 tracks (D1 D2 PC BA C1–C4) + 2 melodies;
  styles with Intro / Main A / Main B / fills / Ending. It plays *styles* steered by chords, not a
  fixed song: our song player would be new code beside it (its melody tracks hold recorded note
  events, a possible start).
- **B Felucca**: sequencer and arpeggiator with swing, chord keys, SLICER, MIDI clock (its
  BUILDING.md test list); 4 parts on MIDI channels 1–4. Not studied in depth.
- **B′ fm1-base**: `app_render` (audio, per block), `app_tick` (1 kHz), `app_frame` (~60 Hz); no
  sequencer of its own.
- **C FM1 Open**: 8-track sequencer, 30–300 BPM, MIDI clock and SPP in, per-track input channels
  (TRACK ROLE: NORMAL / PLAY / OFF).
- **foma**: `midi_clock` option: `clock_events()` gives each audio block's ticks and start/stop at
  the frame they fall on.
- **TRIO**: networks decide in the main loop; "the audio interrupt plays their decisions on time".

## Storage and getting a song onto the unit

- **A fm1-dock**: settings/presets per module; added modules share **296 KiB** of flash.
  `dock.py send FILE` puts a file into the **RAM** of the playing module (FM-1 SID plays a `.sid` at
  once): fast, but the module must store it itself to keep it.
- **B Felucca**: settings, presets, projects in flash (store at 0x97000–0xDFFFF, ~292 KiB); web
  editor and backup tools over USB.
- **B′ fm1-base / D Polly**: object store (`plat_store_*`), big data in flash beyond it
  (`plat_data*`, `PLAT_XDATA0/1`), backup/restore over USB (`fm1_backup.py`), the app's own USB
  commands (`APP_USB_CMD`, data pulled from the host), and a **resource region** the installer
  writes after the firmware (Polly's styles; `qy70_install.py` writes 128 styles over USB in ~30 s).
  App slot 581 KB flash, 96 KB RAM for .data/.bss, 336 KB pool.
- **C FM1 Open**: projects, 8 sample slots (~19.55 s), DX7 library; the workbench transfers them.
- **foma**: `settings_load` / `settings_save` only.

## A second mode

- **A fm1-dock**: built for it: our play-along would be one module among CS-1, OberVave, the sine…
  (HOME picker). The strongest story for "a non-accompaniment mode".
- **B Felucca fork**: a mode inside Felucca's UI; tight RAM.
- **B′ / D**: one app; modes are ours to write (Polly already has many pages).
- **C**: modes inside M-VAVE's app.

## Licence

- **GPL-3.0-only** (A, B, D, foma, fm1-chord, CoralFlower): our firmware becomes GPL-3.0 and its
  full source must ship with any binary we give out. Fine for an open hobby project; rules out
  closed or differently-licensed song-format code inside the firmware.
- **MPL-2.0** (B′ fm1-base, felucca-bsp, TRIO): file by file; our own files may use any licence.
  Sonivox EAS is Apache-2.0 (check Polly's modified EAS files per file before reusing them).
- **C FM1 Open**: app GPL-3.0 with a linking exception for M-VAVE's binary engine; the engine's
  grant is **FM1-devices only** (THIRD_PARTY.md); no engine source.
- Song data (arrangements of copyrighted songs) is a separate question from the firmware licence.

## Community activity (2026-11-04)

The whole ecosystem is **2–5 weeks old** (Felucca's repo: 2026-10-02).

| | Last release | Commits, 30 days | Authors, 30 days | Stars / forks |
|---|---|---|---|---|
| A fm1-dock | v0.7.1, 2026-10-31 | 134 | 2 | 18 / 3 |
| B Felucca | v1.8.2-beta, 2026-11-04 (stable 1.8.1) | 63 | 5 | 651 / 159 |
| felucca-bsp | none | 71 | 4 | 20 / 17 |
| B′ fm1-base | none (beta) | 234 | 4 | 28 / 18 |
| C FM1 Open | V14, 2026-10-19 | 8 | 3 | 302 / 53 |
| D Polly | v0.6, 2026-11-01 | 280 | 1 | 26 / 3 |
| foma | none | 21 | 1 | 5 / 0 |
| TRIO | v0.1, 2026-10-19 | 2 | 1 | 7 / 0 |

## Reuse: fm1-chord, CoralFlower, TRIO

- **fm1-chord** (GPL-3.0-only, a Felucca fork): `firmware/src/harmony.c` (333 lines),
  `chordmachine.c` (1,063): chords in a key, slash chords, inversions.
- **CoralFlower** (GPL-3.0-only, a Felucca fork): `cf_theory.c` (649 lines) and `cf_chord.c`:
  27 scales, chord + bass from one key; `cf_draw.c` for its screens.
  Both are useful as references for chord naming/voicing and display; copying code makes our
  firmware GPL.
- **TRIO** (MPL-2.0, on fm1-base): closest in spirit ("you play the chords; a walking bass and a
  drummer play along"): chord recognition from held notes (a small network), form detection,
  scheduling decisions ahead of the beat. Its big chord display (`F7`, `NEXT Bbmaj7 IN 3`) is the
  kind of screen we want.

## Recommendation (for the user and the planning chat to decide)

Leaning: **B′ fm1-base on felucca-bsp as the platform, with Polly (D) as the main reference for the
band engine.**
- fm1-base gives the platform without a sound engine to fight (MPL-2.0, so the licence stays our
  choice), the canvas and fonts for the chord/keyboard screen, flash data areas and USB commands
  for songs, the felucca-bsp loader and rescue, and a host simulator plus browser emulator for tests.
  Adding 0xB36014 is one line in the app; "refuse unknown" in the loader is a small addition.
- Polly already solves the hard parts we need next: a GM sound set (drums, bass, e-piano, lead),
  sample-accurate scheduling in the render, track mutes, MIDI clock. Taking Sonivox EAS (Apache-2.0)
  keeps us licence-free; taking Polly's arranger code makes the firmware GPL-3.0 (a decision for the
  user).
- If "fastest to something playing" matters more than licence freedom: **fork Polly** and replace
  its style player with a song player. Measured: Polly fills ≈99 % of the app slot and 97 % of RAM,
  so its style features (QY70 import, editors) would have to go to make room.
- If "a second mode beside other synths" is the priority: **fm1-dock module** (GPL, 856014-only
  platform today, RAM-only file send).

## Main risk of each

| | Main risk |
|---|---|
| A fm1-dock | Platform and loader accept only 0x856014; no shared sound engine (we'd bring one); songs sent only to RAM; GPL; two authors. |
| B Felucca fork | RAM almost full (62 of 72 KB, pool 326 of 360 KB) before our song player; a large, fast-moving codebase to keep merging; 0xB36014 support still beta; GPL. |
| B′ fm1-base | Beta, no release, `plat.h` "expected to change before 1.0"; no sound engine; app accepts only 0x856014 until we patch it. |
| C FM1 Open | Windows-only build; closed engine (FM1-only grant, no source to fix); M-VAVE's loader with unknown/negative 0xB36014 reports; slow activity. |
| D Polly | **Full**: image ≈99 % of the app slot, RAM 97 %: a song player means removing features first. One author; plays styles, not songs; GPL-3.0; EAS's CPU/voice use on the device not yet measured by us. |
| foma | Docker-only build; no sound engine; 856014-only; one author. |
