# fm1-emu on this laptop

fm1-emu (joakimeriksson/fm1-emu) at 9de1ef0, v0.4.0 (2026-11-02), cloned into
`third_party/fm1-emu` (git-ignored). Emulates the FM-1's CPU (two pi32v2 cores, JIT to x86-64
here), screen, keys, encoders, pot, audio, flash, MIDI IN and USB. Hardware facts it models:
docs/device.md, "Hardware model".

## Build (done 2026-11-04)

```bash
cd third_party/fm1-emu && cargo build --release -p fm1-live -p fm1-emu
```

2 min 40 s on Linux Mint 22.3 with Rust 1.99.1. No extra system packages: the window
(minifb, X11), audio (cpal, ALSA via PipeWire) and MIDI (midir, ALSA sequencer) only need
`libasound2-dev` and `pkg-config`, both already installed. Binaries:
`third_party/fm1-emu/target/release/fm1-live` (play, or headless) and `.../fm1-emu` (batch
runner / debugger). The web version is not needed (and Node and Chromium are not installed).

## Play it

```bash
third_party/fm1-emu/target/release/fm1-live --fwsc backups/stock-firmware/FM-1_015.fwsc --midi all
```
```bash
third_party/fm1-emu/target/release/fm1-live --fwsc third_party/firmwares/felucca-1.7.0.fwsc --midi all
```

Computer keyboard: `A W S E D F T G Y H U J K O L P ; '` = C4..F5, Z/X = OCT−/OCT+,
Space = PLAY, R = REC, 1..0 = FX, SCL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ;
←/→ SELECT, ↓/↑ ALGO, `,`/`.` K1; Esc quits. The mouse plays keys (lower = harder), buttons
and knobs (MASTER = volume). `--midi all` listens to every ALSA MIDI input; to send a file:
`aplaymidi -p 14:0 FILE.mid` (Midi Through, which fm1-live subscribes to).
MIDI channels: M-VAVE stock listens on channel 2; Felucca plays its parts on channels 1–4.

`--flash FILE` keeps the emulated flash (saved presets) between runs. **Never pass our
real dump** (`backups/flash/…`) as `--flash`: it is written back at the end. Copy it first.

Felucca 1.7.0 for the emulator: `third_party/firmwares/felucca-1.7.0.fwsc` from
github.com/hugelton/Felucca/releases/tag/v1.7.0, sha256
`bff7c617cfb2c88e66b22aa691cb798dfa0d9cb12ff9a73deab51d404e82817c`, embedded identity
`FM-1_917`. (1.8.2-beta also exists, 2026-11-04.) Emulator only: never sent to the FM-1.

## Measured here

| Firmware | Speed (`--bench 5000`) | Boot screen at 5 s | C4 key | MIDI note | Silent after note-off |
|---|---|---|---|---|---|
| Stock FM-1_015 | 0.9–1.0× real time (both cores) | "001 EMU E.PNO", OSC page | rms 1434 | ch 2: rms 1106 | yes |
| Felucca 1.7.0 (FM-1_917) | 1.3× real time | track 1, tempo 120, ANALOG ACID, HOME lit | rms 862 | ch 1: rms 854 | decays (rms 100) |

Stock runs just at real time on this laptop: under load it may glitch (`--mhz` lowers the clock).

## Driving it from scripts (for automated tests)

Everything below is headless (no window, no audio device) and deterministic on the emulated
clock, so it suits CI.

**fm1-live** (fast, JIT):
- `--bench MS`: run MS emulated ms. `FM1_BENCH_WAV=FILE` writes the audio (44.1 kHz stereo
  s16le, no header), `FM1_BENCH_PANEL=FILE` the whole front panel at the end (PPM, 1280×820:
  LCD, LEDs, knobs), `FM1_BENCH_JSON=FILE` the speed, `FM1_DUMPRAM=FILE` the 1 MiB RAM.
  `FM1_BENCH_NOKEYS` turns off its default key presses.
- `--shot MS`: run, then write the panel to `work/panel.ppm` (relative to the current folder).
- `FM1_BENCH_KEYS='…'` (or `@FILE`): timed input, comma-separated `MS:EVENT`:
  `MS:C4` / `MS:PLAY` / `MS:n7` / `MS:k12` key down, `MS:C4^` key up, `MS:C4@100` with velocity,
  `MS:C4~90` pressure, `MS:e2:+3` encoder 2 three clicks, `MS:pot:512` MASTER,
  `MS:a4:700` an ADC channel, `MS:m903c64` raw MIDI bytes into the DIN jack.
- `--usb` + `--shot` + `FM1_USB_SEND='MS:HEX;…'`: MIDI over emulated USB, and prints what the
  firmware sends back (only firmwares that poll USB enumerate; stock does not).
- `--flash FILE`, `FM1_FLASH_CUT=N`: persistent flash, and a power cut in the Nth erase/program.
- `FM1_STRICT=1`: realistic timing and misaligned-access faults (slower, interpreter).

**fm1-emu** (batch runner and debugger; **slow**, an interpreter. Tried 2026-11-04:
`fm1-emu --fwsc felucca-1.7.0.fwsc --ms 3000 --keys '1500-2500:C4' --snap 3000 --wav X.wav --quiet`
ran 3 min 50 s and wrote neither file, probably stopping at the default `--steps 5000000`
before 3000 ms; not investigated. Its `--keys` syntax differs from fm1-live's: `T-T2:KEY` holds
a key, `C4^` is refused. **Use fm1-live `--bench` for tests**; keep fm1-emu for debugging):
- `--fwsc PKG | --bin APP.bin`, `--ms MS`, `--keys 'T:PLAY,T:n7,T:enc0:+3,T:pot:512,T:midi:903c64'`,
  `--wav OUT.wav`, `--snap MS,MS` (LCD only, 240×240 BMP into `work/lcd_MS.bmp`),
  `--dump ADDR N`, `--break-pc`, `--watch`, `--trace`; `fm1-gdbstub` crate for GDB.
- `--flash DECRYPTED.bin` / `--raw-flash CHIP.bin`: a whole flash image ("the flash as stored on
  the chip, app area encrypted"; the flash controller decrypts on the fly). Possibly a way to
  boot our own dump later; not tried.

Gaps for our tests:
- The emulated flash always answers JEDEC `85 60 14`. CLAUDE.md requires testing our firmware
  with 0xB36014 and an unknown ID too: needs a small patch (e.g. an env var setting
  `soc.flash.jedec`, as `set_board` does for the SMK-37).
- No machine-readable LCD-only output from fm1-live (the panel PPM includes the bezel; the LCD
  sits at a fixed position in it). `fm1-emu --snap` gives the bare 240×240 LCD.

## Live play on this laptop: choppy (measured 2026-11-04)

The laptop (Intel i7-5557U, 2 cores / 4 threads, 2015) cannot run the live window in real
time: Felucca 1.7.0 at 240 MHz ran 0.25–0.5× real time with ~80 audio underruns a second; at
`--mhz 120`, 0.5–0.8×, still underrunning. Headless the same firmware runs 1.3× (240 MHz), so
the window (1280×820 panel redraw on X11) and the other running programs cost the most; the
kernel also throttles the CPU for heat (`intel_powerclamp`, 116 times in this session's log).
Headless runs are not affected by this: they are timed on the emulated clock, so their output
is exact, just slower to produce. A faster computer (or closing other programs) is needed for
comfortable live play.
