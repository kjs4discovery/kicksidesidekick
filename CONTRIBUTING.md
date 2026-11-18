<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->
<!-- Copyright: The KickSide author -->
# Contributing to KickSide

Thank you for wanting to help. Bug reports, songs, documentation and code are all welcome.

## Before you start

- **This firmware runs on real hardware that you can damage.** Test every firmware build in the emulator first
  (`make emu`, then the test targets in the [Makefile](Makefile)). Never test unreleased builds on a unit you cannot
  restore. The project's own rules are in [CLAUDE.md](CLAUDE.md): in short, nothing is ever written to an FM-1 without
  a backup, a recorded flash-chip check, and an emulator run.
- Anything that changes how the firmware starts, stores songs, or updates itself needs special care (an old change to
  the song store once left a unit in safe mode). Say so in your pull request, and say how you tested it.
- For a big change, open an issue or a Discussion first, so we can agree on it before you spend a weekend on it.

## Reporting a bug

Use the **Bug report** form on the Issues tab. It asks for the firmware version and identity (the splash screen shows
the version; `fm1_install.py --info` or `tools/fm1song.sh list` report the identity and only read), the flash chip
line on the Settings screen, the mode, the song, and what happened. Please do not post anything private.

## Sending songs

- Songs must be **public domain, or your own work**, and you must say which in the song's `song.yaml`.
- **Do not send copyrighted songs** (modern pop, rock, film and game music, and arrangements of them), even as
  "just for testing". They cannot be added to this repository.
- The format is in [docs/song-format.md](docs/song-format.md). `make song-test` checks the compiler.
- Songs and documentation are shared under **CC BY-SA 4.0**.

## Sending code

1. Fork the repository and make a branch.
2. Keep the change small and focused. Match the style of the file you are in.
3. Add the licence line to any new file: `SPDX-License-Identifier: GPL-3.0-or-later` for code,
   `CC-BY-SA-4.0` for songs and documentation. Do **not** change the licence headers of files in `firmware/vendor/`.
4. Run the tests that apply (`make song-test`, `make store-test`, the emulator tests) and say which ones you ran.
5. Open a pull request.

## Your contribution, and the licence

Contributions come under the project's licences: GPL-3.0-or-later for code, CC BY-SA 4.0 for songs and documentation.
We use the **Developer Certificate of Origin** (https://developercertificate.org/): by adding a `Signed-off-by:` line
to each commit you state that you wrote the change or have the right to submit it under those licences. Git adds the
line for you with `git commit -s`.

## Contact

Website: https://kicksidesidekick.com. Email: websiteInfo@KickSideSideKick.com. By taking part you agree to the
[code of conduct](CODE_OF_CONDUCT.md).
