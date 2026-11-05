# Milestone M2 — instructions (one build)

From the planning chat, 2026-11-05 10:35. Replaces the earlier M2a/M2b version (the user
rejected the split, D022). Scope: D020 (the user's list, LOG 2026-11-05 09:53) plus D023
(beat light, three more songs, bass note names, key lights). Item numbers = the user's list;
letters = D023.

```text
Follow CLAUDE.md and docs/HANDOFF.md. Build Milestone M2 in the parts below, in order.
Commit after each part and keep a tested release build of each part in build/step-M2/
(so a later part's problem can fall back to an earlier part's build). Emulator only; at
the end write docs/step-M2-install.md and STOP for review. If the user is away, don't
stop for questions: make reasonable choices, record each in LOG with the reason.

GENERAL — discoverability (lesson from M1: the user couldn't find section looping):
every screen shows a short legend of what the controls do (Skiff-style), and
docs/controls.md is kept up to date. For new screens (loops, keyboard view, rhythm cue),
put the design with a mock screenshot into docs/controls.md first, then build it.

PART 1 — Songs outside the firmware (items 11, 18)
- Songs live in the flash data area, sent from the laptop: `fm1song send / list /
  delete` (over USB, using fm1-base's app USB commands / data API). If it works, ALL
  songs move out of the firmware image.
- Smaller files: delta-coded events (format v1.1; the device keeps reading v1.0).
- Safety (new write path): boot never depends on songs; a corrupt or empty song area is
  tolerated (songs checked at first use, a bad one shows SONG ERROR, the rest work); the
  laptop validates before sending and the device validates the whole file before
  writing; writes stay inside the song area (assert the address range); a power cut
  during a send leaves at worst that one song broken. Test all of this in the emulator
  (fm1-live --usb / FM1_USB_SEND, FM1_FLASH_CUT).
- Empty song area: the list says how to send songs (no crash, no blank screen).

PART 2 — Practice controls and fixes (items 1-7, 9, 13, 15, B)
- Tempo in BPM, 30-250, 1 BPM a detent, faster on quick turns; amber when not the
  song's own; each song opens at its own BPM. Check both extremes (events-per-block
  cap, CPU).
- Boot splash ≥3 s with firmware/assets/tmp-logo.jpg (the user's placeholder),
  converted at build time and compressed hard (e.g. ~120-160 px wide, palette + RLE);
  ONE file to replace for a new logo. Power-on rescue (REC, OCT-, OCT+ held at start)
  still works and isn't delayed.
- Louder count-in, beat 1 accented.
- Loops and start points, designed together: loop a section (found on screen, clear
  way to pick it), loop any bars A-B, start playback from a section or bar A. Keep the
  pickup rules (docs/song-format.md 3.3).
- Pause / resume from the same place (count-in on resume if count-in is on); stop and
  back to the top still easy.
- Bar strip: no dimming of held-over chords.
- Four Chords crackle: measure peak levels of every song (emulator WAVs, all layers,
  L2, MASTER 100 %); find clipping vs instrument; fix (instrument, layer volumes, soft
  limiter); before/after numbers in LOG.
- OCT- / OCT+ after boot shift the octave of the keys the user plays (shown briefly);
  holding them at power-on stays the rescue (test it). REC stays unused (D021).
- B. Beat light: PLAY's LED (or a screen dot) flashes on every beat, brighter on
  beat 1, during playback and count-in.

EARLY INSTALL POINT (the user's weekly usage may run out): after Part 2 is committed
and tested, write docs/step-M2-part2-install.md for that build (same rules as the final
checklist: install over M1, first song send, on-unit checks for parts 1-2, fallback = M1),
log "Part 2 install checklist ready", then CONTINUE with Part 3 (don't stop). If the user
says "finish the current part, then stop", do that and write the checklist for the last
completed part.

PART 3 — Views and guidance (items 8, 10, 12, F, G)
- 8. Change key live: whole band except drums; chord names re-spelled for the new
  key (F# vs Gb by key); ranges kept comfortable (octave folding for bass/melody);
  everything below follows the transposed key. Shown on screen, reset per song.
- 12. Keyboard view: the current chord's keys (voicing of the harmony level) on a
  drawn keyboard; optional chart view (the song's bars and chords, current bar marked).
- F. Bass note names: for a muted bass, show the bass note(s) to play (note name, and
  the next one), following the bass level.
- 10. Rhythm cue next to the chord: when/how long to play at the chosen level, shown
  also when the layer is muted (e.g. whole note at L1, blinking quarter at L2; derive
  it from the layer's own rhythm in the compiled events where possible).
- G. Key lights (plat_lights / plat_leds in fm1-base's plat.h): light the FM-1's keys
  for the part the user is playing: harmony muted -> the current chord's keys (the
  level's voicing), and the NEXT chord dimly in the last bar before a change; bass
  muted -> the bass note (folded into the keys' range F3-G5). If more than one of
  harmony/bass is muted, propose which wins (or a control to choose). Respect
  app_key_led_ok() / the scan limits noted in plat.h. Keys the user presses must still
  play and light normally.

PART 4 — Three more songs (E), with songs/make_songs.py, public domain, our own
arrangements, sent with fm1song (not built in):
- Greensleeves (A minor, 3/4): minor key.
- Danny Boy / Londonderry Air (slow ballad, slash chords like C/E).
- Oh! Susanna (major, fast: tempo extremes).
Each with drums/bass/harmony :1 and :2, melody, chart, sections, previews; check
rights (well-known modern arrangements are copyrighted: write our own, mark "check
before publishing" where in doubt). Make L1 and L2 drums clearly different in these
three (user feedback; the old songs stay as they are for now).

PART 5 — Tests, budget, install checklist
- Emulator tests for every item and every song (8 songs), incl. flash IDs 856014 /
  b36014 / unknown, a corrupt song, an empty song area, a cut-off send.
- Flash / RAM / CPU budget report. Version 0.3.0-m2, release build, new identity.
- docs/step-M2-install.md (rules as M1's): step 1 install M2 over M1; step 2 the FIRST
  song send (all 8 songs) — a new kind of write, explained (what area, what if cut
  off); step 3 on-unit checks for every item; fallback = reinstall M1 (its built-in
  songs still work). STOP for review.
```
