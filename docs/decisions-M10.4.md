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
