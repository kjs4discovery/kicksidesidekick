# Milestone M10.4: instructions. The built-in starter library, two fixes to M10.3, one trimmed test run for M10.3 + M10.4

From the planning chat, 2026-11-15. Scope: vault decisions D070 (option 1) and D072 (revised). This answers
decisions M10.3-01: the planning chat's revised plan-M10.3.md (with the library and a test run) never reached this disk.
The file you followed was the first version. The user wanted the library and a trimmed test run in the same build,
and the knock-knock comment out. So M10.4 adds the library and fixes M10.3. Its test run covers M10.3 and M10.4
together (M10.3 was never run in the emulator).

State at the start: 0.10.4-m10.3 (FM-1_46001004) is built and **not installed**; the unit runs 0.10.3-m10.2.
The user installs M10.4 directly over M10.2 (M10.3's package is never installed).

Version: **0.10.5** (`--release 0.10.5-m104`). FW_VERSION "0.10.5-m10.4", FW_MILESTONE "m10.4". Package
`build/step-M10.4/fm1acc-0.10.5-m10.4.fwsc`; identity FM-1_46001005 if the scheme holds. Record what you get.

## Read this first (the coder is Sonnet)

- **MUST / MUST NOT** are hard rules. Everything else is the intended design; if it turns out impossible or clearly
  worse in the code, choose the SMALLEST change that still passes the part's checks, then record it.
- **docs/decisions-M10.4.md (new; you write it)**, same format as decisions-M10.3.md, numbered M10.4-01, ...
- **MUST NOT** change: the song store's flash code (store.c, store.h), the USB song commands (SC_LIST, SEND, DELETE,
  WIPE, FACTORY: their bytes and behaviour), the update loader, the rescue combos, the factory reset, the hardware
  safety rules in CLAUDE.md, the settings record, the saved user presets, any song file, firmware/vendor/.
- **MUST NOT** refactor code a part does not need. No renames of existing functions or files.
- **Testing:** build after each part (no new warnings); builtin_test after Part 2; the trimmed run of Part 5 ONCE on
  the final package via the test-runner subagent. This run is the emulator run CLAUDE.md requires before hardware.
  Never relax a check to pass; a stale expectation is updated to the exact new value and recorded.
- **Never run a device command.** End by writing docs/step-M10.4-install.md and STOP.

## PART 0 — Housekeeping
- LOG: M10.3 built (b13125b / a3de01f), not installed; M10.3-01 answered by this plan. device.md: M10.3's package is
  "built, superseded by M10.4, never installed". Create docs/decisions-M10.4.md.

