<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->
<!-- Copyright: The KickSide author -->
# KickSide - Your Musical SideKick

Play-along firmware for the **M-VAVE FM-1** synthesizer. The band plays drums, bass, harmony and melody; you mute one
layer and the screen shows what to play. The first use is "mute the harmony, read the chords" for piano players.
It also shows the parts as ukulele and bass tab, and it has a SYNTH mode with presets.

> **Beta software (0.x). Installing any custom firmware is at your own risk.** KickSide replaces the firmware on your
> FM-1. It has been developed and tested on one unit, and a bad install can leave an FM-1 not starting until you
> restore it (see "Going back to the official firmware" below). Read this README before you install. This project is
> not affiliated with M-VAVE.

## What it does

- **SONG LEARN**: pick a song, then play along. The band plays; you mute or quiet one layer (harmony, bass, drums or
  melody) and the screen shows that part as chords, keys, ukulele or tab. Each part has levels from simple to
  detailed.
- **BACKTRACK**: jam over a band; chord loops in many styles to play or sing over.
- **SYNTH**: play the FM-1 as a synthesizer, with presets, an arpeggiator, chorus and delay.
- **Built-in starter library**: 15 songs and 42 backtracks are inside the firmware, so a new unit has something to play
  at once. Songs are public-domain tunes with the project's own arrangements.
- **Your own songs**: made on a laptop (from a MIDI file or by hand) and sent over USB with `tools/fm1song.sh`.

Every control is described in [docs/reference.md](docs/reference.md).

## Supported hardware

- The M-VAVE FM-1. The firmware accepts both flash chips found in these units: **0x856014 (Puya P25Q80)** and
  **0xB36014 (UCUN UC25HQ80, units sold since autumn 2026)**, in the app and in the update loader. An unknown chip is
  refused without writing.
- **Tested on real hardware:** one unit with the Puya chip that came with the official V15 firmware. The UCUN chip and
  units that shipped with or were updated to the official **V16** are covered by the same code and by the emulator,
  but have **not** been tried on real hardware. Do not update an FM-1 to V16 before installing KickSide.

## Installing

1. **Back up first.** Installing replaces the running firmware. Songs you sent and presets you saved live in the
   unit's flash, and another firmware may read them differently or overwrite them. See the release notes for what
   this version does to existing data.
2. **Charge the FM-1** and use a good USB cable, plugged straight into the computer.
3. Get a `.fwsc` package from the [Releases](../../releases) page.
4. Install it with either:
   - **The Firmware Hub** (https://thefirmwarehub.com/), in Chrome or Edge, once KickSide is listed there; or
   - the installer in this repository, on Linux: `python3 firmware/vendor/fm1-base/bsp/tools/fm1_install.py PACKAGE.fwsc`
     (`--info` on its own only asks the FM-1 which firmware is running and writes nothing).
5. After the restart the splash shows the KickSide logo and the version at the lower right.

## Going back to the official firmware

- On the Firmware Hub, press **Back to official V15**, or download V15 from M-VAVE and select that file in the Hub.
- If the FM-1 will not start: hold **OCT-** while switching it on. It then waits for a USB update, and you can install
  V15 or another firmware. The Hub also has a rescue page (https://thefirmwarehub.com/tools/rescue/).
- If the FM-1 fails to start twice, it comes up in a safe mode that the installer can reach.

## Your own songs

Songs are made on the laptop, never on the device. Each song is a folder with `song.mid` and `song.yaml`
([docs/song-format.md](docs/song-format.md)). To turn an ordinary MIDI file into a full set of levels, see
[docs/import.md](docs/import.md). The command line is `tools/fm1song.sh`: `compile`, `import`, `list`, `send`,
`delete` and `wipe`. **`send`, `delete` and `wipe` write to the FM-1's song area**; `list` only asks. The tools need
Python 3 and these packages: `PyYAML` (song files), `numpy` and `lameenc` (the preview audio), and `mido` with
`python-rtmidi` (talking to the FM-1 over USB-MIDI). `pytest` runs the tests.

The songs in this repository are public-domain tunes or the project's own work, and each `song.yaml` says which.
**Please do not add copyrighted songs** (see [CONTRIBUTING.md](CONTRIBUTING.md)).

## Building from source

- Python 3 in a virtual environment (`.venv/`) with `Pillow` and `fontTools` for the build, plus the packages listed
  under "Your own songs" for the song tools and tests. (A pinned `requirements.txt` is still to come.)
- The **JieLi toolchain and AC79 SDK are not in this repository.** Put them in `third_party/jieli/`:
  the Linux toolchain from https://pkgman.jieliapp.com/s/linux-toolchain (release `jieli-linux-toolchains-20260730.1`) in
  `third_party/jieli/toolchain`, and the SDK from https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK (`AC79NN_SDK_V1.2.1`) in
  `third_party/jieli/fw-AC79_AIoT_SDK`. The build uses three SDK files (`uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin`).
- `firmware/build.sh` builds `firmware/app/build/fm1acc.fwsc`. It only builds; it installs nothing.
- Test in the emulator (https://github.com/joakimeriksson/fm1-emu, in `third_party/`) before you go near hardware:
  `make emu`, `make song-test`, `make store-test`. See the [Makefile](Makefile).
- A release is built from a clean, tagged commit; the release notes name the commit.

## Licences

- **Code** (`firmware/app/`, `tools/`, the scripts in `songs/`): GPL-3.0-or-later ([LICENSE](LICENSE)).
- **Songs, backtracks and the documentation**: CC BY-SA 4.0 ([LICENSES/CC-BY-SA-4.0.txt](LICENSES/CC-BY-SA-4.0.txt)).
- **The name KickSide, the logo and the mascot**: not licensed, all rights reserved by the author
  ([TRADEMARKS.md](TRADEMARKS.md)).
- **Third-party code** in `firmware/vendor/` (fm1-base and felucca-bsp, MPL-2.0; Sonivox EAS and its General MIDI sound
  set, Apache-2.0; Felucca-Engines, MIT; fonts under the SIL OFL) keeps its own licences. [NOTICE](NOTICE) lists them all.

**A commercial version.** KickSide is free software under the GNU GPL, version 3 or later: you may use it, change it
and even sell it, as long as you pass on the source of your version under the same licence. If you would like to make
a commercial version on other terms (for example closed source, or built into a product), please get in touch:
websiteInfo@KickSideSideKick.com.

## About the code

Much of the code was written with an AI coding assistant (Claude Code) under my direction. I designed, reviewed and
tested it. The planning, decisions and test records are in [docs/](docs/).

## Contact and links

- Website: https://kicksidesidekick.com
- Email: websiteInfo@KickSideSideKick.com
- Questions and ideas: GitHub Discussions. Bugs and song requests: GitHub Issues (there are forms).
- Contributing: [CONTRIBUTING.md](CONTRIBUTING.md). Code of conduct: [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).
