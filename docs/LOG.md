# LOG

Written by Claude Code, append-only, newest entry at the bottom. The planning
chat reads this. One entry after every step and before ending a session, in
this format:

```
YYYY-MM-DD HH:MM — Step X.Y —
Status: done | partial | blocked
Did: bullets
Learned / measured: bullets (versions, IDs, numbers, file paths)
Hardware touched: none | read-only | WRITE (what, by whom)
Questions for the planning chat: bullets, or "none"
Next: the next step as you understand it
```

---

*(Entries before Milestone M5: docs/archive/LOG-before-M5.md.)*

2026-11-06 16:08 — Milestone M5, Part 1 (fixes from the M4 test) —
Status: done (emulator); committed; tested release build kept
Did (docs/controls.md "M5 Part 1"):
- **Equal loudness:** every layer was measured alone at every level in every song, 12 s from the first bar, before
  and after (tools/m5_test.py level_loudness).
  - Generated levels: harmony L4/L5 velocities 58/48 → 70/58 (arpeggio 70/56), bass L1 84 → 97.
  - Imported parts: scaled to the loudness of the generated level they replace (importer.match_loudness, a model
    where amplitude goes with velocity squared).
- **Let It Be:** the harmony is the file's piano alone; it was piano + organ, and the organ held every chord.
- **Bass in its real octave:** generated roots E1–D#2 (tools/fm1song/arrange.py).
  - Setting "Bass sound: REAL / +1 OCTAVE", default +1 OCTAVE. The player sounds the bass layer +12 and releases
    its notes when the setting changes.
  - The bass-clef notation is written an octave up.
  - The bass tab chooses a hand position: 4 frets, the nearest string, a shift costing by its length, open strings
    free. The position comes from the 8 notes before, so a note never changes as the view scrolls.
  - The tab labels were already right (G at the top). The user saw the D/G strings because the bass was an octave
    high.
- **Harmony L4/L5 lights:** every note the harmony plays in the chord lights (the octave notes of the arpeggio
  were missing).
- **Top line:** the label is laid out first and the second line ends before it (no overlap).
- **Track boxes:** "L1" and "FULL" apart, in two colours.
- **Fresh start per song:** the view; the key and tempo were already reset.
- **Splash:** 4 s, plus a start-up melody: our own motif C5 E5 G5 B5 → E6 on electric piano, reverb send 70,
  channel volume 80, velocity 84/70.
- **Controls hint pop-up:** before a song's play screen, up to 10 times, then off; setting "Controls hint"; the
  stopped legend alternates with the short form.
- **Settings:** the record grew at its end (v2[8], hint_count). **An M4 record is read**, with the new fields set
  to their defaults (no reset this time). 10 rows, scrolling.
- **tools/m5_test.py** (from m4_test.py):
  - OPEN_MS 4400 / PLAY_MS 6200 for the 4 s splash, and early literal times moved.
  - The test flash has the controls hint turned off; a copy with it on is kept for the hint checks.
  - The new m5p1 group.
Learned / measured:
- **Level loudness, before (M4 build) → after (Part 1 build),** RMS per level:
  - generated songs: harmony L4/L5 were 0.70x of L1–L3 and bass L1 0.75x of L2–L4. Now every layer of all 9 songs
    is within 1.6x (worst Danny Boy harmony 1.40);
  - Let It Be harmony L3 3538 → 723 (the others about 740);
  - Creep bass L3 4231 → 1913 (1660–1733);
  - Rockin bass L4 5528 → 2051 (1950–2537);
  - True Colors harmony L3 1041 → 231 (about 580): now too soft;
  - Zombie harmony L3 651 → 236 (about 395), bass L4 1085 (about 2100): too soft.
  - The loudness model is approximate for strummed guitar parts. Within 2.5x everywhere, against up to 5x before.
- m5p1 on the dev build: ALL PASS.
  - The splash is still up at 3.9 s, and the melody plays its 5 notes.
  - The hint shows 10 times, then is off.
  - View, key and tempo reset when a song opens again.
  - The bass is written 28..46; it sounds 48 (+1 octave) or 36 (REAL).
  - L5 lights 4 keys.
- Release build build/step-M5/part1/fm1acc-0.5.1-m5p1.fwsc: FM-1_46000501, sha256
  34104a5b22fecce8124200b8592a2019586ca1d09378a2b9b3ddce1631f929fb. pytest 78 pass.
Choices (with reasons):
- **Loudness:** fixed velocity factors for generated levels, and a per-part scale at import. These are things the
  device's generator (Part 2) can repeat exactly; a whole-level normalization pass on the device would not be.
