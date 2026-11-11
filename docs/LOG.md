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
