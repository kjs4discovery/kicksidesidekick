# Our firmware

**Emulator only so far. Nothing has been installed on the FM-1.** Step 0.7's checklist for the first writes:
[step-0.7-first-writes.md](step-0.7-first-writes.md) (waiting for review).

## Step 0.7: the song-playing PROTOTYPE (v0.1.0-proto, identity FM-1_4600)

Plays the built-in `songs/amazing-grace` with Sonivox EAS's MIDI file player; four layers mutable; the chord
chart from `song.yaml` on the screen. Prototype code, for Phase 1 to replace: `firmware/app/proto_*.c|h`,
`tools/proto_song_to_c.py`, `tools/proto_test.py`; `app.c` only includes `proto_ui.c`.

| Control | Does |
|---|---|
| PLAY | play from the top / stop (lit green while playing) |
| FX, SEL, ENV, LFO | mute / unmute drums, bass, harmony, melody (lit: the layer plays) |
| MASTER | volume |
| keys | Electric Piano 1 on MIDI channel 16, over the song |
| REC, OCT−, OCT+ | not used: fm1-base's rescues at power-on (below) |

Screen: title, `CPU n%  V now/most` (top left), `bar:beat` and the beats of the bar (top right; bar 0 is the
pickup), the current chord large, the next one below, the four layers (amber outline "MUTE" when muted).

How it works:
- **Song data, at build time:** `fm1.toml gen` runs `tools/proto_song_to_c.py songs/amazing-grace` →
  `build/gen/proto_song.h`: `song.mid`'s bytes as they are, which track plays which layer (variant `:1` of each;
  the other variants' tracks muted for good), and the chord chart (from `song.yaml`: pickup, sections, ending)
  as (tick, chord) pairs, consecutive repeats merged. It refuses a song with tempo or metre changes, a chart that
  does not fit the bars, or a chart that disagrees with `song.mid`'s old `chords` text track (D014 cross-check).
- **Playing from the flash:** `eas/eas_host_fw.c` implements EAS's file calls over an `EAS_FILE` locator's
  `readAt`/`size` (upstream's own interface, `eas_types.h`); the song's locator reads the array in place.
- **Muting:** upstream EAS's per-track mute (`eas_smf.c` `PARSER_DATA_MUTE_FLAGS`), compiled in by
  `JET_INTERFACE` (`eas/eas_options.h`; `jet.c` linked, never used): a muted track's note-ons are dropped, its
  note-offs and controllers still applied; on muting, an all-notes-off on the layer's channel stops held notes.
- **One synth:** the live MIDI stream (keys) is opened on the song stream's synth (`EAS_OpenMIDIStream(…, song)`),
  so the 24 voices are shared and a panic silences both.
- **Threads:** the main loop only sets requests (play/stop counter, mute mask); the audio interrupt applies them
  before each EAS render (`synth.c` calls `pp_before` / `pp_after`), and publishes the position and voices.
- **Chord timing:** the screen runs `PP_LEAD_MS` (80 ms) ahead of the parser's position, to cover a UI frame,
  a blit in progress and the big chord's LCD transfer (~0.32 ms a row); the chord is drawn and sent first.

Tests (`make proto-test`, ~10 min headless): `docs/step-0.7-emulator-report.md`.

Fallback diagnostic: `make hello` → `build/hello/fm1acc-hello.fwsc`, the Step 0.6 hello world built from commit
4629478's source (identity FM-1_7100).

## Step 0.6: first build (hello world)

![The firmware in fm1-emu, C4 held](step-0.6-emulator.png)

What it does (v0.0.1): shows "FM-1 Accompaniment", its version and whether the flash chip was accepted; each key
plays its own note on General MIDI Electric Piano 1 (Sonivox EAS) at a fixed velocity; the last key pressed is
shown large ("C4"), and a small keyboard lights the held keys.

## Layout

| Path | What | Licence |
|---|---|---|
| `firmware/app/` | our app: `app.c` (screen, keys), `synth.c` (EAS behind a queue), `eas/` (our EAS configuration, host layer, small headers), `fm1.toml` (build settings), `words.tsv` (screen reader) | TBD (D017) |
| `firmware/vendor/` | fm1-base + felucca-bsp, Sonivox EAS: see `firmware/vendor/README.md` (commits, licences, our patch) | MPL-2.0, Apache-2.0 |
| `firmware/build.sh` | runs fm1-base's `tools/build.py` on `firmware/app` with the toolchain in `third_party/jieli` | TBD |
| `tools/emu.sh`, `Makefile` | build + emulator in one step | TBD |

## Build and run

```bash
make firmware        # firmware/app/build/fm1acc.fwsc (identity FM-1_4600 from Step 0.7), ~10-20 s
make emu             # build, then play it in the emulator window
make shot            # build, 2.5 s headless with C4 held: build/emu/panel.png, build/emu/audio.raw
make patches         # once after refreshing third_party/fm1-emu: the flash-id patch, rebuilt
```

`FM1_FLASH_ID=b36014 make shot` (or any 6 hex digits) runs it on another flash chip.

Measured 2026-11-04: app image 340,236 B of the ~581 KB slot (EAS + GM set ~240 KB of it); RAM .data+.bss
66,604 of 98,304 B (EAS's pool 40 KB); pool 69,384 of 344,064 B; update loader 8,371 B. In fm1-emu: boots,
1.2–1.4× real time headless; C4 held: rms 1271, silent before, decaying after.

## Flash chip ids

| Chip (`FM1_FLASH_ID`) | App (fm1-base, patched) | Screen | Sound |
|---|---|---|---|
| 0x856014 (Puya, our unit) | flash used | FLASH OK | yes |
| 0xB36014 (UCUN UC25HQ80) | flash used | FLASH OK | yes |
| 0x123456 (unknown) | RAM only: nothing is saved, no crash | FLASH: RAM ONLY (amber) | yes |

The update loader (felucca-bsp, patched) writes only to 0x856014 and 0xB36014: with any other chip it stops with
error -14 **before erasing anything** and stays in update mode (a host sees the update fail; power-cycling returns
to the update loader while the update record is there, so install a known-good package or use the ROM rescue).
Tested in the emulator: app side (all three ids). The loader's refusal is not exercised yet (the emulator test
would need an emulated install).

## Rescue (fm1-base's, kept unchanged)

| How | What happens |
|---|---|
| **REC held at power-on** | "UPDATE MODE (REC)": the chip's own ROM update mode (USB `4c4a:8057`, "WL82 UBOOT1.00"), reachable by jl-uboot-tool / M-Flasher / felucca-bsp `tools/fm1_rescue.py`, even when the app is broken |
| **The app crashes or hangs twice while starting** (within its first 30 s) | **SAFE MODE**: no sound, no app; USB on, so the web installer, `fm1_install.py` and M-UPGRADE can reach it; PLAY tries the app again, REC goes to the ROM update mode; the last crash's PC is shown |
| **Four failed starts** (safe mode failing too) | the ROM update mode by itself |
| **USB soft key** (`F0 22 24 35 7D F7`, e.g. `fm1_unbrick.py softkey`) | the ROM update mode, from the running app, safe mode or the loader |
| **An update cut off** (power or cable lost) | the update loader runs again at the next power-on and the host resumes the install |
| **OCT− and OCT+ held at power-on** | the panel set-up (key/knob calibration), not a rescue |

The counter of failed starts is cleared by the platform after 30 s of normal running; the app needs no code for it.
Not yet tried on hardware; the emulator does not model the ROM update mode.
