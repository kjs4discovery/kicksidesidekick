# Milestone M5 — instructions (one build, in priority order)

From the planning chat, 2026-11-06 14:15. Scope: D028 (the user's M4 notes, quoted in the vault),
D029 (storage: generate levels on the FM-1, A+B+C; supersedes D005), D030 (a SYNTH mode chosen at
power-on). The user's weekly usage is limited: the parts are in PRIORITY order, with two early
install points, so that whatever is finished first is the most valuable.

```text
Follow CLAUDE.md and docs/HANDOFF.md. Build Milestone M5 in the parts below, in order.
Commit after each part. Fast checks after each part; the FULL suite only on builds offered for
install (both early install points and the end). Emulator only; no hardware. Mock screenshots in
docs/controls.md for new screens. If the user is away, don't stop for questions: decide, record
in LOG with the reason. If the user says "finish the current part, then stop", do that and write
the install checklist for the last completed part.

PART 1 — Fixes from the M4 test (small, high value)
- Volume: every level of a layer must sound equally loud (user: harmony L3 much louder on Let It
  Be and True Colors; bass L4 louder on Let It Be). Measure RMS per level for every song and
  layer (emulator WAVs); normalise (velocity scaling per level, at compile time or in the player).
  Report before/after per song.
- Let It Be harmony L3 sounds the same as L1 (L2 is busier): find out why (import-level
  detection?) and fix.
- Harmony L5 in the horizontal roll / keyboard: some arpeggio notes are not shown in the darker
  "playing" colour on the keys. Fix: every sounding note lights.
- Bass tab: (1) check the string labels against standard tab (top line = G, the highest string;
  bottom = E) — the user believes they're upside down; (2) fingering: choose positions a bass
  player would use: stay within a small hand position, prefer the next string over long shifts
  on one string (user's example, True Colors: 2 on E then 2 on A, not 2 → 7 on E).
- Bass register (user + planning chat, 2026-11-06): generated bass roots are E2..D#3
  (tools/fm1song/arrange.py _bass_root), an octave above where a bass player plays them, so the
  tab shows D/G strings. Write the bass in its REAL octave (roots E1..D#2; imported basses are
  already real, keep them). Add a setting "Bass sound: REAL / +1 OCTAVE (speaker)", default
  +1 OCTAVE (the small speaker can't reproduce ~41 Hz; the user can pick REAL on headphones).
  The tab always shows the real notes (E and A strings). The bass-clef notation shows them an
  octave higher than they sound, as bass guitar music is written. Check the keyboard view and
  key lights (fold as before) and the golden tests (Part 2) with the new register.
- Main screen: the PLAY / STOP / PAUSE label sometimes overlaps the second row: fix the layout.
- Track boxes: a space between the level and the volume ("L1 FULL"), and different colours for
  the two.
- Opening a song always starts fresh: default view, the song's key and tempo (nothing kept from
  the last visit; don't save these).
- Splash: one second longer (≥4 s), and a short start-up melody of 4–6 notes (an ORIGINAL motif;
  e.g. a rising major-7th arpeggio on a synth/e-piano sound with a touch of reverb), not louder
  than the count-in at MEDIUM.
- Controls hint: before a song's play screen, a pop-up "PRESS = volume (FULL / QUIET / MUTE) ·
  HOLD = make it YOUR PART"; any button closes it; shown at most 10 times, then off by itself; a
  setting turns it on/off. Also a short form in the legend.

PART 2 — Storage: generate levels on the FM-1 (D029: options A + B + C)
- C: the FM-1 generates harmony L1–L5, bass L1–L4, drums L1–L3 from the chord chart, metre and a
  style, porting tools/fm1song/arrange.py to C (the same rules; golden tests: the C and Python
  generators must produce identical events for every song and level). A song file then holds
  the chart, sections, melody, style and only the parts/levels that were made by hand or
  imported (they override the generated level: D004 hybrid).
- A: play only the chosen level of each layer, generated or read from flash as it plays (no
  whole-song decode into RAM); receive songs straight into flash (no 64 KiB RAM limit).
- B: a compact format 2.0: one event per note (with its length), repeated bars stored once
  (drum grooves above all), several songs per flash sector (lift the 69-song cap). Keep reading
  1.x files.
- Re-import the five private songs WHOLE (no cutting) and regenerate our songs in 2.0; report
  sizes and how many songs fit. Put back Greensleeves (and every song: user wants them all).
- Store tests (power cuts) and fuzzing as before, for the new format and store layout.
- EARLY INSTALL POINT 1 after Part 2 (full suite).

PART 3 — Mode select + SYNTH mode (D030)
- At every power-on, after the splash: a mode screen LEARN / SYNTH with the mascot image (the
  same as the splash), cursor on LEARN; SELECT + PLAY (or similar) to choose; the power-on
  rescue (REC, OCT-, OCT+ held at start) is unaffected.
- SYNTH mode, synthwave leaning: Felucca-Engines (MIT, third_party/Felucca/firmware/engines ONLY;
  vendor it in its own folder with its licence; the rest of Felucca is GPL: copy nothing from it,
  incl. its effects). ANALOG + PHASE engines; 8-voice polyphony; our own chorus (Juno-style) and
  delay; reverb from Sonivox EAS (Apache) or our own; an arpeggiator (up, down, up-down, random;
  rate synced to a tempo; latch); OCT buttons; knobs for cutoff, resonance, attack, release,
  chorus, delay; SELECT picks presets.
- At least 20 of OUR OWN synthwave presets (lush pads, PWM strings, octave-bouncing basses, saw
  and sync leads, brass stabs, plucks, bells (PHASE), a sweep/riser, a chip voice…). Names ours.
- Learning and synth mode never run at once: share RAM where possible. Budget report.
- EARLY INSTALL POINT 2 after Part 3 (full suite).

PART 4 — Views
- Ukulele option (setting, OFF by default): ukulele chord boxes instead of / next to guitar
  (GCEA, our own shape table, checked by script like the guitar table).
- Melody: guitar tab and ukulele tab views (same scrolling engine as the bass tab, same
  "playable positions" rule).
- Harmony: the horizontal roll ("bar view") as a view option (chords as stacked bars).
- Zoom in and out in the scrolling and staff views (maybe all views): SELECT zooms in that
  context; sensible min/max for 240x240. (Replaces the 1.5/3 bars setting.)
- Chord boxes (guitar, ukulele): the blinking rhythm note icon, under the open-string row, blinking
  like the others.

PART 5 — Tests, budget, install checklist
- Full suite; version 0.6.0-m5; docs/step-M5-install.md (install over M4 or an early build; resend
  all songs in 2.0; on-unit checks per part; the mode screen; fallback M4: note that M4 can't read
  2.0 songs — how the user gets back). STOP for review.
```
