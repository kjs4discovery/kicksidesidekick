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
- The first release build (commit ac96f73, sha256 7904930c...) was superseded before any test: Part 5 needed three more diagnostic fields in player_stats (M10.4-05), which changes the package. **The package that is tested and installed:
  commit 801ad75 (a clean tree: only the user's untracked import/ and the screenshots folder docs/step-M10.4/), two builds, the same sha256
  4ccaea14b998554cbcb370a15ea70880dec2e45ba6b9c38e6f70c385602db221**, build/step-M10.4/fm1acc-0.10.5-m10.4.fwsc.
  App 554,980 B of the 585,728 B app area: **30,748 B free (M10.3: 62,284 B; the MUST is 16 KB)**; RAM .data+.bss 89,936 B of 98,304 (8,368 B free; the MUST is 4 KB);
  pool 295,560 of 344,064. The pack: 22,162 B packed + 3,712 B dictionary = 25,874 B. No new compiler warnings (the two older ones: EAS eas_voicemgt.c, bt.c:234).

## M10.4-05 — Part 5: how the trimmed run was set up
- **Three more diagnostic words in player_stats** (after M10.4's `builtins`, `open_src`): `key_route` (the last key pressed went to 0 EAS / 1 vsynth), `keys_prog` (the last
  program sent on the keys' channel, 0xFF none yet), `big_sub` (the last pop-up's sub line was the big section name). The plan's m104p4 asks to observe exactly these and M10.3 had
  no way to see them; no behaviour changes.
- **tools/m104_test.py** (the structure of m101_test.py; its stats reader appends the five M10.4 words after M10.1's texts). m104p1 compares every played built-in with the store's copy of the
  same song: the emulator's audio is sample-identical, so "CPU as in M10" is the same CPU, and the limit 60 % of a block (M10.1's) is checked too. The user's "one private song" in
  m104p2 is a copy of saints under another slug and title ("zzz-private", "Zzz Private Test"): the real songs/private/ songs are never read by a test (m5.PRIVATE_SKIP). A store song of
  the same title as a built-in comes first in the list (the insertion sort is stable and the store's songs are scanned first); m10p5's "noinfo" copy of saints relies on it.
- `builtins` counts the built-ins in the lists of both modes (songs and backtracks): 57 on a new FM-1; list_n counts the songs of SONG LEARN's folder (15).
- **Stale expectations fixed to the exact new value** (the built-ins fill every list, so nothing is "empty" any more): tools/m5_test.py store-empty (PLAY now starts the first song:
  starts 1); tools/m101_test.py m101p3 (15 songs before the reset: 2 sent + 13 built-in; 15 after it) and tools/m10_test.py m10p5 (the "noinfo" song is opened by its list index).
- **make_songs.py** for all slugs was run by the main session, not the test-runner subagent (it writes into songs/; the subagent may write only into build/): `git status` showed no song file changed.
- **Dev runs:** tools/m104_test.py itself was run against dev builds while it was being written (its own expectations: the meaning of `builtins`, HOME twice = the mode screen, ALGO's applied structure,
  a not-found delete never leaves the splash). The official run below is the ONE run of the final package, with the subagent.
