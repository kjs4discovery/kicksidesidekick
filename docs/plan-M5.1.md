# Milestone M5.1 — fix the song-send crash, then send all songs

From the planning chat, 2026-11-09. Context: LOG 2026-11-06 21:50 (incident) and 22:10. The unit runs
0.5.3-m5p3 with M4's songs (1.3). Sending songs to M5 caused watchdog resets (~8 s) inside the SEND
handler (library.c sc_send: store_begin's scan / tidy / compaction dry run and moves, the in-place check,
store_commit's rescans), so the unit went to SAFE MODE. The emulator missed it: flash erase/program are
instant there and XIP reads cost nothing.

```text
Follow CLAUDE.md and docs/HANDOFF.md. Milestone M5.1, emulator only, then an install checklist and STOP.
1. Make the emulator honest first: add realistic flash timing to our fm1-emu copy (sector erase,
   page program, XIP read cost; use datasheet-typical values for the Puya P25Q80 and say which),
   as a patch in tools/patches like the others. Reproduce the watchdog reset in the emulator with
   the M5 store and a full send of all 14 songs BEFORE fixing anything. Record the timings.
2. Fix:
   - no stretch over ~1 s without feeding the watchdog anywhere in the store, library, check and
     commit paths (measure the longest stretch on the timed emulator and report it);
   - move compaction out of the SEND path: when room is short the device answers "busy,
     making room" and compacts in small steps from the main loop with progress on screen, then
     the laptop resends (or the send waits, with keep-alives);
   - rescans incremental, not a full 552-block rescan after every step;
   - start-up never depends on the store's state (store scanned lazily, any content tolerated);
   - a recovery action: "WIPE SONG STORE" (settings or a power-on key combo that isn't a rescue
     combo) and `fm1song wipe` (user-run write).
3. Tests on the timed emulator: send all 14 songs one by one and in one command, with the store
   full / fragmented / half-written (power cuts during compaction), and a boot after each; the
   longest watchdog gap stays well under the limit. Full suite on the release build.
4. Version 0.5.4-m5p4. docs/step-M5.1-install.md: install over 0.5.3-m5p3; then send ONE song
   first and check it plays, then the rest (ours, then the private ones); fallback M4 as before.
   STOP for review.
Token saving (planning chat request): use the test-runner subagent (.claude/agents/test-runner.md,
Sonnet) for full-suite runs and long test outputs; keep design and debugging in the main session.
Keep LOG entries short.
```
