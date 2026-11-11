# Felucca-Engines, vendored (M5 Part 3, D030)

From `third_party/Felucca/firmware/engines` (its own repository, MIT), commit `d0260074ce100a47675814b7f38d61f11176c926`. Only these MIT files are
copied, unchanged: `LICENSE`, `README.md`, `src/felucca_engine.h`, `src/dsp.c`, `src/eng_analog.c`,
`tools/gen_engine_tables.py`; M7 (D041, D042), from the same commit, unchanged: `src/eng_noise.c` and `src/eng_lofi.c`
(the 808 and CHIP drum kits), `src/eng_phase.c` (a synth engine). Not TRIO or WHEEL. `src/fe_tables.h` is generated from it (44.1 kHz:
`python3 tools/gen_engine_tables.py src/fe_tables.h`). Nothing else of Felucca (GPL-3.0) is used: not its host, voice
code, effects or presets. The host, the effects (chorus, delay, reverb, bit-crush;
M7: fuzz, tremolo, phaser), the arpeggiator, the drum voices' envelopes and sine parts, the presets and the drum
patterns are ours (`firmware/app/vsynth.c`, `synth.c`, `docs/drum-patterns.txt`).
