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
