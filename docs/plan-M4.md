# Milestone M4 — instructions (one build)

From the planning chat, 2026-11-06 10:35. Scope: D026 (the user's M3 test notes, quoted in the
vault), D027 ("your part" chosen on purpose, layer volume cycle), D004 decided (hybrid songs).

```text
Follow CLAUDE.md and docs/HANDOFF.md. Build Milestone M4 in the parts below, in order.
Commit after each part. Fast checks after each part; the FULL emulator suite only on builds
offered for install (the early install point and the end). Emulator only; no hardware. For
every new or changed screen, put a mock screenshot into docs/controls.md first. If the user
is away, don't stop for questions: make reasonable choices and record each in LOG.

PART 1 — Quick fixes
- Count-in volume default: MEDIUM (not HIGH).
- Loops: bring back looping the WHOLE SONG, next to A–B (user: "keep the option").
- Help screen (GLO) on the list mentions HOME = folders; the "format 1.1" text in
  tools/m3_test.py's store check.

PART 2 — "Your part" chosen on purpose (D027)
- HOLD a layer button (~1 s) = make that layer "your part". Key lights, views, rhythm cue
  and the top line follow your part WHETHER OR NOT it is muted (user: keys should light even
  when the layer isn't muted). A song opens with harmony as your part. Replaces M3's
  "the last one muted is yours".
- PRESS a layer button = cycle its volume: FULL → QUIET → MUTED → FULL. QUIET uses the
  settings' level (the old "guide volume" becomes this; propose 25/50 % options, default
  50 %: the user asked for "half volume"). Clear on screen (layer boxes show FULL / QUIET /
  MUTE and which is YOURS).
- Key lights: setting stays (blink on/off); add "Key lights: ON / OFF".

PART 3 — Views
- Harmony notation: the simplified scrolling staff (D025) with chords as stacked noteheads
  (treble, or treble + bass staff if it fits; propose), chord names above. Follows the
  harmony level (block chords, broken chords, arpeggios all show as notes).
- Drums: only TWO views (user: the current drum screens aren't useful): the scrolling drum
  grid and a drum notation view (percussion staff: kick, snare, hi-hat/ride positions, x
  noteheads for cymbals, same simplified rules). Remove the other drum screens.
- Settings: "Views" lets the user hide views from the EDIT cycle, per layer (at least one
  view per layer stays).

PART 4 — Five harmony levels (format change)
- Format: up to 5 variants per layer (bump the format minor version; the device keeps
  reading older files; the K knobs go 1..what the song has).
- songs/make_songs.py, all 9 songs regenerated:
  - Harmony: L1 block chords held; L2 rhythmic block chords; L3 rhythmic chords with
    inversions (voice leading); L4 broken chords; L5 arpeggios. Patterns that fit each
    song's metre and style (e.g. 3/4 waltz patterns; Alberti-style broken chords for
    ballads; rolling arpeggios across ~1.5 octaves).
  - Bass up to 4: root held; rhythmic root; root–fifth; walking / more motion.
  - Drums 3: pulse; groove; groove with fills (clearly different).
- The rhythm cue, key lights, keyboard view and inversion label follow each level (L4/L5:
  the lights show each note as it's played).
- EARLY INSTALL POINT after Part 4: docs/step-M4-early-install.md (install over M3, resend
  all songs, on-unit checks for parts 1–4, fallback M3); full suite on that build; continue.

PART 5 — MIDI import tool (laptop) + the user's private test songs
- `fm1song import FILE.mid --out songs/NAME/` (Python; core logic separate from the CLI so
  the future website can reuse it, D010): list the file's tracks/channels with program,
  range, note count; map them to drums / bass / harmony / melody (auto-guess: channel 10 =
  drums, lowest pitched part = bass, highest single line = melody, the rest = harmony;
  flags to override, combine or drop tracks); simplify (quantize, merge doubled parts,
  reduce harmony to playable voicings); propose a chord chart by recognizing chords from
  the harmony + bass notes (the user reviews it); write song.mid + song.yaml.
- Hybrid (D004): the tool ALWAYS produces the full level set (the user's request):
  - Melody: L1 only, from the file (quantized, doubled notes removed; nothing added).
  - Harmony L1-L5 and bass up to L4: generated from the (reviewed) chord chart with
    make_songs.py's generators; the file's own part replaces the level it most resembles
    (auto-detected: held / rhythmic / inversions / broken / arpeggio; flag to override).
  - Drums: the file's drums = L2; L1 = simplified FROM the file's drums (kick + snare on
    the main beats, keeps the feel); L3 = the file's drums + generated fills.
  - Pattern style for generated levels picked from metre/tempo, with a --style override.
  - Report per song which levels are from the file and which are generated.
- docs/import.md for the user.
- The user's MIDI files are in import/private/ (add import/private/ and songs/private/ to
  .gitignore NOW, in Part 1). Convert each into songs/private/<name>/ using ONLY the
  content of the user's file: don't add, correct or complete melodies or parts from memory.
  song.yaml: category "Private test", rights: "private test only, remove before beta".
  Report per song what was mapped, merged, simplified and generated.
- Songs need no firmware install: the user sends them with fm1song send.

PART 6 — Tests, budget, install checklist
- Full suite on the final build (all songs incl. the private ones), flash IDs, store,
  settings. Version 0.5.0-m4. docs/step-M4-install.md: install over M3 (or the early build),
  resend songs, on-unit checks per part, fallback M3. STOP for review.
```
