# Decisions of the M10.4 run (Sonnet coder; for the planning chat's review)

Format (docs/plan-M10.4.md, "Read this first"): one entry per judgment call the plan does not spell out, numbered M10.4-01, ...
The user was away and said not to stop: each call was decided and recorded here.

## M10.4-01 — Housekeeping
- M10.3 (b13125b / a3de01f) is recorded as built, superseded by M10.4, never installed (docs/device.md, LOG). The planning chat's plan-M10.4.md answers M10.3-01:
  the library and the trimmed test run are M10.4. docs/HANDOFF.md (modified by the planning chat) and docs/plan-M10.4.md are committed with Part 0 as they are.

## M10.4-02 — Part 1: two fixes
- app.h: the joke's three lines removed (the blank comment line before it too); the contact paragraph is unchanged. `grep -rni knock` finds only plans, HANDOFF and decisions/LOG.
- bt.c VK_KIT default title "Band Kit"; tools/m10_test.py's two expectations; docs/reference.md's SEQ row. docs/controls.md's M10.3 section line is kept as history with a note that M10.4 reverts it.
  docs/step-M10.3-install.md (a package that is never installed) is left as it was.
- The build log's two older warnings (bt.c '^' within '|', EAS eas_voicemgt.c) are not new.

## M10.4-03 — Part 2: the built-in starter library
- **Compression: (a), our own LZ77** (no third-party code, so no NOTICE / REUSE change). Format in firmware/app/builtin.c's header: an LZ4-style
  token (literal count, match length - 4), 16-bit offsets, history = a shared dictionary then the output; the decoder is 45 lines and checks
  every input read and every output write (builtin_test: 500,000 corrupt packs in 5 s, errors only, ASan + UBSan clean).
- **The dictionary** is a few whole files of the library, chosen greedily by tools/builtin_pack.py with zlib's preset-dictionary size as the
  estimate (fast: 2.4 s; scoring with the Python LZ itself took 36 s a round): at most 4 files, each pick must save at least 300 B. Result:
  bt05-pop-4, blow-the-man-down, bt39-minor-cadence (3,712 B). **Measured (measured by the pack tool): 57 files, 73,336 B raw (largest 1,904 B ->
  BUILTIN_MAX 2048), 22,162 B packed + 3,712 B dictionary = 25,874 B** (zlib per file would have been 33,626 B; one zlib stream 13,559 B).
  The estimate depends on zlib's output sizes, so another zlib version could choose another dictionary; the pack is still correct then.
- **Build time:** compiling 57 entries twice (the send route and compile_folder, to prove them identical) takes 24 s, so the tool keeps a cache
  next to its output ({gen}/builtin_gen.cache, a pickle of {path: (sha256 of the folder's files and tools/fm1song/*.py, compiled blob)}); a hit
  skips the two compiles, not the reader check. A cached build costs 7 s. The cache is only read from the build directory.
- **The byte-identity check** in the tool: the blob from fm1song.cli._load (the function `fm1song send` calls) must equal compile_folder's
  blob and the reader's dict must equal the one from the first route; builtin_test.sh additionally compiles every entry with `fm1song compile`
  and builtin_test.c compares the unpacked file with it.
- **Metadata** for the list comes from read_compiled (title, key, category, numerals, kind, number, scale, key_mode, time signature's numerator,
  bpm by player_song_bpm's formula in Python); builtin_test.c checks it against song_peek on the unpacked file. `struct lib_item` moved from
  library.c to builtin.h (fields unchanged) so the generated const table can use it; the table is `bi_items[]` in flash (no RAM copy).
- **Source indices:** the list's order[] (now uint16_t) holds a source index k: k < STORE_MAX_SONGS is the store's song k, k >= STORE_MAX_SONGS is
  built-in k - STORE_MAX_SONGS. peeked[] and peek_err[] stay store-sized (the USB commands index them as before), open_err[] is LIB_MAX, and
  lib_raw() / lib_perr() give a row and its peek error for any k (ui.c's four uses of peeked[order[..]] / peek_err[order[..]] go through them).
  ui.fl[] and bt.list[] (uint8_t) became uint16_t: the list can hold 200 + 57 entries.
- **The store wins** by slug: a built-in is listed unless a store song that peeks has the same slug (the store record's slug). A store song that does not peek
  does not hide it (so both rows show: the unreadable one last). Deleting a song makes the next scan bring the built-in back (delete already
  rescans).
- **No flash (lib_state < 0):** the list stays empty and "NO SONG STORAGE" shows as before; the built-ins are not offered without a store (the plan does not say; the
  store is also where a user's songs would go, and the screen explains it).
- **lib_file(k, slot, &p, &len, &crc):** a store song in place (callers still compare song_crc32 with the record's CRC, as before), a built-in decoded into
  slot 0 or 1 and checked here (length by the decoder, CRC-32); any failure is SONG_E_CRC ("damaged"), shown as SONG ERROR for that song only. The
  slots (2 x 2,048 B) are in the .pool section (PLAT_POOL), not .bss. lib_open uses slot 0 (SONG LEARN), bt_prepare slot `half`.
- **USB unchanged in bytes and behaviour:** sc_list walks the store's entries (n = nstore, not the list's count) and sc_result's song count is nstore.
  `fm1song list` therefore shows only the store. WIPE / FACTORY RESET leave the built-ins. The wipe result screen says "Only the built-in songs now."
  and the two toasts that said "no songs now" say "built-in songs only" (they were the same message).
- **player_stats** (appended after M10.1's texts): `builtins` (built-ins in the list), `open_src` (the last file opened by SONG LEARN or BACKTRACK: 0 none,
  1 the store, 2 a built-in).
- **Budget (MUST):** dev build image 554,900 B (M10.3: 523,444 B): +31.5 KB; app area 585,728 B, so about 30.8 KB free (>= 16 KB); RAM .data+.bss 89,920 B
  of 98,304 (8.4 KB free, >= 4 KB); pool 295,560 of 344,064. The plan's guess for the pack was 14-30 KB; it is 25.9 KB + the decoder and tables.

## M10.4-04 — Part 4: version and release build
- Version 0.10.5-m10.4 (FW_VERSION), FW_MILESTONE "m10.4", `--release 0.10.5-m104`; identity FM-1_46001005 (the scheme held). tools/m10_test.py's source check of FW_MILESTONE updated to "m10.4" (a stale expectation, exact new value).
- Release build from commit ac96f73 (a clean tree: only the user's untracked import/): two builds, the same sha256 7904930c8653a2ff03dbc2d4b977daece88d8a89e5fa21b440a4a2961ebc39b0.
  App 554,916 B of the 585,728 B app area: **30,812 B free (M10.3: 62,284 B)**; RAM .data+.bss 89,920 B of 98,304 (8,384 B free); pool 295,560 of 344,064. The pack: 22,162 B packed + 3,712 B dictionary = 25,874 B.
  No new compiler warnings (the two older ones: EAS eas_voicemgt.c, bt.c:234).
