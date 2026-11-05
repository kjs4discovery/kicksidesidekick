# Our firmware

**On the FM-1 now: M1, v0.2.0-m1 (FM-1_46000200), installed by the user 2026-11-05 (docs/device.md).**
M2 (below) is being built in parts, emulator only; each part's tested release build is kept in `build/step-M2/`.

## Milestone M2 (v0.3.0-m2; part builds 0.2.1-m2p1 = FM-1_46000201, ...)

Plan: docs/plan-M2.md (D020, D023). Controls: [controls.md](controls.md).

### Part 1: songs outside the firmware (items 11, 18)

| File | What |
|---|---|
| `firmware/app/store.c/.h` | the song store in the flash data area, 0x97000–0xDBFFF: records written head last, older copies erased after, every erase / write checked against the area (docs/song-format.md 4). Standalone C: `make store-test` runs it on a simulated flash with a power cut at every operation |
| `firmware/app/library.c` | the store on fm1-base's `plat_data*`; the song list (by title); opening a song (the full check, its events decoded into a 64 KiB RAM buffer); the USB commands LIST / SEND / DELETE (fm1-base `APP_USB_CMD`, the update's framing and read requests) |
| `firmware/app/song.c` | reads format 1.1 too: `EZ` chunks (delta-coded events) decoded at open |
| `tools/fm1song/device.py`, `fm1song send / list / delete` | the laptop side; talks only to our firmware (identity `FM-1_46…`) |
| `tools/patches/fm1-emu-usb-host.patch` | fm1-live `FM1_USB_HOST='CMD'`: a program plays the USB host in `--bench` runs (the tests' `fm1song send --emu-pipe`) |
| `tools/m2_test.py` (`make m2-test`), `tools/m2_host.py` | the emulator tests: the store, every song, flash ids |

- **No songs in the image**: 376 KB (M1: 484 KB with 113 KB of songs). The five songs are 45 KB in format 1.1.
- **RAM**: .data+.bss 73.6 of 96 KB; pool 215 of 336 KB (the receive buffer and the decode buffer, 64 KiB each, and
  the list's 64 song headers).
- **Boot never reads a song**: the store is scanned when the list is first drawn; a song is checked in full when
  opened. Empty store: the list says how to send songs. The platform refused the flash chip: `NO SONG STORAGE`.
- **A send** (`fm1song send`): the laptop compiles and checks each song first; the FM-1 stops playing, goes to the
  list, pulls the whole file into RAM (512-byte reads), checks its CRC and runs `song_open` on it, and only then
  writes it (store.c). Sending a song that is there already replaces it (same slug).

## Milestone M1: song list, player, play screen (v0.2.0-m1, identity FM-1_46000200)

Plays the five starter songs (`songs/`, format `fm1-song/1`, docs/song-format.md) with our own scheduler.
Controls: [controls.md](controls.md).

| File | What |
|---|---|
| `firmware/app/song.c/.h` | the compiled-song parser: standalone C, every bound checked at `song_open`, accessors that read in place. Fuzzed on the laptop under ASan/UBSan (`make fuzz`, tools/song_fuzz.c); `song_fuzz --diff` checks it accepts exactly what tools/fm1song's reader accepts |
| `firmware/app/player.c/.h` | the player (D017: our code): per-block scheduler, mutes, variants, tempo, count-in, loops; stats in RAM for the tests |
| `firmware/app/ui.c` | song list and play screen (simple; step 1.6 designs the real ones) |
| `firmware/app/synth.c` | EAS behind a queue; one MIDI stream for the song and the keys; calls `player_block` before each render |
| `tools/songs_to_c.py` | build step (`fm1.toml gen`): compiles every song folder and writes `build/gen/songs_gen.h` (removed in M2; in git at 0bc7c88) |
| `tools/m1_test.py` | headless emulator tests (`make m1-test`): [step-M1-emulator-report.md](step-M1-emulator-report.md) (became tools/m2_test.py) |

How it works:
- **Songs in the build.** The compiled `.fm1s` files are `const` arrays in the app image (the flash, read in
  place through the cache, as Step 0.7's MIDI file was). Chosen over the resource region or the data area because
  it needs no new flash layout and no write path, and the package's own checks cover the songs; the cost is a
  rebuild to change songs, which `fm1song send` (step 1.7) removes. 113 KB for the five songs; the image is
  484 KB of the app slot's ~581 KB. The list peeks at each song's header and META when it is drawn; `song_open`
  (CRC and every check) runs when a song is opened, not at boot. A song that fails shows `SONG ERROR <reason>`.
- **Timing.** `player_block` runs in the audio interrupt before each EAS render (256 frames, 5.8 ms) and sends
  every event whose tick falls before the block's end, at the block's start (≤ one block early, as EAS's own file
  player). The position is a 64-bit count in units of 1/(tempo_us × 441) tick, advanced by 256 × ppq × tempo% ×
  100 per block: exact at 44.1 kHz, so tempo changes (50–120 %) never drift. All 64-bit multiplies are in a
  `noinline` helper (felucca-bsp docs/CPU.md: inlined beside loads they can corrupt a register on this CPU; the
  build's check confirms none is inlined).
- **Mutes, variants, loops, stop.** Each layer's sounding notes are tracked (128-bit set); a mute, a variant
  change, a loop jump or a stop sends exactly their note-offs (and sustain off). A muted layer's note-ons are
  skipped, its controllers still sent; unmuting resumes at the next note.
- **Count-in** (ARP, default on): wood-block clicks for one bar plus the pickup bar's empty beats.
- **Loops** (SEQ: off / song / section): docs/song-format.md 3.3; the screen maps positions past the loop end
  back to its start, so the chord, the countdown and the bar strip show what will be heard.
- **Chord timing.** The big chord is drawn `PL_LEAD_CHORD_MS` (100 ms) ahead of the sound, first in the frame,
  and the frame waits for its DMA to finish before logging it; the beat cursor and countdown use 30 ms. The
  tests check every chord change was on the LCD before the block that played it.

Fallback diagnostic, unchanged: `make hello` (the Step 0.6 hello world).

## Step 0.7: the song-playing PROTOTYPE (v0.1.0-proto, identity FM-1_4600) — history

Replaced in M1 (code removed; in git at 98fc08c). Kept here as the record of what is installed now.

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

Tests (then `make proto-test`, removed in M1): `docs/step-0.7-emulator-report.md`.

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
| `firmware/app/` | our app: `app.c` (includes `ui.c`: screens, controls), `song.c` (parser), `player.c` (scheduler), `synth.c` (EAS behind a queue), `eas/` (our EAS configuration, host layer, small headers), `fm1.toml` (build settings), `words.tsv` (screen reader) | TBD (D017) |
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
