# HANDOFF

Written by the planning chat and the user. Claude Code reads this at the start
of every session and before every step, and never edits it except to tick a
checkbox the note asks it to tick.

---

## 2026-11-04 14:45 — from the planning chat — after Step 0.1

Step 0.1 reviewed: looks good. Thanks for the clear log.

### Answers to your questions
1. **Rust:** only used to build laptop-side tools (the fm1-emu emulator's native
   build, and possibly M-Flasher in step 0.3). Our firmware will be **C** on the
   FM-1's JieLi pi32v2 CPU, built with whatever toolchain the chosen base uses
   (decided in step 0.5). **Do not add any rustup embedded targets** — none apply.
2. **Stock version and flash chip:**
   - Stock firmware version → **Step 0.2** (read-only identity query; the user
     will paste the prompt next).
   - Flash chip ID + full flash backup → **Step 0.3**, and in 0.3 you only
     *prepare* the procedure; the user runs every device command.

### Notes for Step 0.2
- The unit is on factory firmware; version unknown (could be V14, V15 or V16).
  If the identity tool doesn't recognise the version, record exactly what it
  printed and stop — don't try another tool that sends anything else.
- If any tool's "info"/setup path would download firmware, explain first. Saving
  stock packages to disk for safekeeping is fine; sending them anywhere is not.
- Small fix: your first LOG entry says "nothing committed yet", but there is a
  commit (c071c5b). No action needed beyond being accurate in future entries.

### Current step
- [ ] Step 0.2 — Identify the unit (read-only)

---

## 2026-11-04 14:58 — from the planning chat — FYI, no action needed during Step 0.2

The planning chat added a **draft test song** at `songs/amazing-grace/`:
`song.yaml` (lead sheet + metadata, draft source format v0.1), `song.mid`
(9 tracks: conductor, chords, melody, harmony:1/2, bass:1/2, drums:1/2),
`make_amazing_grace.py` (regenerates song.mid with the standard library only;
`--preview` needs numpy + ffmpeg), and `previews/*.mp3`.

- Don't modify these files; they're a fixture for Steps 1.1–1.3. The format is a
  draft and may change.
- When you next commit, include them (the mp3 previews are ~0.9 MB each; fine to
  commit, or add `songs/*/previews/` to .gitignore if you prefer — note which in LOG).
- Finish Step 0.2 first; this doesn't change it.

---

## 2026-11-04 15:02 — from the planning chat — after Step 0.2

Step 0.2 reviewed: excellent work, especially reading the whole --info path and
recovering the pinned V15 package from the Wayback Machine.

### Answers
1. **V16 on M-VAVE's site:** nothing in the plan assumes V16. The unit stays on
   **FM-1_015** (D012). V15 is actually the best starting point: most community
   tooling and return-to-stock steps are built around it. Keep FM-1_016.fwsc on
   disk for reference only; **never propose installing it** unless the user asks.
   Our "restore to stock" target is `backups/stock-firmware/FM-1_015.fwsc`.
2. **V14:** not needed. Unit is V15.

### Notes for Step 0.3
- Version is settled; the flash chip is still unknown. The unit was bought new
  in late October / early November 2026, so treat 0xB36014 as quite likely.
- If the chip is 0xB36014: record it, do NOT propose any further step (no backup
  attempt with a tool that doesn't explicitly support it), and stop for the
  planning chat.
- Allow rules like the one in .claude/settings.local.json must stay limited to
  exact read-only commands. Never add a rule that matches a write.

### Current step
- [x] Step 0.2 — Identify the unit (read-only) — done, FM-1_015
- [ ] Step 0.3 — Flash chip ID + full flash backup (prepare only; user runs device commands)

---

## 2026-11-04 15:06 — from the planning chat — decision D014 (no action during Step 0.3)

The user accepted **D014**: chord symbols live in `song.yaml` (the chord chart),
next to `song.mid`, not in a MIDI "chords" track. A song is a folder:
`song.yaml` (metadata, rights, key, tempo, pickup_beats, form, layers, chart) +
`song.mid` (notes only). The sample in `songs/amazing-grace/` follows this; its
`chords` MIDI track is now just an optional cross-check.

- After Step 0.3 is finished (not during it), update the Project rules in
  CLAUDE.md to: "Compiler input: a song folder with song.mid (tracks drums, bass,
  harmony, melody, optional variants like bass:1, bass:2) and song.yaml (metadata
  and the chord chart; D014). See songs/amazing-grace/." Note it in LOG.

