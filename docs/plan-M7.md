# Milestone M7 — instructions (after M6 is installed)

From the planning chat, 2026-11-10. Scope: vault decisions D039–D044 (the user's M6 notes, quoted in the vault's
Brainstorms/2026-11-04 Prototype feedback). M7.1 (saving user presets) is a SEPARATE milestone after M7 (D040):
do not write any preset-saving code in M7 beyond the RAM record format.

State at the start: the user installed 0.6.0-m6 (FM-1_46000600), ran docs/step-M6-install.md steps 1-6, all fine,
and ran the optional step 5 (the 5 private songs resent with the 4-bar loudness matching). Record that in LOG and
device.md first.

```text
Follow CLAUDE.md and docs/HANDOFF.md. Build Milestone M7 in the parts below, in PRIORITY order. Commit after each
part. Fast checks after each part; the full suite plus make m51-test (via the test-runner subagent) only ONCE, on
the final package in Part 6. No early install point. Emulator with the M5.1 flash timing. Mock screenshots in
docs/controls.md ("M7" section) for every new screen. If the user is away: decide, record in LOG briefly, carry on.
If the budget in Part 6 is not met, use the drop order there; never relax a check to pass.

PART 0 — Measure first (no behaviour change)
- Record in LOG: the compiled size of vsynth.c, synthui.c, synth.c, jam_songs.h, the EAS host, and the free app
  space (slot 0x4000..0x92FFF minus the image); the SYNTH CPU worst case as in step-M6-install.md.
- Confirm in code and with the M5.1 flash dump notes that nothing uses 0xDE000..0xDFFFF (for M7.1, D040); write
  the finding in LOG. Do not use the area in M7.

PART 1 — Learning mode (D043, D039)
- Remove "Rockin' in the Free World": git rm songs/private/rockin and its line in songs/private/import_all.sh.
  Keep import/private/Rockin_in_the_Free_World.mid (not committed anyway). Drop its rows from the loudness checks.
  The unit: the user runs `tools/fm1song.sh delete rockin-free-world` (writes the song area: marks the record
  deleted, erases nothing). Put it in the install checklist; never run it.
- Zombie melody too loud: the MIDI has NO vocal track (planning chat checked: 6 instrument tracks, no lyrics; the
  "melody" is the overdrive guitar lead). Do not reprocess. Set layers.melody.volume in songs/private/zombie/
  song.yaml so the melody sits with the others (measure melody vs. harmony+bass in the emulator, as the M6
  loudness runs; aim at the public songs' typical ratio; report before/after).
- Per-song start level (format 2.1, additive): song.yaml layers.<layer>.start_level: 1..N (default 1). Stored in
  LAYR's flags byte (r[5], must be 0 today). The compiler writes 2.0 when no layer has a start level, so every
  other song is byte-identical and the golden tests do not change; Zombie gets bass start_level: 2. Firmware: a
  song opens with that level (fresh start per song as M5). Update docs/song-format.md, the parser, the fuzz
  (song_fuzz.sh) and the Python reader. The L1 generation rules do NOT change. Zombie must be resent after the
  install (M6 firmware refuses 2.1: send only after installing M7; checklist).
- Zombie drums L1 1.68x (step-M6-install "Known"): accept for now (backlog); no gen.c change in M7.
- "Not implemented" pop-up in LEARN mode too (D044): census every learning screen; any button with no job there
  shows a small pop-up "NOT IMPLEMENTED" that any button closes. The closing press does nothing else. Never for a
  button held since power-on; never on the WIPE question, the mode screen, the splash, the hint pop-up. HOME held
  2 s always works (also with the pop-up open). Keys keep playing; encoders never raise it. List each screen's
  free buttons in controls.md.

PART 2 — SYNTH UI rewrite on the SAME engine (D039; no engine rewrite)
- One parameter table (name, range, page, default, how it is shown) drives: the edit pages, the presets, the
  preset record and (in M7.1) saving. synthui.c becomes table-driven; aim: the synth UI code not bigger than today.
- Preset record (RAM, for M7.1): u8 version, u8 engine, name (12 chars), the parameter values (u8 each), padding
  to a fixed size <= 64 B; a load of an older version fills new parameters with defaults. Stock presets are this
  record in a const table; the sound plays from a RAM copy (M7.1 saves from RAM only: the M5 lesson).
- Main screen: the preset name (big), the drum pattern, the tempo (small), and the bottom instruction row as the
  learning app. Nothing else (the other settings live on the edit pages).
- Controls (the user approved this map):
  | PRESET knob | the preset 1..30 |
  | SELECT (main screen) | the tempo 60..200 BPM (arp, delay, drums follow) |
  | K1..K4 (main screen) | quick: cutoff, resonance, reverb mix, delay mix (the bottom row shows the value while turned) |
  | EDIT | the next edit page; after the last, the main screen |
  | ALGO | turn: the edit pages forward / back (from the main screen: the first / last page) |
  | K1..K4 (edit page) | that page's four settings |
  | HOME tap | the main screen |  | HOME held 2 s | the mode screen (as M6; use held_ms(), the M6 fix) |
  | SEQ | the drum pattern menu (SELECT scrolls, SEQ/PLAY chooses, HOME back) |
  | PLAY | drums on / off |
  | GLO | help (the controls, as the learning app's GLO) and from there settings: drum volume, drum kit |
  | ARP | the arpeggiator on / off (pattern and rate: on the ARP page) |
  | ENV / FX | jump to the ENVELOPE page / the first FX page |
  | LFO, SCL, SAVE, REC | the "not implemented" pop-up (SAVE: M7.1) |
  | OCT- / OCT+ | the octave, as now |
  The EDIT-held SHIFT layer goes away. Power-on rescues (OCT-+OCT+, REC: fm1-base; GLO+HOME: ours) unchanged.
- Pages, in order; an effect's settings never split across pages:
  ENVELOPE (attack, decay, sustain, release) - FILTER (cutoff, resonance, env amount, type LP/BP/HP) -
  SOUND A and SOUND B (the engine's own 8 parameters from its descriptors, 4 a page; names from the engine) -
  ARP (pattern UP/DOWN/UP-DOWN/RANDOM, rate 1/4 1/8 1/8T 1/16, octaves 1-3, gate) -
  FX 1 (chorus depth, chorus mix | tremolo rate, tremolo depth) - FX 2 (phaser rate, depth, mix | crush) -
  FX 3 (delay time in note values, feedback, mix | fuzz) - FX 4 (reverb size, damping, mix | preset level).
  Each page: title, its number ("3/9"), four labelled values in the K1..K4 positions, the instruction row.
- Synth settings (drum volume, drum kit) are saved in OBJ_SETTINGS, the existing settings write path (settings
  record grows as in M6: new fields read defaults). Defaults: kit ACOUSTIC, pattern ROCK1 at every power-on.

PART 3 — Effects (D039)
- New: tremolo, phaser (4-6 all-pass stages, no buffer), fuzz (a waveshaper with a tone filter). Kept: chorus,
  delay, reverb, bit-crush. Fixed chain: voices -> fuzz -> crush -> tremolo -> phaser -> chorus -> delay -> reverb.
  Our own code only; nothing from Felucca's GPL files (fx.c), nothing from EAS's reverb.
- Each effect at 0 costs no CPU (skipped). vsynth_test: each effect alone and all together, silence after release.

PART 4 — Drums (D041)
- Take the jam OFF the learning engine: remove jam_songs.h / tools/jam_songs.py and the song player in SYNTH mode.
  Our own step patterns: tools/jam_patterns.py reads a readable text grid (docs/drum-patterns.txt: one row per
  instrument, "x" accent, "o" normal, "." rest) and writes firmware/app/jam_patterns.h. Per pattern: name, beats
  per bar, steps per bar (16, or 12 for triplet feels), 1-2 bars, per step 8 instruments x 2-bit velocity
  (kick, snare, closed hat, open hat, clap, tom low, tom high, rim/perc). About 64 B a pattern.
- At least 24 patterns: POP1 POP2 POP3 ROCK1 ROCK2 ROCK3 FUNK1 FUNK2 DISCO HOUSE TECHNO SYNTHWAVE1 SYNTHWAVE2
  HIPHOP1 HIPHOP2 REGGAE SALSA BOSSA SHUFFLE BLUES(12/8) WALTZ(3/4) BALLAD 6/8 TRAIN MARCH. ROCK1 default. Write
  them from common genre conventions; the user will listen and prune (list them in controls.md as grids).
- A small sequencer in synth.c (tempo shared, steps on the control tick, swing none) sends each hit to the kit:
  ACOUSTIC = the EAS GM kit (notes on channel 10 through the EAS API directly; the only kit the GM bank has: 1
  drum program), 808 = synthesized (Felucca NOISE engine for snare/hats/clap + a sine-sweep kick of our own),
  CHIP = Felucca LOFI. Up to 6 drum voices, their own, never stealing synth voices. Drum volume from settings.
- PLAY on/off as M6; a pattern change while playing takes effect on the next bar.

PART 5 — Engines and the 30 presets (D042)
- Vendor Felucca-Engines PHASE, NOISE, LOFI (MIT, firmware/vendor/felucca-engines, unchanged, VENDORED.md
  updated). Not TRIO/WHEEL. Not msfa/Dexed (a later milestone).
- A preset chooses its engine (ANALOG or PHASE for the synth voice). SOUND A/B pages show that engine's parameters.
- 30 stock presets: today's 21 minus DEEP SUB and ANALOG BELL (19), plus E-PIANO (PHASE: CZ-style keys, tremolo),
  E-GUITAR (ANALOG pluck into fuzz), E-SITAR (PHASE resonant wave, bright, short decay, a touch of chorus), POTATO
  (a warm, round, slightly lumpy low lead), PARKER (a breathy sax-like lead: PWM, filter env, a little noise), EDS
  (a gritty bit-crushed sequence with its own arp), and 5 more of yours (synthwave-leaning; name them in LOG).
  The user may rename or re-voice POTATO / PARKER / EDS after hearing them.
- Each preset uses a sensible subset of the effects; presets are level-matched (preset level parameter).

PART 6 — Tests, budget, install checklist. STOP for review.
- Full suite + make m51-test once on the final package (test-runner subagent). New: a BUTTON CENSUS test (both
  modes, every screen, every button: its job or the pop-up, never nothing, never a crash); the power-on rescues
  with M7 (OCT-+OCT+ and REC reach fm1-base's paths, GLO+HOME the WIPE question, with no pop-up); 24 quick HOME
  taps never open the mode screen (M6's check) in the new SYNTH UI too.
- Budget table as step-M6-install.md, plus: CPU worst case = 8 voices of the heaviest preset, every effect at max,
  the busiest pattern on each kit. LIMIT: <= 60 % of a block. Over it, drop in this order and report: (1) the
  phaser, (2) 8 -> 6 synth voices, (3) the CHIP kit. App image must leave >= 32 KB free in the slot (M7.1, later).
- docs/step-M7-install.md: install over M6; then the user deletes Rockin' and resends Zombie (2.1) only after the
  install; other songs untouched. Known items. STOP.
```

Not in M7: saving user presets (M7.1, D040: 30 user slots in their own fm1-base store object at 0xDE000, built in
RAM, M5.1 rules, its own review and install); recording; synth LFO; msfa/DX7; song info page; ALBUM level.
