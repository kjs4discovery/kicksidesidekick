# Milestone M6 — instructions (after M5.1 is installed and the songs are on the unit)

From the planning chat, 2026-11-09. Scope: D031 (the user's M5 notes, quoted in the vault).

```text
Follow CLAUDE.md and docs/HANDOFF.md. Build Milestone M6 in the parts below, in order. Commit after
each part. Fast checks after each part; the full suite (via the test-runner subagent) only on builds
offered for install. Emulator with the M5.1 flash timing. Mock screenshots in docs/controls.md for new
screens. If the user is away: decide, record in LOG briefly, carry on.

PART 1 — Learning-mode fixes
- Back to the mode screen: HOLD HOME ~2 s from any screen (learning or synth) and a "Switch mode"
  item in settings. Rescue combos unaffected.
- Harmony L3 still much louder on Let It Be and True Colors (the user): check whether imported parts
  skip the M5 loudness normalisation; normalise every level of every layer, imported or generated;
  report before/after per song.
- "QUIET" text unreadable on its box: raise the contrast (check every box state/colour pair).
- Melody roll: replace the row labels on the left with a vertical piano keyboard (the rows are
  pitches), like the drum grid's labels are for drums.

PART 2 — The views postponed from M5 Part 4
- Ukulele option (setting, off by default): ukulele chord boxes (GCEA, our own table, script-checked).
- Melody: guitar tab and ukulele tab views (playable positions, as the bass tab).
- Harmony: the horizontal roll ("bar view") as a view option.
- Zoom in/out with SELECT in scrolling and staff views, sensible min/max.
- Chord boxes (guitar, ukulele): the blinking rhythm icon under the open-string row.
- EARLY INSTALL POINT after Part 2.

PART 3 — Synth mode, round 2
- Reverb and a bit-crush effect (our own code or Sonivox EAS's Apache reverb; nothing from Felucca's
  GPL files).
- Arpeggiator: rate setting (e.g. 1/4, 1/8, 1/8T, 1/16) and a RANDOM pattern.
- A SHIFT layer for K1-K4 (second set of parameters; shown on screen while held), e.g. reverb,
  bit-crush, arp rate, detune/PWM.
- Drum jam: pick a drum pattern from the learning engine's styles (and their levels), set the tempo,
  play it, and play the synth over it.

PART 4 — Tests, budget, install checklist (install over M5.1; no song resend unless the format
changed). STOP for review.
```

Not in M6 (backlog): synth LFO; saving presets (user/default folders; a new write path: own review);
song info page; an "ALBUM" level (the imported part labelled as album-accurate).