---

## 2026-11-04 15:45 — from the planning chat — after Step 0.3

Step 0.3 reviewed: very good. Flash ID 0x856014 (Puya), chip key 0x980F, full
backup confirmed by 4 identical reads in 2 sessions. The B36014 stop rule no
longer applies.

### Answers
1. **Why the firmware region differs from the V15 package:** almost certainly
   per-chip encryption, not a problem. m-flasher's docs/safety.md says a package's
   firmware "must decrypt with this unit's chip key", and its Analysis decrypts
   flash with CHIP_KEY 0x980F. The on-flash firmware is encrypted with the chip
   key, so it was never going to match the package bytes. (Planning chat's reading
   of the source; not yet confirmed by decrypting our dump.)
2. **Restore plan (proposed as D015, pending the user):**
   - *Normal return to stock* = install `backups/stock-firmware/FM-1_015.fwsc`
     through the normal update path. That's what the community and m-flasher do
     with official .fwsc files; the loader handles the encryption.
   - *Last resort / bit-exact* = the full dump, written via ROM boot mode
     (restores calibration, Bluetooth address and settings exactly).
   Keep both. Update the 15:02 note's "restore target" wording accordingly in
   docs/device.md (add a short "Restore options" section).
3. **M-Flasher hardware cross-check:** not needed; skip it. It would mean another
   ROM boot session for no new information. Optional, offline only (zero hardware):
   if it's easy, build a tiny program against m-flasher's `boards::Analysis` that
   reads our dump file and reports what it identifies (board, firmware version,
   regions). Only if it takes under ~30 minutes; otherwise skip and note it.
4. **udev rule:** fine as installed (4c4a:8057 only).
5. **Previews:** committing them is fine.

### Notes to carry into Step 0.7 (first writes)
- "One loader command per ROM boot session": write it into the 0.7 checklist.
- The repeated -110 descriptor timeouts and 15–20 s enumeration on USB port 1-2:
  before any write, test other USB ports (and another data cable) with read-only
  `lsusb`/`--info` until we find a port that enumerates cleanly. Record it in
  docs/device.md. Don't do any write on a port that shows -110 errors.

### Next
- After this: do the CLAUDE.md D014 update (15:06 note), then **Step 0.4** (emulator).
- During 0.4–0.6 the FM-1 is not needed: the user will unplug it.

### Current step
- [x] Step 0.3 — done (0x856014, backup OK). Off-laptop copies: pending on the user.
- [ ] Step 0.4 — Emulator on Mint

---

## 2026-11-04 16:00 — from the planning chat — after Step 0.4

Step 0.4 reviewed: great result (240x240 RGB565, 27 keys F3–G5, 14 buttons,
7 encoders, 2x pi32v2 @ 240 MHz, scriptable headless runs).

### Answers
1. **Headless-only on this laptop:** yes. Automated tests run headless (deterministic
   emulated clock, WAV + panel capture). The live window is a smoke test only.
   Real listening happens on the unit (after 0.7) or a faster computer.
2. **JEDEC-ID option:** yes, as a **local patch** to our copy of fm1-emu. Keep it
   as a patch file (e.g. `tools/patches/fm1-emu-jedec-id.patch`) plus a script that
   applies it, so third_party/ can be refreshed. Make it an env var or flag
   (e.g. `--flash-id b36014`). Needed by Step 0.6. Don't open an upstream PR; the
   user decides that later.
3. **Polly / fm1-chord / CoralFlower:** yes, study them **in Step 0.5**:
   - Treat **Polly** (QY70-style arranger) as a possible **4th candidate** next to
     fm1-dock / Felucca / FM1 Open: could our play-along mode be built on it or
     borrow its band engine? Report its licence, base, code quality, how it
     schedules the band, voice usage, and whether it supports 0xB36014.
   - fm1-chord and CoralFlower: chord voicing/display code worth reusing? Licences?
   - Same rule as always: read, build, run in the emulator only.

### Next
- **Step 0.5** (pick the base): the user will paste the prompt. Add the Polly
  candidate and the reuse questions above to docs/base-comparison.md.

### Current step
- [x] Step 0.4 — done
- [ ] Step 0.5 — Pick the base (incl. Polly as candidate D)

---

## 2026-11-04 15:58 — from the planning chat — D015 accepted (no action during Step 0.5)

The user accepted **D015**. When convenient (after 0.5), change the "Restore options"
heading in docs/device.md from "proposed … pending the user" to "accepted 2026-11-04".

