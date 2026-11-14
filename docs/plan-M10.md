# Milestone M10: instructions. BACKTRACK polish, 6 progressions, 6 songs with a song info line, user presets, band kits

From the planning chat, 2026-11-13. Scope: vault decisions D056 to D062 (the user's M9 notes are in the vault's
Brainstorms/2026-11-04 Prototype feedback, 2026-11-13 "M9 build"). M10 **includes** what was M9.1: saving user presets
(D040) and BACKTRACK band kits (the user's choice, D056).

State at the start: the user installed 0.9.0-m9 (FM-1_46000900) and ran docs/step-M9-install.md steps 1 to 5: all
fine (bt06-la-bamba, bt13-mario and bt14-creep deleted, the 36 backtracks sent). Record that in LOG and device.md first.

## Read this first (the coder for M10 is Sonnet)

This plan is deliberately prescriptive. Follow it literally.
- **MUST / MUST NOT** are hard rules. Everything else is the intended design; if it turns out impossible or clearly
  worse in the code, choose the SMALLEST change that still passes the part's checks, then record it (below).
- **docs/decisions-M10.md (new; you write it).** Every judgment call this plan does not spell out goes there, one
  entry each, numbered M10-01, M10-02, ..., in the format of docs/decisions-M9.md:
  ```
  ## M10-NN — <short title> (Part X)
  - Situation: what the plan did not cover, or why it could not be followed as written
  - Options: A ..., B ... (at least two)
  - Chosen: A, because ...
  - Undo: how to reverse it (files, functions)
  ```
  Small choices count (a name, a colour, a threshold, a pixel position). LOG points to the decision numbers.
- **MUST NOT** change: the levels SONG LEARN generates for a song of kind "song" (the event streams of the 9 older
  public songs and the private songs MUST stay identical; the ONLY allowed difference in their compiled files is the
  new INFO chunk of Part 5); the song store and its flash code; the settings store's existing fields; the rescue
  combos; the hardware safety rules in CLAUDE.md.
- **MUST NOT** refactor code a part does not need. No renames of existing functions or files unless a part says so.
- **Bugs: test first** (Part 1): write the failing emulator check, see it FAIL, record that in LOG, fix, see it PASS.
- **The 6 new songs are NOT yours to write.** The planning chat wrote them (D059): `songs/m10_songs.py` (melodies,
  charts, credits, info lines) and the 6 folders it generated with `songs/make_songs.py`. Commit them as they are
  (Part 0). MUST NOT change a melody, a chord or a form. If one fails a check, record it in LOG and
  decisions-M10.md and leave the song out of the send list; the planning chat fixes it.
- If a part's checks fail and you cannot fix them with reasonable effort: stop that part, record exactly what fails
  in LOG ("Status: blocked"), and go on to the next part only if it does not depend on it.
- **One early install point** (after Part 6, before any flash-write code): build the package and write
  docs/step-M10-early-install.md, but do NOT stop; carry on with Part 7. The user may install the early package
  while you work, so the preset-saving code (Part 7) gets its own install (D051's reason, kept).

```text
Follow CLAUDE.md and docs/HANDOFF.md. Build Milestone M10 in the parts below, in PRIORITY order. Commit after each
part. Fast checks after each part; the FULL suite (the list in Part 9) only ONCE, on the final package, via the
test-runner subagent. Emulator with the M5.1 flash timing. Mock screenshots in docs/controls.md ("M10" section)
for every new or changed screen. If the user is away: decide, record in docs/decisions-M10.md and LOG briefly,
carry on. Never relax a check to pass. Never run a device command. New checks go in tools/m10_test.py (copy the
structure of tools/m9_test.py).

PART 0 — Housekeeping (no firmware behaviour change)
- LOG + device.md: the M9 install (above).
- Commit the planning chat's songs/m10_songs.py, the 5-line change at the end of STYLE_OF in songs/make_songs.py,
  and the 6 song folders (songs/camptown-races, clementine, auld-lang-syne, drunken-sailor, blow-the-man-down,
  michael-row). Six more songs in m10_songs.py's DROPPED_SONGS were dropped by the user: never generate, commit or
  send them. Run `.venv/bin/python songs/make_songs.py <those 6 slugs>` once:
  the files MUST come out byte-identical to the committed ones (the planning chat ran it with Python 3.10; if 3.12
  differs, record it and keep yours). Then `tools/fm1song.sh validate` on each: all ok.
- songs/hallelujah/ (the user's untracked import, a copyrighted song): `mv songs/hallelujah songs/private/hallelujah`
  (songs/private/ is git-ignored). No test may read songs/private/hallelujah; tools/song_fuzz.sh and the m5/m51
  fixtures take only tracked songs (M9's correction).
- Trace the m5 check that failed in M9's accidental run ("Happy Birthday's expectation", LOG 2026-11-13): run that
  check alone on the M9 package, clean tree. Record the cause. If it is a stale expectation, fix the expectation
  (exact new text/number, as M8 did); if it is a firmware bug, record it as "blocked: for the planning chat" and go on.
- LOG: free app space, RAM .data+.bss, BACKTRACK's CPU worst case (the M9 figure, re-measured), the song store's
  blocks with 13 + 6 songs + 42 backtracks (count it with the compiler's sizes, no device).

PART 1 — Fixes from the M9 review (test first)
(a) M9-07 is REVERSED: a section's crash no longer forces a kick. At the first bar of every section except INTRO
    (tier 2 and up) add only the crash (49, vel 92); the kick stays whatever the tier pattern has (REGGAE's one-drop
    has none on beat 1). Both gen.c bt_drums and arrange.band_drums; C = Python test for all 11 styles x 4 tiers
    regenerated. Failing check first: REGGAE tier 2, the first bar of a chorus has no kick on beat 1.
(b) M9-17 is CHANGED: a change made while PAUSED keeps the pause. The new version is built (the 300 ms rule) and
    the player stays paused at the mapped position (bt_map_bar: the same section and bar); PLAY resumes there with the
    count-in rule of D055 (a count-in after PLAY from pause, if on). While STOPPED: unchanged (made at once; PLAY
    starts from the loop's start). Failing check first: pause in bar 3 of a VERSE, K2 to ROCK, PLAY: the music starts
    in bar 3 of that VERSE, not at the loop's start.
- m10p1: (a) and (b), and that the M9 checks m9p1 and m9p4 still pass.

PART 2 — BACKTRACK screens (D057; BACKTRACK only, SONG LEARN unchanged)
(a) The VALUE POP-UP. Turning K1, K2, K3, K4, PRESETS, ALGO or SELECT on SETUP or on the play screen opens a pop-up
    over the screen: a title line and the value in a big font (FONT_B, or bigger if there is one that fits 200 px).
    Titles, exactly: K1 "Progression", K2 "Style", K3 "Song Complexity", K4 "Melody Sound", PRESETS "Key",
    ALGO "Structure", SELECT "Tempo". Values: K1 "<number> <name>" and the MAJOR / MINOR badge under it; K2 the style
    name; K3 SIMPLE / MEDIUM / BUSY; K4 the preset name; PRESETS the key as SETUP shows it ("C major" / "A minor");
    ALGO LOOP / VERSE/CHORUS / FULL; SELECT "<n> BPM". On the play screen, for a change that waits (K1, K2, K3, PRESETS,
    ALGO), a third line in FONT_XS: "at the next phrase" or "at the next bar" (D053's classes).
    - The pop-up follows the knob while it turns (another knob replaces it with its own title). It closes by itself
      1,500 ms after the last turn. Any button press closes it AND does its normal job (unlike D044's NOT IMPLEMENTED
      pop-up, which eats the press). HOME held 2 s always works.
    - It is drawn in the same way as D044's pop-up (ui.c draw_popup: the screen under it frozen) but it is a separate
      state (do not reuse ui.popup's eat-the-press rule). Size and position: your choice, centred, readable; record it.
    - While the pop-up is open the music and the key lights carry on; the screen under it updates when it closes.
(b) Names, not numbers, for the complexity everywhere: SIMPLE / MEDIUM / BUSY on SETUP, in the pop-up, in the NEXT
    line and on the play screen's top line. Nowhere "C1", "C2", "C3" or "Cn".
(c) The NEXT line spells the setting out: "NEXT: COMPLEXITY BUSY", "NEXT: STYLE REGGAE", "NEXT: KEY D", "NEXT: PROG 7"
    (the progression by its number: "PROGRESSION" does not fit with a name), "NEXT: STRUCTURE FULL". Several waiting
    changes: the first in this order (progression, style, structure, key, complexity), then "+n". FONT_XS; if a line
    does not fit 236 px, shorten the value, never the setting's word.
(d) K4's label on SETUP: "MELODY SOUND" (was "SOUND"). On the help screens too.
(e) The SECTION INDICATOR (play screen only). The section's name in capitals (INTRO, VERSE, CHORUS, BRIDGE, ENDING,
    from the FORM chunk's kind; a numbered section shows its name without the number) at the right end of the top
    line, where the play screen now shows the MAJOR / MINOR badge (the badge stays on SETUP and in the pop-up; record
    the move as a decision). In the LAST BAR before a different section starts, the indicator shows the NEXT
    section's name and blinks (on 250 ms, off 250 ms) until that section starts, then shows it steadily. In LOOP (one
    section repeating) it never blinks. At a loop wrap to a different section (VERSE/CHORUS) it blinks as at any
    change. The blink follows the beat light's time base (no extra timer).
- m10p2: each title and value of the pop-up; the 1,500 ms close; a press closes it and acts; the NEXT texts;
  SIMPLE / MEDIUM / BUSY on every screen; the indicator's name in every section of a FULL run and its blink starting
  exactly one bar before each change (+-1 frame), none in LOOP. The button census of SETUP and the play screen
  (m9census) still passes.

PART 3 — The mode screen and the splash (D058)
- **The name is now Tuneka** (D063; it replaces Notami everywhere on the unit): APP_NAME in firmware/app/app.h becomes
  "Tuneka - Song Sidekick" (the splash, the help, fm1-base's crash / safe-mode screens: everything that shows it),
  firmware/app/words.tsv's line likewise ("Tuneka, song sidekick"), and the tests that expect the old text (e.g.
  tools/m8_test.py's splash check) get the exact new text. USB / MIDI device names unchanged. grep the firmware, tools
  and docs/controls.md for "Notami" afterwards: none left outside history (LOG, HANDOFF, old plans).
- The mode screen: a title "Tuneka - Song Sidekick" across the top (FONT_B if it fits 240 px with a 4 px margin,
  else the next size down), the mascot scaled down to make room (nearest-neighbour or 2:1 from the asset, no blur),
  then the "modes:" line and the three choices as now. Nothing else moves except what has to.
- The splash: "Tuneka - Song Sidekick" (was "Notami - SongSidekick") and "m10" under it.
- m10p3: screenshots; the text fits; the cursor still starts on SONG LEARN.

PART 4 — Six more progressions (D060): 42 in all
- tools/backtracks.tsv, rows 37 to 42, exactly (tab separated as the file):
  37 PHRYGIAN       i bII                       -  minor
  38 LINE CLICHE    i imaj7 i7 i6               -  minor
  39 MINOR CADENCE  i iv V i                    -  minor
  40 DESCENT        IVmaj7 iii7 ii7 Imaj7       -  major
  41 LYDIAN         I II                        -  major
  42 JAZZ BLUES     I7 IV7 I7 I7 IV7 IV7 I7 VI7 ii7 V7 I7 V7   -  blues
- tools/backtracks.py's numeral parser MUST learn "maj7" and "6" on a MINOR numeral (imaj7 -> Am(maj7) = "Ammaj7",
  i6 -> "Am6"); today it silently drops "maj7" on a minor numeral: make any unknown numeral+extension an error
  instead of a silent drop, with a test. "II" (upper case) is a major chord on the second degree (D in C).
- Regenerate songs/backtracks/ (42 folders); the 36 existing ones MUST be byte-identical (the tier generators did not
  change after Part 1's crash fix; if Part 1 changed their compiled files, that is expected: list which).
- Two copies of the largest backtrack MUST still fit BACKTRACK's song room (56,448 B each, M9-01): check every new
  one in every style at every tier; record the largest.
- m10p4: K1 reaches 42; each new one plays in POP and in its own suggested style (JAZZ BLUES: SHUFFLE); the badges.

PART 5 — The song info line (D062)
- song.yaml gets an optional `info:` (printable ASCII, <= 38 characters). songs/make_songs.py writes it from the
  song's `info=` (the 6 new songs have it already). Add `info=` to the 9 older public songs, exactly:
  amazing-grace "Trad. hymn tune, 1830s; words J. Newton", twelve-bar-blues "Tuneka original, 12-bar blues form",
  saints "Traditional spiritual, c. 1896", four-chords "Tuneka original", scarborough-fair "Traditional English
  ballad", greensleeves "Traditional English, 16th century", happy-birthday "Mildred J. Hill & Patty Hill, 1893",
  danny-boy "Irish traditional, first printed 1855", oh-susanna "Stephen Foster, 1848". Private and imported songs:
  none (the importer writes no info line).
- The compiler stores it in a new optional chunk `INFO` (u8 length, the text), written only when `info:` is set.
  Check song.c's parser first: if the M9 firmware already skips unknown chunks, the format stays 2.2 with an optional
  chunk; if it refuses them, the format becomes 2.3 for songs with INFO only (record which, M10-NN). An M9 FM-1 must
  never crash on a file with INFO (test it: the M9 package and one new file).
- The goldens: for every older song, the compiled file MUST equal the M9 one plus the INFO chunk (a test removes the
  chunk and compares byte for byte); every event stream identical.
- The FM-1: on the SONG LEARN Settings screen (ui.c draw_settings), WHEN A SONG IS LOADED, the last line of the screen
  shows that song's info line in FONT_XS, dimmed (the unselected subtitle colour). Nothing when no song is loaded or
  the song has no INFO. Nowhere else: not on the list, the play screen, BACKTRACK or SYNTH.
- docs/song-format.md: `info:` and the INFO chunk. HANDOFF note for the website: the JavaScript port (D036) must
  follow (it is the planning chat's job there, not yours).
- m10p5: the line for a song with INFO; none for a private song; none with no song loaded; the M9-package test.

PART 6 — Band kits for BACKTRACK (D062)
- A KIT chosen on SETUP only (not on the play screen): **SEQ** on SETUP steps to the next kit (it now shows NOT
  IMPLEMENTED there), with the value pop-up (Part 2) titled "Band Kit". A new SETUP row "KIT  <name>" (FONT_XS, under
  TEMPO; move rows up to 2 px each if needed, nothing cut). Resets at power-off like every BACKTRACK choice (D051:
  the user keeps BACKTRACK resetting). On the play screen SEQ stays NOT IMPLEMENTED.
- Four kits, in this order (drums / bass GM program / harmony GM program; 1-based GM numbers):
  STUDIO    the GM kit (EAS) / the style's programs as today (the default: M9's sound, unchanged)
  ACOUSTIC  the GM kit / 33 Acoustic Bass / 25 Acoustic Guitar (nylon)
  ELECTRO   SYNTH's 808 kit / 39 Synth Bass 1 / 90 Pad 2 (warm)
  CHIP      SYNTH's CHIP kit / 39 Synth Bass 1 / 5 Electric Piano 1
  Reuse SYNTH's drum voices (synth.c KIT_*: the 808 and CHIP kits, up to 6 drum voices) for the backtrack's drum
  layer; the tier tables are the same for every kit (a kit changes the sound, never the notes). Never GM 81 (the M2
  aliasing crackle).
- CPU: the worst case of M9 (44 %) with each kit, FULL, BUSY in a chorus, MIAMI PAD x 4 voices: MUST stay <= 60 %.
  If a kit goes over, drop that kit (keep the others), record it.
- m10p6: each kit plays (hits counted per kit, notes identical across kits); SEQ on SETUP cycles STUDIO..CHIP..STUDIO;
  the pop-up; reset at power-off; CPU per kit.

EARLY INSTALL POINT (after Part 6; do not stop)
- Release build 0.10.0-m10a (identity FM-1_46001001; the splash says "m10"), reproducible (two builds); m10p1..p6,
  m9p1, m9p4, m9census and pytest on it. Write docs/step-M10-early-install.md (as step-M9-install.md: install over M9,
  then send the 6 new backtracks and the 6 new songs, and resend the 9 older public songs and the 36 backtracks only
  if their files changed: list exactly which). Carry on with Part 7.

PART 7 — Saving user presets (D040, D062). The first new kind of flash write since M5.1: follow D040 exactly.
- Storage (D040, MUST): one fm1-base store object (Felucca storage.c: A/B sector pair, CRC, header written last,
  torn-write safe) at 0xDE000-0xDFFFF, <= 3,840 B: a header and 30 records of the M7 preset record (<= 64 B, versioned:
  an older version loads with defaults for new parameters). Built in RAM, never written from a const table (the M5
  crash). The watchdog fed through the write; the audio muted for the save moment (no click: fade out / in).
  Part 0 of M7 confirmed nothing uses 0xDE000: confirm again in code (grep every flash address) and record it.
- Start-up MUST NOT depend on the object's state: no object, a torn write, a bad CRC, junk from another firmware
  -> all 30 slots EMPTY, no crash, no write at start-up. "Wipe song store" never touches it.
- SYNTH, SAVE (it shows NOT IMPLEMENTED now): opens a SAVE screen:
  - "SAVE TO U01" .. "U30": SELECT chooses the slot; each slot shows its name or "EMPTY".
  - The name: the current preset's name (<= 12 characters) to start with; K1 moves the cursor, K2 changes the
    character (A-Z, 0-9, space, -), shown with a cursor under it.
  - SAVE again = save. If the slot holds a preset: "REPLACE U05 <name>?  SAVE = yes  HOME = no". HOME = cancel (back,
    nothing written). After the write: "SAVED U05" (the value pop-up style, 1,500 ms) and the sound goes on as the
    saved preset (now selected as U05).
  - Stock presets 1-30 can never be written or replaced.
- PRESET knob in SYNTH: 1..30 the stock presets, then the user slots that are not EMPTY, in slot order (shown as
  "U05 <name>"). BACKTRACK K4 (melody sound): the same list (stock, then the saved user presets).
- No delete in M10 (overwrite only): backlog.
- Tests (MUST, the M5.1 rules): fm1-base storage_test with a power cut at every flash operation of a save (both
  copies; the older copy survives a cut); the TIMED emulator (FM1_FLASH_TIMED=1, as m51): save 30 slots, power cut
  during a save at 20 points, boot after each: no watchdog reset, no XIP read while a flash command has it off,
  longest unfed stretch < 1000 ms; junk in 0xDE000 at boot; the song store untouched by a save (byte compare), the
  object untouched by sends, deletes and WIPE; an old-version record loads with defaults.
- m10p7 in tools/m10_test.py, plus the timed runs in tools/m10_flash_test.py (copy tools/m51_test.py's runner).

PART 8 — Docs
- docs/controls.md "M10" (every new or changed screen, screenshots from the release run), docs/song-format.md
  (INFO; the new progressions), docs/decisions-M10.md complete.
- A NOTICE file at the repo root: the third-party code we ship and its licence, with paths: fm1-base / felucca-bsp
  (MPL-2.0; say which of its files we patched), Sonivox EAS and its GM bank (Apache-2.0, with its NOTICE text if it
  has one), Felucca-Engines (MIT, its copyright line). Our own code: "Licence: TBD (D017)". Read the licences from
  third_party/ and firmware/vendor/; do not guess. (The user is leaning towards open source and decides later.)

PART 9 — Tests on the final package, budget, install checklist
- Release build 0.10.0-m10 (identity FM-1_46001000), reproducible (two builds), from a clean tree (`git status`
  clean apart from build/; songs/private/ not read).
- The FULL suite ONCE on it (a beta candidate: M9-21 is suspended for M10 unless the prompt says otherwise), via the
  test-runner subagent, timings in build/step-M10/times.txt. It is TRIMMED to what M10 can break (D064): every song
  is still checked cheaply by pytest's golden compares; the emulator plays only a sample. Run it in THREE LANES at
  once, each with its own --out folder (build/step-M10/emu-<suite>), from a clean tree (tracked songs plus the 5 private songs the suites always used; never songs/private/hallelujah):
  Lane A (laptop tools, in order): store_test; pytest (all goldens: every song's events identical, the INFO-only
    difference, C = Python for every backtrack, style and tier); vsynth; guitar/ukulele; song_fuzz 600 s (Part 5
    adds parser code: keep the full 600 s).
  Lane B (emulator, features):
    - m5 with --only: store, flash (the flash IDs 0x856014, 0xB36014, unknown: on THIS package), boot, practice,
      views, levels, part1..part9, m4p1..m4p5, m5p1..m5p3, and the per-song playback for 9 songs only: the 6 new ones,
      twelve-bar-blues (shuffle), four-chords (pop), and the largest private song (the song buffer's edge). Not every
      song: SONG LEARN's generation MUST NOT change (pytest proves it), so playing all 19 again adds nothing.
    - m6 --quick (no loudness runs: the levels are unchanged by rule; the known Zombie item is not re-measured).
    - m7 --only m7p1,m7p2,m7p4,m7p5,m7p6,m7cpu (skip m7p3: the effects are untouched; m7p4 stays: kits reuse
      SYNTH's drum voices).
    - m8 default set (m8p1, m8p4, m8p5, m8p6; the splash check expects "Tuneka - Song Sidekick").
    - m9 --only m9p1,m9p2,m9p4,m9census,m9cpu, and m9sweep TRIMMED to complexity 1 and 3 with progressions 1 and 42
      (11 styles x 3 structures x 2 x 2 = 132 runs instead of 396): set it with an env variable or option, record how.
    - m10: m10p1..m10p7 (m10p6 plays every kit in every style at FULL, BUSY).
  Lane C (timed flash): m51 (the song store, unchanged scope: it is the M5 crash guard and shares the flash driver);
    m10_flash (the presets).
  A failure that is a stale expectation: fix the expectation to the exact new value and re-run that group on the
  same package; anything else: record and report, do not relax. If two lanes at once overload the laptop (an
  emulator run reports late blocks only when another lane runs), run lanes B and C one after the other and record it.
- Budget table as step-M9-install.md, plus: app space left (MUST >= 16 KB), the preset object's size, the song store
  with everything sent, CPU per kit.
- docs/step-M10-install.md: install over M9 or over M10a; delete nothing; send the new songs and backtracks (and the
  changed ones, listed); a SAVE test the user does by hand (save a preset, power off/on, it is there; replace it;
  power off during nothing); the known items; the restore paths (M9 package as fallback; a preset object left behind
  by M10 is ignored by M9: say so after checking).
- STOP. The user installs; you run only --info / fm1song list and record.
```

Not in M10: a pop-up or section indicator in SONG LEARN (backlog, D057); deleting user presets; kits on the play
screen; remembering BACKTRACK settings after power-off (the user keeps the reset); a public release, its licence and
the beta gate (the user decides later); the website's JavaScript port catching up (the planning chat).