## PART 1 — Two fixes to M10.3
1. **Remove the knock-knock joke** (the user's decision: no risk of being flagged). firmware/app/app.h: delete the
   joke's lines (the paragraph starting "A knock-knock joke for whoever reads this"). The contact paragraph stays
   unchanged. `grep -rni "knock" --exclude-dir=.git --exclude-dir=build . ` afterwards finds only plans, LOG and
   decisions files. No other comment addressed to readers or AI agents anywhere.
2. **The kit pop-up says "Band Kit" again** (bt.c, the VK_KIT title; tools/m10_test.py's expectation). The user asked
   for "Band Drum Kit" only if a kit changed the drums alone. It also sets the bass and harmony programs
   (BT_KIT_BASS / BT_KIT_HARM), so the old name stays. M10.3's SEQ-while-playing and help rows stay.

## PART 2 — The built-in starter library (D070 option 1)
The songs and backtracks of `songs/library.txt` (today 15 songs, 42 backtracks) go inside the firmware, compressed,
as a read-only second source beside the song store. A new FM-1, or one after a factory reset, has them at once.
Measured by the planning chat: 73 KB raw (the 15 songs 18.7 KB, the largest file 1,644 B; the backtracks about
54 KB); about 14 KB as one zlib stream, about 30 KB zlib per file. App space free in M10.3: 62,284 B.
1. **The pack tool** `tools/builtin_pack.py` (GPL SPDX line), run by fm1.toml gen at every build ->
   `{gen}/builtin_gen.h`. It reads ONLY `songs/library.txt`. It MUST refuse any path under songs/private/ and any
   path not in the list. It compiles each entry with the same compiler entry point `fm1song send` uses, and MUST
   check in the tool that the result is byte-identical to what send sends. Per entry: slug, raw length, CRC32 (the
   CRC the store records), the packed data. Also the list's metadata (title, key, category, bpm, beats_per_bar,
   numerals, kind, number, scale, key_mode), read from the compiled file by fm1song's own Python reader. Defines
   BUILTIN_N and BUILTIN_MAX (the largest raw file rounded up to 256).
2. **Compression** (your choice, recorded): (a) our own small LZ77 (byte-oriented, LZ4-like) with a shared
   dictionary that the pack tool builds from the library; the decoder is about 60 lines of C. Or (b) a vendored
   permissive inflate (zlib or MIT licence, e.g. tinf or uzlib) under the third_party rules, added to NOTICE and
   REUSE.toml. Each file decodes on its own, one at a time, into a small buffer: the whole library cannot be
   unpacked (about 9 KB of RAM is free). The decoder MUST bounds-check both input and output: a corrupt pack gives
   an error, never a write outside the buffer.
   **Budget, MUST:** app space free >= 16 KB and RAM .data+.bss >= 4 KB free. If the pack does not fit, stop this
   part, record the numbers and propose what to leave out. Do NOT drop songs yourself.
3. **Firmware (library.c, plus the open call in bt.c):**
   - lib_scan: the store's songs as today, then each built-in whose slug no valid store song has. **The store
     wins:** a song sent later replaces its built-in copy, and deleting it brings the built-in back at the next
     scan. Sorted by title as today. Everything sized for the LIST (order[], indices, counts) holds
     STORE_MAX_SONGS + BUILTIN_N; order[] becomes uint16_t. For built-ins, lib_peek returns the const generated
     metadata (no RAM copy). Folders and categories, and BACKTRACK's list, work for built-ins as for store songs.
   - Opening: a helper `lib_file(k, slot, &p, &len)` gives a store file in place (as today), or decodes a built-in
     into file slot `slot` and checks its length and CRC32. Two slots of BUILTIN_MAX bytes (static_assert <= 2048).
     SONG LEARN uses slot 0. BACKTRACK's bt_prepare uses slot `half`, because the playing backtrack and the next
     one are open at once. lib_open and bt_prepare call the helper instead of store_get / store_file; in bt.c that
     is the only change to the open. song_open then runs on the buffer exactly as on a store file. A built-in that
     fails to decode or check shows SONG ERROR (open_err) like a bad store song; the rest work.
   - The USB commands MUST NOT change: they see the store only. `fm1song list` / send / delete / wipe /
     factory-reset, tools/fresh_install.sh and m51 are therefore unchanged, and `fm1song list` does not show
     built-ins (record it). A wipe or factory reset leaves the built-ins (they are part of the firmware). The WIPE
     result text "No songs on the FM-1 now." becomes "Only the built-in songs now."
   - player_stats: the number of built-ins listed, and the open song's source (store / built-in).
4. **tools/builtin_test.c + builtin_test.sh** (host, like store_test / song_fuzz, with ASan). For every built-in:
   the decoded file equals the compiled file byte for byte, its CRC matches, song_peek's fields equal the generated
   metadata, and song_open succeeds. Then 30 s of random corruptions of the pack: errors only, no ASan report.

## PART 3 — Docs
- docs/reference.md: a "Built-in songs" section: what a new FM-1 has, what sending, deleting, wipe and factory reset
  do to them, and that `fm1song list` shows only the store. Version line "for firmware 0.10.5".
- docs/controls.md: an "M10.4" section. NOTICE / REUSE.toml if the decoder is third-party.

## PART 4 — Build
- The release build (two builds, same sha). Record app size, free space (MUST >= 16 KB), RAM, the pack's size.

## PART 5 — The trimmed test run (once, on the final package, via the test-runner subagent)
It covers M10.3 and M10.4. It is small because no song changed (pytest's goldens prove every song's events are
identical, so no song needs a play-through), the store code and USB commands are MUST NOT, and the presets are
untouched. Ask the subagent for its usual summary (under 30 lines); times go to build/step-M10.4/times.txt. Run two
lanes at once, each with its own --out folder:
- **Lane A (laptop tools):** pytest tools/fm1song (with `-n auto` only if pytest-xdist is already installed: do not
  install it); `songs/make_songs.py` for all slugs changes no file; store_test; upreset_test; vsynth_test LEVELS=1
  (report POTATO's level after CRUSH 127; no change is made); builtin_test.
- **Lane B (emulator):** a new `tools/m104_test.py` (the structure of tools/m101_test.py), short runs only:
  - m104p1, a new FM-1 (empty store): it boots; SONG LEARN lists 15 songs and BACKTRACK 42 (stats); the list footer
    is drawn at the end. Open and play 8 s each: greensleeves (the largest file), auld-lang-syne (a pickup), a 3/4
    song. Then two backtracks, changing the progression while playing so both file slots are used. No halt; CPU as
    in M10.
  - m104p2, the store wins: a store with oh-susanna and one private song lists 16 songs with no duplicate, and
    oh-susanna opens from the store (stats). After the emulator's `fm1song delete oh-susanna` there are still 16,
    with oh-susanna now built-in.
  - m104p3, BACKTRACK (M10.3): HOME from playing and from paused goes to SETUP in one tap. SEQ while playing changes
    kit, kit_bass_prog and kit_harm_prog with no halt, and the pop-up reads "Band Kit". After ALGO goes from LOOP to
    VERSE/CHORUS, ind_name is not "LOOP" in the next frame.
  - m104p4, SONG LEARN (M10.3): with a preset as the melody sound, a key goes to vsynth (key_route). With SONG, the
    keys' program on channel 16 is the song's melody program. On a section's first bar the Start pop-up sets
    big_sub.
  - m104p5: one screenshot each (ppm2png) of the splash, the mode screen and BACKTRACK's help (the ten-row tables of
    M10.3-02) to docs/step-M10.4/. Read them and record what you see.
  - Regressions, trimmed: m5 --only store,flash,boot (the flash IDs on this package); m10 --only m10p2,m10p3,m10p5;
    m101 --only m101p3,m101p6 (the factory reset, and the whole fresh_install.sh on the emulator, now with built-ins
    present: 57 sent, and the list still 57 with no duplicates).
- **Not run, and why (write it in LOG):** m51 (store and commands unchanged; m101p6 sends all 57 through the same
  path); m6-m9; m10p1/p4/p6/p7; m10_flash and m101_flash (presets untouched); song_fuzz (the parser is unchanged and
  the decoder has builtin_test); every-song play-throughs.
- A stale expectation: fix it to the exact new value and re-run only that group. A real failure: fix it, then
  re-run only the failed group (plus builtin_test if Part 2 code changed). Record each re-run.

## PART 6 — Install note
- docs/step-M10.4-install.md: install over **M10.2** (M10.3 was never installed). No factory reset and nothing to
  send: no song changed, and saved presets and settings stay. Pop-ups is ON again after the update. The unit's store
  already holds the 57 library files, so the store copies win and the list looks the same. Pre-flight as M10.2's,
  with P4b the trimmed run's result. Fallback: M10.2's package.
- A by-hand list with one line per item of the user's M10.2 notes (the LOOP label, HOME from playing / paused, SEQ
  while playing with the bass / harmony sound changing too, "Band Kit", the list footer, the keys with a preset and
  with SONG, the Start pop-up, POTATO, the splash and version, the mode screen's mascot and bold "Kick"). Add **the
  built-ins (optional):** the user runs `tools/fm1song.sh delete oh-susanna`, and Oh! Susanna is still in the list
  (built-in; `fm1song list` no longer shows it); a send puts the store copy back. The empty screens appear only if
  the pack fails: say so.
- LOG, commit after each part. STOP.