---

## 2026-11-04 16:30 — from the planning chat — base decided; notes for Step 0.6

Step 0.5 reviewed: excellent, thorough comparison. Decisions by the user:

- **D016 — Base: fm1-base (on felucca-bsp) + Sonivox EAS for General MIDI sounds.**
  Pin fm1-base and felucca-bsp to the commits you evaluated (or newer, if you note
  why). Polly and TRIO are references only.
- **D017 — We write our own code.** Reading Polly / TRIO / fm1-chord / CoralFlower /
  Felucca to understand techniques is fine; **copying GPL code is not**. Allowed as
  libraries: fm1-base, felucca-bsp (MPL-2.0) and **upstream** Sonivox EAS (Apache-2.0).
  Our own files: MPL-2.0 header for now.
- D015 heading in docs/device.md: still to update (15:58 note), if not done yet.

### Before writing code in Step 0.6, check and report in LOG
1. **EAS provenance:** where Polly got Sonivox EAS (upstream repo + commit), and
   which of Polly's EAS files are modified. Read their licence headers. If modified
   files are GPL-3.0 (or unclear), use **upstream** EAS and list what we'd have to
   re-do ourselves (e.g. sample-accurate note starts within a block). Do not copy
   Polly's modifications.
2. **GM sound bank licence:** which wavetable/sound set the ~200 KB GM bank is, where
   it comes from, and its licence. If it isn't clearly redistributable, stop and
   report.
3. **Flash budget:** fm1-base (~222 KB) + EAS code + GM bank vs the ~581 KB app slot,
   and where song data will live (data area / resource region). Report numbers.

### Step 0.6 scope (the user will paste the prompt)
- A minimal app on fm1-base: title + version on screen; holding a key plays that
  note on GM Electric Piano 1 through EAS; key name shown on screen.
- App accepts 0x856014 and 0xB36014, refuses unknown IDs without crashing (RAM-only).
- Loader: add the "refuse unknown flash ID" check to our build of the felucca-bsp
  loader if it's small; if not, report what it would take and don't block on it.
- fm1-emu JEDEC patch (16:00 note) so both IDs + one unknown ID can be tested.
- Emulator only. Nothing to the FM-1.

### Current step
- [x] Step 0.5 — done; base decided (D016, D017)
- [ ] Step 0.6 — Hello world on fm1-base + EAS (emulator only)

---

## 2026-11-04 16:36 — from the planning chat — correction to the 16:30 note (D017)

The user is deferring the licence decision. Replace "Our own files: MPL-2.0 header
for now" with:
- Our own files carry `Licence: TBD (see D017)`, no licence header yet.
- Keep third-party code (fm1-base, felucca-bsp, EAS) in its own clearly marked
  folders with its own licence files, never mixed into our files.
- Write original code whenever possible.

---

## 2026-11-04 17:10 — from the planning chat — after Step 0.6; preparing Step 0.7

Step 0.6 reviewed: great milestone. Our own firmware on fm1-base + upstream EAS,
both flash IDs OK, unknown ID → RAM-only, 340 KB of ~581 KB used.

### Answers
1. **Loader refusal on an unknown chip:** keep it as is. Stay in update mode and
   write nothing on a chip we don't know. Don't drop the update record.
2. **Velocity:** not now. In our concept the user plays their own instrument; the
   FM-1 keys are mostly controls. Leave APP_TOUCH off; revisit in Phase 1 if needed.
3. **Identity FM-1_71xx:** before 0.7, check it against the identities used by known
   firmwares (Firmware Hub list, awesome-fm-1, the catalogue in fm1-emu) and pick an
   unused range. Note what you checked. Keep the placeholder words URL for now.
4. **Sample-accurate note starts:** fine to defer. Note it as a Phase 1 item: revisit
   if the drums feel loose. Measure EAS CPU load on the device in 0.7.
5. Small polish for later (Step 1.6, not now): in the mini keyboard drawing, the black
   keys look slightly off-centre between the white keys.

### Step 0.7 — rules for this step (first WRITES to the FM-1)
- The user will switch you to "ask" permission mode for this step.
- **Prepare only.** Write the checklist to `docs/step-0.7-first-writes.md` and then
  **STOP**. The planning chat and the user review it together before any device
  command is run. Say clearly in LOG that it is waiting for review.