- **Start-up melody:** our own rising major-7th motif (the plan's suggestion).
- **Controls hint:** saving its count writes the settings sectors at most 10 times.
Hardware touched: none
Questions for the planning chat:
- The private songs' file parts are now within 2.5x but some are softer than the rest. Is that good enough for test
  songs, or worth a measured calibration later?
Next: Part 2 (storage: generate levels on the FM-1, play only the chosen level, format 2.0, store packing).

2026-11-06 19:40 — Step M5 Part 2 (storage, D029 A+B+C) —
Status: done (emulator); committed; early checklist 1 written
Did:
- Format 2.0: chart, sections, melody, style (STYL) and only the stored levels (NT chunks, bars as patterns); the
  FM-1 generates harmony L1-5, bass L1-4, drums L1-3 (firmware/app/gen.c, a port of arrange.py + arrange.canonical).
- Plays one level a layer (song_build into a region per layer, 192 KiB; player_hold while a level is rebuilt).
- Store v2: 512 B blocks, several songs a sector, delete = magic zeroed, receive straight into flash, compaction
  (dry run on a model, one sector kept free), M4's FM1R records still read. Up to 200 songs.
- fm1song: 2.0 by default; to an M4 FM-1 it sends 1.3; SEND carries the slug. Importer: no cutting.
- Tests: pytest 93 (golden C vs Python), store_test ALL PASS, fuzz, m5_test p2 checks; docs updated.
Learned / measured:
- Our 9 songs 168,712 B (1.3) -> 11,312 B (2.0), 28 blocks in 4 sectors. All 14 songs (5 private, whole): 252 of
  552 blocks. C and Python generators: identical events, all 14 songs, every level.
- Package build/step-M5/part2/fm1acc-0.5.2-m5p2.fwsc, FM-1_46000502, sha256 968f0650...3301 (rebuilt from the
  commit's tree: the same hash). Full suite on it: 276 checks ALL PASS (build/step-M5/part2/emu-full/report.md).
  Fuzz 600 s: 768,000 files, no report. RAM .data+.bss 74,736, pool 289,312 of 344,064.
- Choices made while away: delete writes zeros over a record's magic (NOR bit-clearing, as fl_write allows); a
  record of <= 8 blocks stays inside its sector; 2.0 needs ppq 480 and notes in both voicings of a rooted chord.
- HANDOFF 17:15 (trim Part 3, stop after it) followed: it narrows the prompt; checklist named
  docs/step-M5-early-install-1.md.
Hardware touched: none
Questions for the planning chat: none
Next: Part 3 trimmed (mode screen, SYNTH with ANALOG, chorus, delay, arp, 20 presets), early checklist 2, STOP.

2026-11-06 21:30 — Step M5 Part 3 (trimmed per HANDOFF 17:15: mode screen + SYNTH) —
Status: done (emulator); committed; early checklist 2 written; STOP for review (Parts 4-5: next milestone)
Did:
- Mode screen after the splash (mascot, LEARN / SYNTH, cursor on LEARN; SELECT, PLAY). Rescue unaffected.
- SYNTH: Felucca-Engines' ANALOG only (MIT files vendored in firmware/vendor/felucca-engines, VENDORED.md); our
  host (8 voices, ADSR, PWM LFO), chorus, ping-pong delay (dotted eighth at the tempo), arpeggiator (up/down/up-down,
  1/4 1/8 1/16, 60-200 BPM), OCT, knobs (K1-K4 cut res atk rel, ALGO chorus, PRESET delay), 21 presets of ours.
- Delay/chorus lines reuse the song buffer; a SEND in SYNTH mode is refused (result 9); LIST reports the mode.
- tools/vsynth_test.sh (laptop, ASan/UBSan): 21 presets sound, no clipping, voices end, arp steps.
Learned / measured:
- Package build/step-M5/part3/fm1acc-0.5.3-m5p3.fwsc, FM-1_46000503, sha256 ee11165d...d4ce. Full suite on it:
  286 checks ALL PASS (build/step-M5/part3/emu/report.md).
- Budget: image 448,736 B; RAM .data+.bss 76,032 of 98,304; pool 289,312 of 344,064; SYNTH 8 voices (SYNC SCREAM,
  chorus + delay): longest render 24 % of a block (emulated).
- Choices: no reverb, no PHASE, no random/latch (trimmed); mode fixed until power-off; ARP toggles on release (so
  ARP held + SELECT sets the tempo); fixed velocity (the keys have none).
Hardware touched: none
Questions for the planning chat:
- Review docs/step-M5-early-install-2.md (it supersedes -1 unless that was run).
Next: STOP. Parts 4 (views) and 5 move to the next milestone, per HANDOFF.

2026-11-06 21:50 — Incident: M5 0.5.3-m5p3 in SAFE MODE after song sends (HANDOFF 21:50) — read-only diagnosis —
Status: partial (diagnosed read-only; M4 reinstall command given to the user; root cause for the next milestone)
Did (all read-only, Claude Code):
- lsusb (21:46:38): 1209:0001 present (our app's USB ID; safe mode keeps USB on).
- Kernel log, port 1-2, no -110 errors: 21:40:50 M4 app; 21:42:04 update loader 1209:0002 (the M5 install);
  21:42:58 M5 app; 21:43:31 disconnect (sending); 21:43:32 app; 21:43:41 disconnect; 21:43:42 app (= safe mode).
- fm1_install.py --info: `FM-1_46000503  [running]  port: FM-1 Accompaniment:FM-1 Accompaniment MIDI 1 24:0`.
- fm1_remote.py log (one read-only question, F0 7D 4C 47 F7; build/step-M5/part3/crash-log.txt):
  `no crash recorded`; `last watchdog reset in USB`; rails boot 0B 1B now 0F 1F; clock/pll as before.
Learned / measured:
- Not a fault: a watchdog reset (armed 0x0D, ~8 s) while the main loop was in its USB step, i.e. inside our song
  SEND handler (library.c sc_send). Two such resets soon after start-up count as failed boots -> SAFE MODE; the third
  send got no answer ("stopped answering"). Start-up itself is probably not the problem (the store is scanned only
  when LEARN shows the list), but that is not proven.
- Why the emulator missed it: fm1-emu models the watchdog, but flash erase / program are instantaneous there, and its
  XIP reads cost nothing like the real chip's. fm1-base's ota_read feeds the watchdog while waiting, so the transfer
  is not it; the long stretches without a feed are ours: store_begin (scan, tidy, the compaction dry run of up to
  1173 planning steps, moves = page programs that do not feed, a full rescan after every step), then the in-place
  check (CRC, song_open with the 2.0 counting) and store_commit (two rescans, deletes). Each rescan reads 552 blocks
  through XIP. Unverified which one passed 8 s.
- The song store may now hold a partly written M5 record; M4 reads only whole-sector FM1R heads and erases before it
  writes, so it is expected to cope; M4 sees M5's songs as absent.
- Fallback package build/step-M4/final/fm1acc-0.5.0-m4.fwsc sha256 c811e0f7...195b (checked); fm1-base unchanged
  since 98fc08c. After the install the boot guard may show SAFE MODE once more (failed-boot count in RAM that a
  reset keeps): PLAY ("try again") or power off/on clears it.
Hardware touched: read-only (--info, the log question, lsusb, kernel log). No install, send or delete.
Questions for the planning chat:
- For the fix (next milestone): feed the watchdog in the store's long loops and between flash operations (or move
  compaction out of the SEND path, e.g. a "make room" step with its own progress), measure each step on hardware
  timing (add erase/program delays and XIP read cost to the tests), make start-up independent of the store, add a
  "wipe the song store" recovery action. Don't reinstall M5 until then.
Next: the user reinstalls M4 (command given in chat), Claude Code checks with --info and records the result.

2026-11-06 22:10 — Incident follow-up —
Status: done
Did: the user reinstalled M4 from safe mode; --info (Claude Code, read-only): `FM-1_46000500  [running]`.
The user asked to try M5 again until Monday (accepting a fallback to M4): advised 0.5.3-m5p3 with NO song sends or
deletes from M5 (the hang was in the SEND handler; M5 reads M4's stored songs and SYNTH needs no store), knowingly
against HANDOFF 21:50 "don't reinstall M5" at the user's explicit request.
Hardware touched: read-only (--info); the reinstall was the user's.
Questions for the planning chat: none (the fix stays for the next milestone).
Next: the user's trial; fix the watchdog stretches next milestone.

2026-11-09 — Step M5.1.1 (honest emulator; the crash reproduced) —
Status: done
Did:
- tools/patches/fm1-emu-flash-timing.patch (applied by tools/apply_patches.sh): FM1_FLASH_TIMED=1 (or "E,P" us):
  status WIP set 8000 us after a 4 KiB erase, 2000 us after a page program (Puya P25Q-H series typical tSE / tPP:
  the P25Q40H datasheet and the P25Q80H distributor listing; no P25Q80H datasheet table found), a busy end is an
  event (no loop skip past it); data-area XIP reads cost ~300 cycles a 64-byte line miss (the cache harbor measured
  on an FM-1; FM1_STRICT's own cache otherwise); every run prints the watchdog's longest unfed stretch. New, and the
  key: a read of the XIP window while a flash command has it off (SFC_CON 0 + pins to SPI0) stalls the core until
  the watchdog resets it (FM1_XIP_OFF=warn: only counted).
- Reproduced on 0.5.3-m5p3 with the unit's likely store (M4's 9 songs + 2 private, 68 of 69 sectors), sending ours:
  `xip off: a read of 0x02033cb8 at 1832 ms ... the core stalls`, `chip reset at 9826 ms (watchdog)`, fm1song
  `the FM-1 was disconnected` (the user's message). Timing alone (default and FM1_STRICT, the M4 store, all 14 in
  one command) did NOT reproduce it: longest gap 590 ms (boot) / 765 ms (strict), 699 flash ops, busy 1452 ms.
Learned / measured:
- ROOT CAUSE: store.c kill() wrote its zeros from `static const uint8_t zero[4]` = .rodata in the XIP flash
  (nm: kill.zero 0x02033cb8). felucca-bsp's fl_prog_ram runs from RAM with XIP off and reads src while sending
  the page: on the chip that read cannot complete -> core stalled, IRQs off -> watchdog (~8 s, "last watchdog reset
  in USB", no crash record). First hit: the first resend's delete of the song's M4 copy (store_commit kill_slug).
  M2-M4 never wrote from .rodata (deletes erased); the emulator read XIP freely while it was "off".
Hardware touched: none
Questions for the planning chat: none
Next: M5.1.2 fixes (RAM sources, compaction out of SEND, incremental table, yields, wipe).

2026-11-09 — Step M5.1.2–4 (fixes, tests, release 0.5.4-m5p4, install checklist) —
Status: done; STOPPED for review (nothing installed, nothing sent; the FM-1 was not touched)
Did:
- store.c / library.c: every flash write's source copied to RAM (store_prog; kill's zeros on the stack); compaction out
  of SEND (store_begin -> STORE_BUSY, SEND answers result 10, lib_poll does store_step ~40 ms a main-loop round with
  "MAKING ROOM step n of about m"; fm1song resends each 1 s, up to 10 min); the store's table kept up to date (no
  552-block rescans); store_yield (watchdog feed) in scan / planning / receive loops; WIPE SONG STORE: Settings last
  row, GLO+HOME at power-on (a fresh PLAY press held 2 s; held keys never count), USB 0x75 + `fm1song wipe --yes`.
- tools/m51_test.py (`make m51-test`), store_test.c (BUSY/steps, wipe, table == rescan after 254 ops).
- Archived HANDOFF notes and LOG entries before M5 to docs/archive/ (verbatim), as the 2026-11-09 note asked;
  committed .claude/agents/test-runner.md.
- Release build/step-M5.1/fm1acc-0.5.4-m5p4.fwsc, FM-1_46000504, sha256 197b84a5...0a187d, from 902eaa7.
- docs/step-M5.1-install.md: over 0.5.3-m5p3 or M4; ONE song first, then ours, then the private ones; fallback M4.
Learned / measured (release package):
- m5_test 286 PASS ALL PASS; m51_test (timed) 17 PASS ALL PASS over 116 runs: longest watchdog gap after boot 71 ms
  (limit 8000), 0 resets, 0 XIP-off reads; store_test 35 ALL PASS; pytest 93; vsynth ALL PASS; fuzz 10 min 1,118,000
  files clean. Image 451,744 B, .data+.bss 76,344, pool 289,864.
- Decisions taken while the user was away: wipe combo GLO+HOME (REC, OCT-+OCT+ are fm1-base's); wipe as the last
  settings row (m5_test's hint-off row moved to e0:9); BUSY + resend chosen over keep-alives. At the user's request
  the extra timed re-run of the M5 suite was dropped. A first m51 run on the release aborted on a test-fixture race
  (two parts writing frag.bin at once); fixed, rerun clean.
Hardware touched: none
Questions for the planning chat:
- Review docs/step-M5.1-install.md.
- Erase/program timings are Puya P25Q-H typical (P25Q40H datasheet, P25Q80H distributor listing); a hardware
  measurement (fl_erase4k's took_us) would confirm them.
Next: STOP for review; then the user runs the checklist.

2026-11-09 23:35 — Step M5.1 install (the user's report) —
Status: done
Did: the user installed 0.5.4-m5p4 (FM-1_46000504) and ran docs/step-M5.1-install.md steps 1-5: all 14 songs
sent and working. device.md "Currently installed" updated (it still said M1).
Learned / measured: the user's report only; Claude Code ran no device command this session.
Hardware touched: WRITE by the user (install, song sends), per the checklist.
Questions for the planning chat: none
Next: M6 Parts 1-4 (docs/plan-M6.md), the early install point after Part 2 skipped at the user's request.

2026-11-10 — Step M6 Parts 1-2 (learning-mode fixes, views) —
Status: done (emulator); committed 71e608a (one commit: the two parts' ui.c changes interleave); no install point
(skipped at the user's request)
Did:
- HOME held 2 s from any screen (not the WIPE question; a HOME held since power-on never counts) and Settings > Switch
  mode: the mode screen; leaving SYNTH stops the synth and gives the song buffer back; leaving LEARN stops the song.
- QUIET box: dark amber box, light amber text (was grey on brown, 1.5:1); every layer-box text >= 4.5:1 (checked).
- Melody roll: a vertical piano keyboard on the left, a key per pitch row, sounding keys lit, each C named.
- Harmony L3 loudness: imported parts were NOT skipping M5's normalisation (x0.45 Let It Be, x0.47 True Colors), but it
  was one factor for the whole song. The importer now matches every 4 bars (tools/fm1song/importer.py; private songs
  re-imported, the report gives each part's 4-bar spread before/after).
- Ukulele: setting "Chord boxes, tab" GUITAR / UKULELE (default GUITAR); firmware/app/ukulele.h (own table, 43 open +
  19 movable shapes); tools/guitar_check.py --uke. Melody tab view (guitar or ukulele, by the setting); harmony BAR
  VIEW (the roll); SELECT zooms the scrolling and staff views (0.8..6 bars; notation up to 2x); the chord boxes'
  rhythm icon under the x/o lines.
Learned / measured:
- Loudness, emulator, 25 s from bar 1, harmony alone L1..L5: Let It Be 737 735 710 725 714 (A-weighted 558 558 569 557
  550): equal. True Colors 628 688 358 630 624: its L3 is quieter in the intro. Model per 4 bars, M5 -> now: True
  Colors 0.37..1.43x -> 0.98..1.02x, Creep 0.68..1.42 -> 0.99..1.01, Zombie 0.36..1.51 -> 0.99..1.01, Rockin
  0.45..1.26 -> 0.98..1.02, Let It Be 0.96..1.04 -> 0.99..1.01.
- Most likely cause of the user's note: it came from the 0.5.3 trial, when the unit still held M4's imports (before
  M5's normalisation, up to 5x louder). Since M5.1 the songs on the unit are the normalised ones.
Decisions (the user away):
- SELECT is the tempo in the views that do not zoom; in the scrolling and staff views it zooms, and REC turns on a
  TEMPO mode (4 s, like SAVE's KEY mode). REC is free at run time (fm1-base uses it only at power-on).
- One melody tab view whose instrument follows the ukulele setting (not two views). "Zoom at start" (was "Scrolling
  views show") keeps 1.5 / 3 bars as the starting zoom; zoom is fresh per song, not saved.
- Settings rows: Switch mode (10), Chord boxes, tab (11), Wipe song store last (12).
Hardware touched: none
Questions for the planning chat:
- Ask the user whether Harmony L3 is still louder with the songs sent at M5.1 (Let It Be measured equal); resending the
  5 private songs after M6 gives the 4-bar matching.
Next: Part 3 (synth round 2).

2026-11-10 — Step M6 Part 3 (SYNTH round 2) —
Status: done (emulator); committed 667c4f7
Did:
- Reverb (ours: 4 damped combs + 2x2 all-passes, ~1.5 s, lines after the delay's in the song buffer) and bit-crush
  (16..7 bits, 1..8 samples held); nothing from Felucca's GPL files, not EAS's reverb.
- Arp: rates 1/4 1/8 1/8T 1/16 (step timing in 1/256 control ticks: triplets keep time), RANDOM pattern.
- SHIFT = EDIT held: K1 reverb, K2 bit-crush, K3 arp rate (a step per 2 clicks), K4 detune; the screen shows that set.
- Drum jam: PLAY on/off, FX style (WALTZ BALLAD SHUFFLE MARCH POP TRAIN), SCL level 1-3, the tempo shared with the
  arp (ARP held + SELECT). tools/jam_songs.py compiles a tiny 2.0 song per style (364 B each, drums only, 8 bars +
  closing bar) into firmware/app/jam_songs.h; the song player loops bars 1-8; EAS renders under the synth only while
  it plays (synth_mix_eas).
Learned / measured:
- vsynth_test ALL PASS (21 presets; rates 8/16/24/32 steps in 4 s at 120 BPM; reverb tail silent after 8.5 s; crush).
- m6p3 12 PASS; m5p3 10 PASS on the same build. CPU: 8 voices SYNC SCREAM + chorus + delay + reverb 127 + jam L3:
  synth render + EAS render 34 % of a block (emulated). Image 460,480 B.
- First reverb version had a rounding limit cycle (>> on negatives: the tail never reached silence); now / (toward 0).
Decisions: SHIFT on EDIT; jam on PLAY / FX / SCL; reverb and crush defaults per preset (PRESET_REV, PRESET_CRUSH).
Hardware touched: none
Questions for the planning chat: none
Next: Part 4 (tests, budget, install checklist), then STOP.

2026-11-10 — Step M6 Part 4 (final package, full suite, install checklist) —
Status: done (prepared; not installed)
Did:
- The full suite (test-runner subagent) on 0.6.0-m6 from 6f8ffe0 (sha256 6451c638…): store, pytest (93), vsynth, guitar/ukulele, fuzz (210,000 files), m51 all PASS; m5 FAIL (part3 "HOME while setting", m5p1 hint 11 openings), m6 FAIL (m6p1 loudness over all songs incl. private).
- The m5 failures were a real bug (mine, M6 part 1): the HOME hold's start `now | 1u` can be 1 ms ahead of now, and a second input() pass in that ms read now - t as ~4e9 ms, so a HOME tap opened the mode screen. Fixed with held_ms() (negative = 0); commit 920fa67. Added an m6p1 check (24 quick HOME/PLAY taps: never the mode screen).
- The same pattern was in M5.1's WIPE question (`wipe_t0 = now | 1u`): fixed in the same commit.
- Decided (user away): didn't re-run the whole 4 h suite for a two-line fix; re-ran the groups the fix touches on the final package: m5 part3,m5p1,boot; m6 --quick; m51 wipe: ALL PASS.
- docs/step-M6-install.md written (Known section, P4/P5, budget).
Learned / measured:
- Package build/step-M6/fm1acc-0.6.0-m6.fwsc, FM-1_46000600, sha256 8944fb02a725978306754601e053e7c1f8b96f0280e13deed23201335888abec, from 920fa67, reproducible (two builds).
- App 460,496 B; RAM .data+.bss 78,888; pool 289,864. SYNTH worst (8 voices SYNC SCREAM, chorus+delay+reverb 127, jam L3) 34 % of a block. Longest no-watchdog stretch while sending (timed emu) 67 ms.
- Open: the generated levels of private songs: rockin drums L1 2.15x quieter than L2/L3, harmony L1 (held block chords, distortion gtr) 2.06x louder than L3–L5; zombie drums 1.68x. Public songs, Let It Be and True Colors within 1.6x. M6 didn't change gen.c; M5 never measured private songs. The m6p1 check is left failing on purpose (not relaxed).
Hardware touched: none
Questions for the planning chat:
- M5.1 (on the unit now) has the WIPE-question timer bug: a PLAY tap there could wipe the song store without the 2 s hold. Until 0.6.0-m6 is installed the user shouldn't open Settings > Wipe song store or GLO+HOME at power-on. Please confirm the install soon.
- Loudness of generated L1 for sustaining/sparse cases (rockin harmony/drums): fix in gen.c's level matching (M7?), or accept?
- Please review docs/step-M6-install.md.
Next: the user runs docs/step-M6-install.md steps 1–6; Claude Code runs only --info / fm1song list and records.

2026-11-10 — Step M6 install (the user's report) —
Status: done
Did: the user installed 0.6.0-m6 (FM-1_46000600) and ran docs/step-M6-install.md steps 1-6, all fine, including the
optional step 5 (the 5 private songs resent with the 4-bar loudness matching). device.md "Currently installed" updated.
Learned / measured: the user's report only; Claude Code ran no device command this session.
Hardware touched: WRITE by the user (install, private song resends), per the checklist.
Questions for the planning chat: none
Next: M7 Parts 0-6 (docs/plan-M7.md).

2026-11-10 — Step M7 Part 0 (measure first; no behaviour change) —
Status: done
Did: built 0.6.0-m6 from HEAD (dev build) and measured; checked the flash area 0xDE000..0xDFFFF.
Learned / measured:
- App image 460,480 B; slot 0x4000..0x92FFF = 585,728 B -> 125,248 B free. RAM .data+.bss 78,888 of 98,304; pool 289,864.
- vsynth.o (engine host, effects, arp, 21 presets): 9,720 B code + 4,393 rodata + 48 data = 14,161 B in the image
  (+1,790 B bss). synth.c: 798 B code (3,127 bss). EAS host (eas/eas_host_fw.c): 1,006 B code (41,236 B bss).
  jam_songs.h: 6 x 364 B + a 72 B table = 2,256 B. synthui.c (unity build, inlined into input/app_frame): measured by
  building with synth_input / draw_synth / synth_hash stubbed: 460,480 - 454,016 = 6,464 B (incl. what only they reach).
  Part 2 is measured the same way.
- SYNTH CPU worst case (step-M6-install.md, m6p3-cpu on the 0.6.0-m6 package, the same code): 8 voices SYNC SCREAM,
  chorus + delay + reverb 127 + jam L3: synth + EAS render 34 % of a block (emulated).
- 0xDE000..0xDFFFF: unused. Code: the song store ends at STORE_HI 0xDC000 and refuses anything outside it; OBJ_PANEL
  (fm1-base, APP_STORE_MAP 0xDC000) takes the A/B pair 0xDC000 + 0xDD000; OBJ_SETTINGS 0xFC000/0xFD000; no
  APP_STORE_POOL; nothing in firmware/app or fm1-base device/ names 0xDE000 (only fm1-base's examples/template).
  PLAT_DATA_HI is 0xE0000, so it is inside the data area. Dumps: the unit's backup has 0xDC000..0xDFFFF all 0xFF;
  1,207 emulator flash images under build/emu (M2..M6 and M5.1's timed runs): none has a byte written there.
  Not used in M7.
Hardware touched: none
Questions for the planning chat: none
Next: Part 1 (learning mode).

2026-11-10 — Step M7 Part 1 (learning mode) —
Status: done (emulator); committed
Did:
- Rockin' removed: songs/private/ is git-ignored (never committed), so `git rm` had nothing to remove: deleted the local
  folder songs/private/rockin and its line in songs/private/import_all.sh; import/private/Rockin_in_the_Free_World.mid
  kept. The loudness checks glob songs/private/*: its rows are gone. The unit: `tools/fm1song.sh delete
  rockin-free-world` (the user, in the checklist).
- Format 2.1 (additive): song.yaml layers.<layer>.start_level (1..levels, style songs only); stored in LAYR's flags byte
  r[5] (2..levels, 0 = L1); header 2.1 only when a layer has one, else 2.0 byte for byte (golden tests unchanged).
  Parser (song.c), the song's open (library.c / ui.c open_song: each layer built and played at its start level),
  Python reader + writer, `compile --format 2.1/2.0`, song_fuzz.sh (a 2.1 seed), pytest (3 new tests, the 2.1 seeds in
  the C-vs-Python agreement test), docs/song-format.md 2.6. The song list's END message gets byte 12 = 1 ("reads
  2.1"); `fm1song send` sends 2.0 (no start level) to an FM-1 that does not say so (M5/M6), so no refusal.
- Zombie: bass start_level 2; melody volume 88 (was 100). import_all.sh re-applies both after a re-import.
- The NOT IMPLEMENTED pop-up (D044) in LEARN: list (free: FX SCL ENV LFO SAVE ARP SEQ REC; HOME on the folder list,
  on release), settings and views screens (the same 8), play screen (only the layer buttons of layers the song lacks),
  help (none: any button goes back). Never: splash, mode screen, hint, WIPE, a button held since power-on. Any button
  closes it (that press and its release do nothing else); it also closes itself after 2.5 s (decided: the screen under
  it is frozen while it is open); encoder turns are dropped while it is open; HOME held 2 s works; keys play.
Learned / measured:
- Zombie's melody plays only in bars 28-32, 92-96 and 168-200 (the lead): measured from bar 168 (PRESETS to Part 22),
  20 s, each alone, level 1. Melody / (harmony + bass) before: rms 1.05, A-weighted 2.80; the 9 public songs (first
  21 s): rms 0.56..2.52 (median 1.23), A-weighted 1.12..4.16 (median 2.90). So by the numbers it already sat at the
  public median. Decided (the user hears it too loud; an overdrive lead is louder than its rms): volume 88 -> after
  (with bass L2): rms 0.90, A-weighted 2.40, the public songs' lower quartile.
- pytest 96 passed; fuzz 20 s 62,000 files clean (the full run: Part 6); m7p1 11 PASS. Image 461,216 B.
Hardware touched: none
Questions for the planning chat:
- Zombie's lead measured at the public median before any change; I lowered it to the lower quartile (volume 88) on
  the user's ear. Please ask the user after the install whether it is right now.
Next: Part 2 (the SYNTH UI on the parameter table).

2026-11-10 — Step M7 Part 2 (the SYNTH UI on the parameter table) —
Status: done (emulator); committed
Did:
- vsynth.h/.c: one parameter table (VS_PARAMS: name, page, range, display) for 36 settings on 9 pages + ARP on; the
  preset record (u8 version, u8 engine, name[12], 37 values, padded to 64 B; vsynth_record reads one back, an older
  version's new values from the defaults, out-of-range values to the default); stock presets are const records, the
  sound plays from a RAM copy (vsynth_load / vsynth_set). M6's 21 presets converted value for value (chorus depth 64,
  delay 1/8D feedback 64, reverb size / damping 64 = M6's fixed values). Arp: OCTAVES 1-3 and GATE (new).
- synthui.c rewritten, table-driven: main screen (preset big, tempo, drum pattern, instruction row; the quick knob's
  value in the bottom row 1.2 s), 9 edit pages (2 x 2 cells: name, value, bar), drum pattern menu, SYNTH help, SYNTH
  settings (drum volume 0..10, drum kit; in OBJ_SETTINGS: the record grows 36 -> 40 B, an M5/M6 record reads the
  defaults). The control map as approved; the SHIFT layer gone.
- M6's drum jam removed already here (library.c lib_jam*, jam_songs.h, tools/jam_songs.py): the new UI drives synth.c's
  drum API (stubs in this commit; the sequencer is Part 4). docs/drum-patterns.txt + tools/jam_patterns.py ->
  jam_patterns.h (25 patterns) added now (the menu lists them).
Decisions (the user away):
- ANALOG's own CUT and RES are the FILTER page's; its SOUND B shows our LFO's rate and PWM depth in their place (the
  same knob in two places would confuse). PHASE (Part 5) gets all eight of its own.
- K1..K4 cells as 2 x 2 (K1 K2 / K3 K4). Values 0..127 move 2 a click; short lists (<= 16 values) one per 2 clicks.
- HOME (a tap) on the main screen is its job (stay on it), so no pop-up there. The pattern menu: PLAY chooses and
  starts the drums, SEQ only chooses. SELECT is the tempo on the pages too; PRESET changes the preset everywhere.
- Renamed to fit 12 characters: MIDNIGHT STRINGS -> DUSK STRINGS, OCTAVE BOUNCE -> OCT BOUNCE, SOFT SAW STRINGS ->
  SAW STRINGS, NIGHT DRIVE SUB -> NIGHT DRIVE.
Learned / measured:
- The synth UI measured as in Part 0 (synth_input, draw_synth, synth_hash stubbed): 8,320 B (M6: 6,464 B). The aim
  "not bigger" is missed by 1,856 B: it now has five screens (main, pages, pattern menu, help, settings) instead of one.
- Image 465,952 B. m7p2 19 PASS (main, preset, tempo, quick knob, EDIT through the 9 pages, ALGO, ENV, FX, HOME, menu,
  PLAY, help, settings saved over a power-off, ARP, the 4 pop-up buttons, keys).
Hardware touched: none
Questions for the planning chat: none
Next: Part 3 (effects).

2026-11-10 — Step M7 Part 3 (effects) —
Status: done (emulator); committed
Did:
- New, ours: fuzz (2x..64x into dsp.c's tanh curve, a one-pole tone filter, its level following the dry sound's
  envelope x 1.2 so FUZZ changes the colour, not the loudness), tremolo (sine 0.1..12 Hz, ramped per control block),
  phaser (6 first-order all-passes, no buffer, swept 300 Hz .. +2.5 kHz by a 0.05..5 Hz triangle). Kept: chorus (now
  DEPTH and MIX), delay (TIME as note values 1/16..1/2 at the tempo, halved until it fits; FEEDBACK), reverb (SIZE,
  DAMPING, MIX), bit-crush. Chain: voices -> fuzz -> crush -> tremolo -> phaser -> chorus -> delay -> reverb -> LEVEL.
  Nothing from Felucca's fx.c or EAS's reverb.
- Each effect at 0 is skipped; the delay and reverb only once their lines have gone silent (else a later mix would
  play stale sound). The delay's feedback rounding changed to toward-zero: M6's arithmetic shifts left a +-2 limit
  cycle in the lines, so they never went silent.
- tools/vsynth_test.c on the new API: every preset (held, released, its arpeggio), arp rates / RANDOM / OCTAVES, each
  effect alone and all together (differs from the dry waveform by >= 0.2 of it, ran, no clipping, silent after the
  release), nothing running with every mix at 0.
Learned / measured:
- vsynth_test ALL PASS (difference from dry: fuzz 127 0.55, crush 0.32, tremolo 0.62, phaser 0.78, chorus 1.03, delay
  0.76, reverb 2.52, all 1.24; every tail ends at peak <= 1,024 of 8,388,607).
- Emulator: every effect at its top + 8 voices of SYNC SCREAM: synth render 32 % of a block. m7p3 3 PASS; m7p2 rerun
  ALL PASS. Image 466,992 B.
- The build's ROM-address check refused one build (a clamp after / 128 compiled to the constant 0xFFC00001); the line
  needs no clamp (bounded). That failed build left the previous package in place and one test run used it: from now
  on I check the "package" line, not only "image".
Hardware touched: none
Questions for the planning chat: none
Next: Part 4 (drums).

2026-11-11 — Step M7 Part 4 (drums) —
Status: done (emulator); committed
Did:
- docs/drum-patterns.txt (25 patterns, ours, from genre conventions; x accent, o normal, g ghost, . rest; 8 instruments)
  -> tools/jam_patterns.py -> firmware/app/jam_patterns.h (4 + 12 + up to 64 B a pattern; 1 or 2 bars; 12 or 16 steps).
  ROCK1 first and default at every SYNTH start.
- synth.c: the step sequencer in the audio interrupt, before anything renders: steps counted in 1/256 control ticks at
  the shared tempo; ACOUSTIC sends GM notes on channel 10 to EAS (CC7 = drum volume; note-offs at the next step) and
  EAS is mixed under the synth only then (and 2 s after, for the tails); 808 / CHIP hits go to vsynth's own drum voices
  at their control tick. A pattern chosen while playing starts at the next bar.
- vsynth.c: 6 drum voices of their own. 808: our sine-sweep kick and toms (150->48 Hz, 140->95, 220->150), NOISE for
  snare (+ a 200 Hz body), hats (band-pass high; closed chokes open), clap (3 bursts 12 ms apart), rim (sine click +
  noise). CHIP: LOFI (triangle with its pitch sweep for kick and toms, its noise wave, a pulse rim). After the
  preset's LEVEL, not through the synth's effects.
- Vendored eng_noise.c and eng_lofi.c (MIT, unchanged, the same commit d026007); VENDORED.md updated (eng_phase.c is
  listed already: Part 5).
Learned / measured:
- m7p4 8 PASS: each kit 26 hits in 4 s of ROCK1 at 120 BPM (13 a bar); at 200 BPM 44 (43 wanted); a mid-bar change
  plays to the bar's end; PLAY off silent; volume 0 silent, 10 louder than 7; 8 synth voices + 4 drum voices.
  Loudness (ROCK1, volume 7, rms): ACOUSTIC 1,238, 808 1,538, CHIP 1,277 (CHIP got x3.5, NOISE x2: they sat
  4x / 15 dB low). vsynth_test: every 808 and CHIP sound alone and all eight together sound and end (ALL PASS).
- Image 474,800 B (+7.8 KB: NOISE, LOFI, the kits, the sequencer).
Hardware touched: none
Questions for the planning chat: none
Next: Part 5 (PHASE, the 30 presets).

2026-11-11 — Step M7 Part 5 (engines, the 30 presets) —
Status: done (emulator); committed
Did:
- Vendored eng_phase.c (MIT, unchanged, the same commit). A preset chooses ANALOG or PHASE; SOUND A / B show the
  engine's own parameters from its descriptors (PHASE: WAVE WAVE2 DCW ENV | DTN LINE SUB FOLD). PHASE has no filter:
  the FILTER page drives our SVF after it (ANALOG's LP/BP/HP taps, per voice; skipped when wide open).
- 30 stock presets: M6's 21 minus DEEP SUB and ANALOG BELL, plus E-PIANO (PHASE double-sine, DCW from the envelope,
  tremolo, chorus), E-GUITAR (ANALOG saw pluck into the fuzz), E-SITAR (PHASE resonant saw, bright, short, a little
  chorus), POTATO (ANALOG sub-osc, low cutoff, driven, a slow tremolo and phaser: warm and lumpy), PARKER (ANALOG PWM
  with LFO, a little noise, filter envelope: breathy), EDS (square, bit-crushed 60, its own up-down 1/16 arp over 2
  octaves), and mine: OUTRUN BASS (ANALOG saw bass, resonant envelope), MIAMI PAD (PHASE saw + saw-pulse, slow,
  chorus, phaser, reverb), CHROME ARP (PHASE pulse arp 1/16 over 2 octaves, delay), NEON BELLS (PHASE resonant
  triangle, decaying, reverb; takes ANALOG BELL's place), VHS STRINGS (ANALOG detuned saws, deep chorus, a little
  tremolo and crush). The user may rename or re-voice POTATO / PARKER / EDS.
- LEVEL made logarithmic (100 = as made, half a dB a step: 127 = x4.76, 0 = -50 dB): the linear 0..1.27 could not lift
  the arpeggio presets (one note at a time) to the chords'. Each preset's LEVEL set from its measured loudness
  (tools/vsynth_test.c LEVELS=1: a C E G B chord, or its arpeggio, the loudest 100 ms; the median as the target).
- m5p3 (M5's SYNTH checks) ported to M7's controls: presets by PRESET, 30 presets, arp pattern/rate on the ARP page.
  m6p3 RETIRED (not run; header note): it tested the SHIFT layer and the jam, both replaced by D039 / D041; its checks
  live on in m7p2-m7p4, m7p6 and vsynth_test.
Learned / measured:
- Laptop: the 30 presets' loudest 100 ms 928k..987k (+-0.3 dB of the median) after matching (before: 0.20x..6.4x;
  MIAMI PAD had a deep reverb: its size 80 -> 64, mix 70 -> 55). vsynth_test ALL PASS (30 presets, effects, drums).
- Emulator: m7p5 6 PASS (every 3rd preset sounds, the loudest 100 ms within 1.42x; 30 presets; E-PIANO is PHASE and
  its SOUND A page differs; a PHASE chord); m5p3 10 PASS on this build. 8 voices SYNC SCREAM + chorus + delay: 30 %.
- Image 479,920 B: 105,808 B free in the slot (the M7.1 reserve of 32 KB is there).
Hardware touched: none
Questions for the planning chat: none
Next: Part 6 (census, budget, the full suite once, the install checklist).

2026-11-11 — Step M7 Part 6 (tests, budget, the full suite once, install checklist) —
Status: done (prepared; not installed). STOPPED for review.
Did:
- m7p6: the button census (13 screens x 14 buttons, both modes: each its job or the pop-up; only the mode screen's
  others do nothing, D044), the power-on rescues with M7 (OCT-+OCT+, REC: fm1-base's; GLO+HOME: WIPE, no pop-up),
  24 quick HOME/PLAY taps in SYNTH never the mode screen, leaving SYNTH with the drums on. The census found two gaps,
  fixed: ENV pressed on the ENVELOPE page did nothing (now: the main screen; FX on an FX page: the next one) and the
  HOME press that opens the folder list opened the pop-up on its release (now only a press begun on that screen).
  HOME tapped on the SYNTH main screen shows a hint. m7cpu: the budget.
- Version 0.7.0-m7; docs/controls.md "M7" (control map, census table, the 25 pattern grids, presets, 19 screenshots
  from the release package in docs/step-M7/); docs/step-M7-install.md.
- The full suite once on the release package (test-runner subagent), then 3 stale expectations fixed and only their
  groups re-run on the same package (see below).
Learned / measured:
- Package build/step-M7/fm1acc-0.7.0-m7.fwsc, FM-1_46000700, sha256 8478a955eb29c1de7474274a94ff6d96e4f7e2b0bfb8d98580012764bbfdb512,
  from d1140ba, reproducible (two builds). App 479,968 B (105,760 B free in the slot); RAM .data+.bss 81,128; pool
  289,864. fm1-base unchanged since 98fc08c.
- CPU worst (8 voices of the heaviest preset, MIAMI PAD, every effect at its top, SYNTHWAVE2): ACOUSTIC 35 %, 808 44 %,
  CHIP 42 % of a block (limit 60 %): nothing dropped. 8 voices as made: 9..34 %.
- Suite: store ALL PASS; pytest 94/1; vsynth ALL PASS; guitar+uke ok; fuzz 600 s 2,452,000 files clean; m7 52/1;
  m6 21/1; m51 16/1; m5 283/1. Longest stretch without a watchdog feed 92 ms, 0 resets, 0 XIP-off reads.
- Failures: (1) pytest golden test and (2) m5 m4p5 asserted every private song is 2.0 (Zombie is 2.1 by design):
  fixed to the exact expectation (Zombie 2.1, its generated levels still compared), re-run: pytest 95 passed, m4p5 9
  PASS. (3) m51 sends: the M4 incident store keeps Rockin' (never deleted in that scenario): 14 songs all ok vs 13
  expected; the check now expects the repo's songs plus the fixture's own; re-run: sends ALL PASS. (4) m6p1 Zombie
  drums L1 1.68x: known, the plan accepts it (backlog); left failing. (5) m7p5 preset loudness in the emulator 1.88x
  (my fast checks had sampled every 3rd preset); holding the chord 2.1 s as the laptop matching does: 1.78x, DUSK
  STRINGS +2.3 dB, GLASS PLUCK -2.7 dB (the laptop: all within 0.3 dB). Left failing, not relaxed, not re-levelled
  (it would change the package after its one full run). In the checklist's Known.
- Laptop tools: `fm1song list` now says "2.0..2.1" for an M7 FM-1.
Hardware touched: none
Questions for the planning chat:
- Review docs/step-M7-install.md (install over M6; then the user deletes Rockin' and resends Zombie 2.1).
- Preset levels: re-level DUSK STRINGS / GLASS PLUCK by the emulator's measure in M7.1, or by the user's ear?
- The synth UI is 1,856 B bigger than M6's (5 screens instead of 1): acceptable?
Next: STOP. The user runs docs/step-M7-install.md; Claude Code runs only --info / fm1song list and records.

2026-11-12 — Step M7 install (reported by the user) —
Status: done
Did:
- The user installed 0.7.0-m7 (FM-1_46000700, package sha256 8478a955…bfdb512) and ran docs/step-M7-install.md:
  all fine. Rockin' deleted on the unit, Zombie resent (2.1); Zombie's quieter lead sounds right.
- docs/device.md "Currently installed" updated (M7; fallback M6, then M5.1).
Learned / measured:
- The user's report only; Claude Code recorded no device outputs.
Hardware touched: WRITE (install, delete, send; by the user)
Questions for the planning chat: none
Next: Milestone M8 (docs/plan-M8.md), Part 0.

2026-11-12 — Step M8 Part 0 (measure first) —
Status: done
Did:
- tools/m8_test.py m8p0 (on the M7 sources, package 00d850e3…, no behaviour change): the song buffer per song, LEARN
  with every layer at its top level for 30 s (13 songs, the private ones too), SYNTH's MIAMI PAD x 4 voices.
Learned / measured:
- App 479,952 B: 105,776 B free in the slot (585,728). RAM .data+.bss 81,128 of 98,304; pool 289,864 of 344,064.
- Song buffer (196,608 B) in LEARN: largest Zombie 141,616 B; then Creep 64,752; ours at most 28,128 (Oh! Susanna).
- LEARN, EAS alone, every layer at its top: busiest Zombie / Creep 18 % of a block (17 / 13 voices); ours 12-15 %.
- SYNTH, MIAMI PAD x 4 voices: as made 22 %, without delay/reverb 17 %, delay + reverb at their top 22 %, every effect
  at its top 23 %. Sum with the busiest song: 42 % (limit 60 %).
- Decision (D049): BACKTRACK's melody sound = the 30 SYNTH presets, 4 voices, mixed over EAS. The song buffer is split:
  the backtrack's levels first (up to 112,896 B), the synth's effect lines after them with the delay's maximum halved
  in BACKTRACK (16,384 frames = 0.37 s; 149,248 -> 83,712 B; a longer delay time is halved until it fits, as in M7).
  tools/backtracks.py checks every backtrack in every style fits that room. GM fallback not needed.
Hardware touched: none
Questions for the planning chat: none
Next: Part 1 (renames).

2026-11-12 — Step M8 Part 1 (renames, D050) —
Status: done (emulator); committed
Did:
- Mode screen: SONG LEARN, BACKTRACK, SYNTH (a row each under the mascot); the cursor on SONG LEARN at power-on and
  whenever the screen opens (HOME held 2 s, Switch mode); SELECT steps and stops at the ends. (BACKTRACK: Part 4.)
- APP_NAME "Notami - SongSidekick" (splash in FONT_B: it fits; the help line; words.tsv), the splash's second line
  "m8"; FW_VERSION 0.8.0-m8. USB / MIDI names, identity, package name unchanged.
- Presets: E-PIANO, E-GUITAR, E-SITAR are 1-3 (SYNTH starts on E-PIANO). Tests now name presets by name
  (m7_test PI / OLD keeps M7's samples); m5p3 / vsynth_test indices moved; SYNTH = e0:2 on the mode screen.
- Titles: "12-Bar Blues", "Four Chord Tune" (make_songs.py; slugs unchanged).
Learned / measured:
- Fast checks on that build: m5p3 ALL PASS; m6p1 only the known Zombie drums 1.68x; m7p2, m7p5, m7p6 ALL PASS;
  vsynth_test ALL PASS; pytest 95.
- (Decided: leaving a mode puts the cursor on SONG LEARN, not on "the other one" as M7 did with two choices.)
Hardware touched: none
Questions for the planning chat: none
Next: Part 2.

2026-11-12 — Step M8 Part 2 (format 2.2, energy, five styles; D047, D048) —
Status: done (laptop: C = Python); committed
Did:
- Format 2.2 (docs/song-format.md 2.7), additive: header 2.2; STYL 6..10 = rock, indie, bossa, funk, reggae; FORM
  (required in 2.2): kind (song / backtrack), a backtrack's number, scale (major / minor / blues) and numerals, per
  section role (intro verse chorus bridge ending other) and energy 0..2. 2.2 is written only for a backtrack, a role /
  energy, or a new style: every existing song stays 2.0 / 2.1 byte for byte. Compiler, reader, C parser, fuzzer.
- Energy, the clean way (no fallback needed): gen.c's note filter keeps, per bar, the level L + the bar's section
  energy (capped at the top level); a note belongs to the bar it starts in. So the higher bars are generated when the
  level is built (no rebuild, no gap); the buffer need counts them. Only for layers with no stored level (D: an NT
  level is a bar pattern of its own level; documented).
- Five styles, table-driven in gen.c and arrange.py (16th grid, repeated / cut for other metres, 2-bar tables by bar
  parity): harmony L2/L3 rhythm (+ "upper" voicing for funk / reggae), bass L4 line, drums L2 groove from ROCK1, POP3,
  BOSSA, FUNK1, REGGAE (x 92, o 70, g 34); L1 the pulse, L3 + crashes / fills as every style.
- song_restyle (for BACKTRACK, Part 4): any of the 11 styles and a metre (WALTZ 3/4), chord ticks scaled bar for bar.
- fm1song send refuses 2.2 to an FM-1 before M8 ("install M8 first"; there is no older form); LIST END byte 12 = 2.
Learned / measured:
- pytest 123 passed: the 13 songs' golden events unchanged; each new style (4/4, 3/4, 5/4, 7/4, swing, two chords a
  bar, energy 1 and 2) C = Python at every level; energy changes the chorus/bridge bars to the next level(s) and no
  other bar; the FM-1's restyle of a POP backtrack = the chart compiled in each of the 11 styles (WALTZ in 3/4).
- song_fuzz 60 s clean (with 2.2 seeds and a random restyle of every accepted file).
Hardware touched: none
Questions for the planning chat:
- Energy applies only to layers with no stored level (a hand-made / imported level is a different part): OK?
Next: Part 3.

2026-11-12 — Step M8 Part 3 (the 30 backtracks; D046, D047) —
Status: done; committed
Did:
- tools/backtracks.tsv (the user's 30, one table) -> tools/backtracks.py -> songs/backtracks/btNN-<name>/song.yaml +
  song.mid (a conductor track only). C major / A minor, kind backtrack, POP, category Backtracks; the full form with
  repeats stored once (a chart part per progression): INTRO (verse's last 4), VERSE 1 (x2), CHORUS 1 (x2, +1),
  VERSE 2, CHORUS 2 (+1), BRIDGE (the rule; last bar V), CHORUS 3 (+1), ENDING (I / i). Blues: INTRO, 3 rounds
  (0, +1, +1; roles verse, chorus, chorus), ENDING. A progression under 4 bars is doubled first (DORIAN, TWO CHORD).
- Names for the unnamed: 10 FOLK ROCK, 12 CLASSIC ROCK, 16 MINOR ROCK, 18 MINOR SOUL (drafts, <= 15 chars).
Learned / measured:
- 30 backtracks: 39,036 B, 92 blocks; 37 bars (4-bar progressions), 65 (8-bar), 41 (blues); 1,084..1,904 B each.
- The song store: the user's 13 songs 172 blocks + 92 = 264 of 552 (288 free).
- Every backtrack in every style (WALTZ in 3/4) needs at most 54,032 B (bt28 COUNTRY 8 in FUNK) of BACKTRACK's 112,896.
- Golden test: all 30 C = Python.
Hardware touched: none
Questions for the planning chat: none
Next: Parts 4-6 (BACKTRACK mode).

2026-11-13 — Step M8 Parts 4-6 (BACKTRACK: setup and play, the scale views, the melody sound; D046, D048, D049) —
Status: done (emulator); committed together (one module, bt.c / btview.c, and the same ui.c hooks; tested part by part)
Did:
- Part 4: bt.c. SETUP (K1 progression + numerals, K2 style (11), K3 structure, K4 key (+ the verse's chords in it),
  SELECT tempo (the style's own until turned), PLAY starts, GLO help, HOME held: modes); defaults 1 POP 1 / POP /
  LOOP / C / 100; kept until power-off. PLAY: lib_open + lib_restyle (WALTZ 3/4) + SONG LEARN's play screen
  (open_song split into open_play), the key by player_transpose, LOOP = verse 1, VERSE/CHORUS = verse 1 + chorus 1 (as
  an A-B loop: SEQ works as in SONG LEARN), FULL = no loop. HOME stopped -> SETUP. SONG LEARN's list skips
  backtracks; sends refused in BACKTRACK (the synth holds part of the buffer). Help screens for SETUP and its play.
- Part 6 (Part 0's choice): the song buffer split (lib_room 112,896 B; vsynth_start on the rest, VS_MEM_BT: the
  delay's maximum halved), vsynth_voices(4), synth_band(1): EAS + the player under the synth every block. K4 on the
  play screen: the melody sound (30 presets, arp off), its name shown; the keys play it.
- Part 5: btview.c. Melody (nothing else your part; LFO held brings it back): SCALE KEYBOARD (27 keys) and SCALE
  FRETBOARD (guitar / ukulele, frets 0-12), EDIT between them; scale notes dark amber, chord tones bright, the next
  chord's a ring in its last beat; key lights the same (bright / mid / dim). Settings > Scale guide (row 13; Wipe
  stays last): PENTATONIC / FULL SCALE / CHORD TONES.
- player_stats: 18 M8 words (tools/m8_test.py); tools/m8_test.py m8p1, m8p4, m8p5, m8p6, m8census, m8cpu, m8sweep.
Learned / measured:
- A first build crashed on SETUP (names() read ui.s before any song was open): names_of(); found in the emulator.
- (The emulator boots the app already in the flash image: a test flash must be made with the build under test.)
- On the dev build: m8p1, m8p4, m8p5, m8p6, m8census ALL PASS; m8cpu: worst 37 % of a block (INDIE POP, FULL, every
  layer at its top, MIAMI PAD x 4; EAS 15 %).
- Image 493,152 B (+13 KB); RAM .data+.bss 81,416; pool 291,464 (+1,600: the list's backtrack fields).
Hardware touched: none
Questions for the planning chat:
- Mode screen cursor after leaving a mode: always SONG LEARN (decided). The melody sound's arp is switched off
  (EDS, OCT BOUNCE... play notes, not arpeggios): OK?
Next: Part 7 (release build, the full suite once, step-M8-install.md).

2026-11-13 — Step M8 Part 7 (tests, budget, the full suite once, install checklist) —
Status: done (prepared; not installed). STOPPED for review.
Did:
- Before the release build, on the dev build: m8sweep (11 styles x LOOP, VERSE/CHORUS, FULL x backtracks 1, 9, 20 at
  250 BPM: 99 runs) ALL PASS, worst 26 % of a block.
- Release build: build/step-M8/fm1acc-0.8.0-m8.fwsc, FM-1_46000800, sha256
  19e054002de21a3a204611b553fad6997a944db2f3fa83c08da651725e91665f, from 13c759e, reproducible (two builds).
  fm1-base unchanged since 98fc08c.
- The full suite once on it (test-runner subagent; build/step-M8/times.txt, *.log, emu-*/report.md): store ALL PASS;
  fuzz 600 s 822,000 files clean; pytest 153 passed; vsynth ALL PASS; guitar ok; m8 42/0; m7 53/0; m6 21/1; m51 17/0
  (`make m51-test`'s test on this package; longest stretch without a watchdog feed 92 ms, 0 resets, 0 XIP-off reads);
  m5 283/1.
- Failures: (1) m6p1 Zombie drums L1 1.68x: known, left failing. (2) m5p3 "a song sent in SYNTH mode" expected the
  laptop's text "SYNTH mode"; it now says "SYNTH or BACKTRACK mode": fixed to the exact new text, m5p3 re-run on this
  package: ALL PASS (build/step-M8/m5p3-rerun.log). Firmware unchanged after the build.
- docs/controls.md "M8" (controls, census, 18 screenshots from the release run in docs/step-M8/);
  docs/step-M8-install.md.
Learned / measured:
- App 493,312 B: 92,416 B free (M7.1's 32 KB there). RAM .data+.bss 81,416; pool 291,464.
- BACKTRACK CPU worst 37 % (INDIE POP, FULL, every layer at its top, MIAMI PAD x 4 voices; EAS 15 %); limit 60 %.
- Song store with 13 songs + 30 backtracks: 264 of 552 blocks.
Hardware touched: none
Questions for the planning chat:
- Review docs/step-M8-install.md (install over M7; then the user sends songs/backtracks/*/ and resends twelve-bar-blues
  and four-chords, in SONG LEARN).
- The styles / progressions / names are unheard drafts: the user's ear decides (one table each).
- Energy only for generated layers; the melody sound's arp off; the mode cursor always back on SONG LEARN: OK?
Next: STOP. The user runs docs/step-M8-install.md; Claude Code runs only --info / fm1song list and records.

2026-11-13 — Step M8 install (reported by the user) —
Status: done
Did:
- The user installed 0.8.0-m8 (FM-1_46000800, package sha256 19e05400…1665f) and ran docs/step-M8-install.md: all
  fine. The 30 backtracks sent; 12-Bar Blues and Four Chord Tune resent.
- docs/device.md "Currently installed" updated (M8; fallback M7, M6, M5.1).
Learned / measured:
- The user's report only; Claude Code recorded no device outputs.
Hardware touched: WRITE (install, sends; by the user)
Questions for the planning chat: none
Next: Milestone M9 (docs/plan-M9.md), Part 0.

2026-11-13 — Step M9 Part 0 (measure first) —
Status: done
Did:
- Measured with a TEMPORARY build (tools/patches/m9-part0-measure.patch: bt.c, player.h; reverted, the firmware is
  unchanged): a style change's work done from the main loop while the player plays on (song_open + song_restyle + the
  three layers' level 1 built, into the song room's second half), for backtracks 1, 5, 9, 20, 28 in all 11 styles.
- Free app space 92,432 B (slot 585,728 - dev build 493,296); RAM .data+.bss 81,416 B of 98,304; pool 291,464 B.
Learned / measured:
- The largest build need: 54,032 B (bt28, FUNK; COUNTRY 49,168; ROCK 46,528; POP 42,432). Two copies: 108,064 B <=
  112,896 B (BACKTRACK's song room): they fit, each in half (56,448 B).
- bt28 FUNK: song_open 11 ms, song_restyle 12 ms, build of 3 layers 38 ms = 61 ms (10.5 blocks); rebuilding 3 levels
  62 ms (FUNK, COUNTRY), 23-50 ms for the other styles. 37-bar POP 1: 7 + 6 + 15 ms, rebuild 27 ms.
- The audio did not miss a block while it ran (168 blocks made in 972 ms, 167 expected; render_max 17,841 of 139,296).
- Decision M9-01: (A) SEAMLESS (docs/decisions-M9.md). To re-check after Part 3 (the tier tables change the needs).
Hardware touched: none
Questions for the planning chat: none
Next: Part 1 (count-in bugs, failing check first).

2026-11-13 — Step M9 Part 1 (the count-in bugs, both modes) —
Status: done
Did:
- The failing check first (tools/m9_test.py m9p1, on the M8 player + a counter of count-ins, player_stats.countins): (a)
  BACKTRACK POP 3, LOOP and FULL, K4 and K1-K3 turned while playing: PASSED already (count-ins 1; K4 is not a second
  path: it only loads a melody sound); (b) BACKTRACK LOOP, pause in bar 3, PLAY, 3 wraps: FAILED, count-ins 5, clicks 29
  (expected 2); (c) SONG LEARN, LOOP SONG with the start point at bar 4: FAILED, count-ins 2 (one at the wrap); BARS A-B
  pause / resume, 3 wraps: FAILED, count-ins 8, clicks 50; (d) count-in OFF: FAILED, clicks after the first start (d-off-pause
  4 count-ins / 28 clicks; d-off-song 1 / 9, the wrap's).
- The fix (player.c): a flag `counting`, set by start() when a count-in is scheduled (from < s0), cleared when the position
  reaches music_from; the click test and the state test (PL_COUNT_IN) both need it. A loop jump back below music_from (the
  loop starts before the resume point) no longer counts in.
- m9p1 ALL PASS: (a) count-ins 1; (b) 2 (the start's and the resume's); (c) 1, and 2 with 6 wraps; (d) count-in OFF: 0 clicks
  at wraps (BACKTRACK 1 count-in, 1 click: the first start's, stopped at once).
- Fast checks on that build: m8p4 + m8p6 ALL PASS; m5p3 ALL PASS; m6p1 only the known Zombie failure (drums 1.68x).
Learned / measured:
- The user's "K4 restarts the count-in from 20-something" was the loop wrap after a pause / resume or a later start point
  (the wrap went back below music_from); K4 itself cannot do it. Decision M9-02.
Hardware touched: none
Questions for the planning chat: none
Next: Part 2.