- Pre-flight items you must show as ✅/❌ (any ❌ = stop):
  - Off-laptop backup copies recorded in docs/device.md (**currently TBD**: the user
    still has to make them; ask them).
  - A USB port + cable that enumerates cleanly (no -110 errors in dmesg, normal
    enumeration time). Test read-only: plug, `dmesg | tail`, `lsusb`, `--info`.
  - Restore target per D015: `backups/stock-firmware/FM-1_015.fwsc` via the normal
    update path; full dump as last resort, with the exact last-resort commands.
  - Round-trip firmware: suggest **Felucca 1.8.1 stable** (or say why another);
    confirm from source that its app and loader accept 0x856014.
  - Our `fm1acc.fwsc`: the exact build (commit + sha256) that passed the emulator.
- The checklist order: (1) community firmware → boots → a setting survives a power
  cycle; (2) back to stock V15 → `--info` shows FM-1_015; (3) our hello world → note
  plays, screen OK, note EAS CPU load; (4) back to stock V15. One loader command per
  ROM boot session. What to do if any step fails, for each step.

### Current step
- [x] Step 0.6 — done
- [ ] Step 0.7 — prepare checklist, then STOP for review

---

## 2026-11-04 17:15 — from the planning chat — Step 0.7 scope changed (D018)

The user decided (D018) that Step 0.7 is bigger. It supersedes the "checklist order"
in the 17:10 note; the other 17:10 rules still apply (ask mode, prepare only, STOP
for review, pre-flight ✅/❌).

- **Part A (emulator only):** a prototype that plays songs/amazing-grace/song.mid
  with EAS's MIDI-file player, PLAY/stop, per-layer mute buttons, current/next chord
  (from song.yaml, converted at build time), bar:beat. Prototype code clearly marked.
- **Part B (prepare only):** checklist order is now: (1) community firmware,
  (2) stock V15, (3) **our prototype** (not the hello world), (4) stock V15 from our
  firmware, (5) our prototype again, **left installed**. Hello world = fallback
  diagnostic only.
- Full prompt: the user will paste it (vault build plan, Step 0.7).

---

## 2026-11-04 20:40 — from the planning chat — REVIEW of docs/step-0.7-first-writes.md: APPROVED with additions

Reviewed with the user. Excellent checklist. Answers and additions:

1. **Felucca 1.8.1:** approved (tag f4df296, blob = GitHub's, emulator boot, loader read).
2. **"A setting survives a power cycle":** non-blocking, as written. Suggest the user
   saves something with Felucca's SAVE button (or the palette preference) and checks
   it after the power cycle; record what was tried.
3. **P1 (thumb drive + repo, no cloud):** accepted for this step. The writes only touch
   the app area; the dump matters only for path B2. Cloud copy recommended later.
4. **P2 (the unit's single -110, ~16 s):** accepted; your stop rule (a second -110 or
   >25 s) stands.
5. **80 ms chord lead:** fine; the user judges by eye in step 3.
6. **Add to "Rules for every step"** before the user starts:
   - Disable the laptop's automatic suspend / sleep for the session (and no lid closing).
   - Close fm1-live / the emulator, aplaymidi and any other MIDI apps (fm1-live with
     `--midi all` subscribes to ports).
   - The FM-1 stays on USB power the whole time; don't switch it off during a write.
   - The user runs the commands in their own terminal tab; Claude Code only reads
     (`--info`, `lsusb`, kernel log) between steps and records.
7. The "P2 procedure" section is now historical (P2 decided); mark it so.

**Go:** after adding items 6–7, the user runs steps 1–5 one at a time, pasting each
step's output to you before starting the next. Stop and report on any unexpected
output.

### Current step
- [x] Step 0.7 Part A — done
- [x] Step 0.7 checklist — reviewed and approved (20:40)
- [ ] Step 0.7 Part B — user runs steps 1–5

---

## 2026-11-04 22:58 — from the planning chat — Phase 0 done; Phase 1 starts with Milestone M1

Step 0.7 reviewed: all five writes passed; prototype left installed. Phase 0 is done.
Thank you. Device CPU/sound notes from the user will come later.

User feedback #1 on the prototype: "NEXT" showed the next chord change a bar too
early when a chord lasted two bars (bars 4-5, 15-16). Fix in M1b with a bar strip
(this bar + next 3) and a beat countdown before a change.

Phase 1 now runs as **Milestone M1** (proposed D019): M1a = steps 1.1-1.3 (format v1,
fm1song, 3-5 songs), STOP for format review; M1b = steps 1.4-1.5 + hardware
checkpoint. The user will paste the M1a prompt from the vault's build plan.

### Current step
- [x] Step 0.7 — done; Phase 0 complete
- [ ] Milestone M1a — song format v1, compiler, starter songs

---

## 2026-11-04 23:02 — from the planning chat — M1 runs overnight as ONE prompt

The user accepted D019 and will run M1a + M1b as one prompt overnight (auto mode,
FM-1 unplugged). Changes to the 22:58 note:
- No stop between M1a and M1b. Log the format summary and your doubts; continue.
- Record every judgement call in LOG with the reason (the planning chat reviews in
  the morning).
- End with docs/step-M1-install.md and STOP. No hardware tonight.

### Current step
- [ ] Milestone M1 (M1a + M1b) — overnight, ends with the install checklist

---

## 2026-11-05 07:00 — from the planning chat — REVIEW of Milestone M1: APPROVED

Impressive overnight run. Reviewed: LOG 23:16 and 23:44, docs/song-format.md
(summary), docs/controls.md, docs/step-M1-install.md, screenshots.

### Install checklist (docs/step-M1-install.md)
- **Approved as written.** Pre-flight P1–P7 accepted.
- **Optional step 3: skip.** The update code is byte-identical to Step 0.7's
  (P3), where leaving our firmware was proven.

### Format questions (23:16)
- (a) Loop rule with pickups: accepted.
- (b) Compiler-computed voicings: accepted for now. Note for the D004 experiment
  (step 1.10): hand-arranged songs may need voicings taken from harmony:1; keep
  that as a possible v1.x option.
- (c) 8-byte events: accepted. The flash limit (484 of ~581 KB with 5 songs) makes
  step 1.7 (songs in the data area, `fm1song send`) the next priority after M1
  feedback; delta coding can come with it.
- Arrangement licence: **TBD**. The user is deferring licence decisions (D017
  covers code; songs likewise). Write `arrangement_license: TBD` for now, not CC0.
- Scarborough Fair's "check before publishing": good, keep it.

### Decisions digest (the vault's decisions you asked about)
- D004 (deferred): hand-arranged MIDI vs chord chart + generated parts; test in
  step 1.10. Generated starter songs are fine meanwhile.
- D009 + D014: song = folder with song.mid (notes, layer/variant tracks) and
  song.yaml (metadata + chord chart). The Song Format Draft has nothing v1 is
  missing; optional lyrics in the melody track stay ignored (v1 may skip them).

### Controls / cosmetics
- docs/controls.md: fine as the M1 proposal; the user's feedback comes after
  testing.
- Agreed: the current bar's box should stay bright even when the chord is held
  over (step 1.6).

### Current step
- [x] Milestone M1 built and reviewed
- [ ] M1 install, steps 1–2, run by the user

---

## 2026-11-05 10:10 — from the planning chat — M2 scope received; M2a instructions

Thanks for logging the M2 discussion (09:53); the planning chat has filed it in the
vault. Decisions: **D020** (the user's M2 scope, as you logged it), **D021** (OCT-/OCT+
shift the keys' octave after boot; holding them at power-on stays the rescue; REC
stays unused; update docs/controls.md's rule), **D022** (proposed: split M2 into M2a
and M2b; the user is confirming).

- Your split proposal is accepted with one change: item 9 (OCT keys) moves to M2a
  (small, no risk). Item 11 (songs in the data area) is M2b: a new write path,
  reviewed on its own.
- The temporary logo is now in the repo: `firmware/assets/tmp-logo.jpg` (copied by
  the planning chat from the vault; the user's file).
- Full instructions: **docs/plan-M2.md**. The user will start M2a with a short prompt.
- Your user-notes review (LOG 2026-11-05) was good. Note for the future: the vault is
  the planning chat's; reading it is fine when the user points you to it, as here.

### Current step
- [x] M1 installed and tested by the user
- [ ] M2a — practice controls and fixes (docs/plan-M2.md), ends with the install checklist

---

## 2026-11-05 10:18 — from the planning chat — HOLD: M2 scope being revised

The user rejected the M2a/M2b split (D022): M2 will be ONE build, possibly with a few
more items. docs/plan-M2.md will be rewritten. Don't start M2 until the user pastes
the new prompt.

---

## 2026-11-05 10:35 — from the planning chat — M2 is ONE build: docs/plan-M2.md rewritten

HOLD lifted. docs/plan-M2.md now describes a single M2 (parts 1-5): D020's items plus
D023 (beat light, three more songs, bass note names, key lights on the FM-1's keys).
The user will start it with a short prompt. Commit and keep a tested build per part.

---

## 2026-11-05 10:40 — from the planning chat — early install point after Part 2

docs/plan-M2.md now has an EARLY INSTALL POINT: after Part 2, write
docs/step-M2-part2-install.md, log it, and continue. The user's weekly usage may run
out; this makes sure an installable build with parts 1-2 exists.

---

## 2026-11-05 12:50 — from the planning chat — REVIEW of Milestone M2: final checklist APPROVED

Excellent work: all five parts, 175 checks, the store's power-cut testing, the crackle
diagnosis (aliasing on GM 81, not clipping).

- **docs/step-M2-install.md: approved as written.** The early part-2 checklist is not
  needed (the whole build is done); mark it superseded.
- **Both harmony and bass muted → harmony wins:** accepted for now; the user decides
  after testing.
- **CLAUDE.md rules** (update after the install, not during it):
  - `fm1song send` and `fm1song delete` write the FM-1's flash (song area): add them to
    the list of commands only the user runs.
  - `fm1song list` (a SysEx question, writes nothing): allowed for Claude Code as a
    read-only command, like `--info`; say before running it.
- **The three new melodies (from memory):** fine for testing; keep "check before
  publishing" until checked against public-domain scores.

### Current step
- [x] M2 built and reviewed
- [ ] M2 install: steps 1-3 run by the user

---

## 2026-11-05 12:45 — from the planning chat — checklist bug in M2 step 2b (no harm done)

`tools/fm1song.sh send songs/*` also matched songs/make_songs.py (and README.md): the
tool refused it ("does not compile: nothing sent"), correct behaviour. The user is
checking with `list` and, if needed, sending `songs/*/` (folders only).
After the install (not now): fix docs/step-M2-install.md to `songs/*/`, and make
`fm1song send` skip non-song arguments (not a folder with song.yaml + song.mid, and
not a .fm1s) with a one-line "skipped" note instead of an error. Record in LOG.

---

## 2026-11-05 17:40 — from the planning chat — Milestone M3 planned: docs/plan-M3.md

The user tested M2 (notes reviewed by the planning chat; they're in the vault).
M3 = one build (D024, D025): bugs, "follow your part" for all four layers with blinking
in rhythm, loops redesigned (A–B only), screen polish, louder count-in beep, inversion
label, a small settings screen, a MIDI import tool + Happy Birthday, guitar chord
diagrams, bass tab, scrolling views (melody display, simplified notation per
docs/m3-design/notation-mock.png, drum grid), song folders, guide volume. Early
install point after Part 5. The user will start it with a short prompt.
- The CLAUDE.md rule updates from 12:50 and the `songs/*/` fix are Part 1 item 6.

---

## 2026-11-05 17:45 — from the planning chat — M3 changes before you start

- The MIDI import tool moved out of M3 (to M4: the user wants to think it through).
  Part 5 is now just Happy Birthday; the early install point stays after Part 5.
- Testing: fast checks after each part; the FULL emulator suite only on builds offered
  for install (early install point and the end). docs/plan-M3.md is updated.

---

## 2026-11-05 19:45 — from the planning chat — REVIEW of Milestone M3: final checklist APPROVED

Excellent: nine parts, full suite 235 checks on the exact package.

- **docs/step-M3-install.md: approved as written**, incl. the settings save (a second
  kind of flash write: 0xFC000–0xFDFFF via fm1-base's own A/B store; stock never uses
  it). The early checklist is superseded; mark it so.
- **CLAUDE.md updated by the planning chat** (your proposed text, applied verbatim:
  `fm1song send` / `delete` in the NEVER list, `fm1song list` in the read-only list).
  Please commit it ("CLAUDE.md: fm1song rules, applied by the planning chat").
  `fm1song list` is now yours to run between steps.
- **Proposals accepted for testing** (the user may change them after trying): the last
  layer muted is "your part"; settings open with GLO, GLO; folders one HOME up; guide
  levels OFF / 15 / 30 %.
- **Small things** (help screen not mentioning HOME = folders; "format 1.1" in a test
  message): fix in the next build, not now.
- **Human checks before beta** (the planning chat records them in the vault): a
  guitarist for the shape table, a staff reader for the notation, and the melodies
  typed from memory.

### Current step
- [x] M3 built and reviewed
- [ ] M3 install: steps run by the user
