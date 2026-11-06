# Song format v1 (`fm1-song/1`)

Milestone M1a, 2026-11-04. Built from the draft v0.1 sample (`songs/amazing-grace/`, the planning chat's
Song Format Draft) and D014 (chords live in `song.yaml`, notes in `song.mid`). The reference implementation
is `tools/fm1song/` (Python); the device reads only the compiled file (`.fm1s`).

```
song folder                     tools/fm1song.sh compile             tools/fm1song.sh send        device
  song.yaml  (lead sheet)   ──►  checks everything, bar:beat   ──►  over USB, checked again ──►  its flash song
  song.mid   (notes)             errors; deterministic output        on the FM-1 before it is     store (section 4)
                                 (build/songs/<slug>.fm1s)           written
```

M2 (2026-11-05): format **1.1** stores the event streams delta-coded (section 2.2, `EZ` chunks: the files are 40 %
of 1.0's), and the songs live in the FM-1's flash, sent from the laptop (section 4); M1 bundled them into the
firmware.

M3 (2026-11-05): format **1.2** adds the song's **category** (`category:` in song.yaml, the `CATG` chunk): the
FM-1's list groups songs into folders by it. The M3 device reads 1.0, 1.1 and 1.2. `fm1song send` asks the FM-1
which versions it reads and, for an older one (M2: up to 1.1), sends 1.1 without the category.

## 1. Source: a song folder

A folder named after the song's slug, holding `song.yaml` and `song.mid`. Optional: `previews/`
(mp3s), a generator script (D004: say so in `rights.generated`).

### 1.1 `song.yaml`

```yaml
format: fm1-song/1            # required, exactly this
slug: amazing-grace           # required: a-z 0-9 '-', 1..23 characters
title: Amazing Grace          # required: printable ASCII, up to 31 characters (the device font)
category: Hymns               # optional (M3, format 1.2): the list's folder, printable ASCII, up to 15 characters
credits:                      # required mapping; free text (music, words, arrangement, ...)
  music: ...
rights:                       # required mapping
  composition: public domain  # required text: the basis for using the tune
  arrangement_license: TBD    # optional
  generated: make_songs.py    # optional: the script that generated the notes (D004)
music:
  key: G                      # required: C, F#, Bb, ... ; "Em" = E minor
  mode: dorian                # optional: major (default) minor dorian mixolydian lydian phrygian locrian
  time_signature: 3/4         # required: 2/4 .. 7/4 (v1)
  tempo_bpm: 84               # required: 30..250, quarter notes; one tempo per song (v1)
  pickup_beats: 1             # optional, default 0: beats played in the pickup bar (bar 0)
  feel: straight              # optional: straight | swing (information; the swing is in song.mid)
form:                         # required: sections in order, covering bars 1..N with no gaps
  - { section: Verse 1, bars: 1-16, chart: verse }
  - { section: Verse 2, bars: 17-32, chart: verse, pickup_beats: 1 }
layers:                       # required: at least one of drums, bass, harmony, melody
  drums:   { channel: 10, variants: { 1: "...", 2: "..." } }       # drums: channel 10, no program
  bass:    { channel: 3, program: 33, variants: { 1: "...", 2: "..." } }
  harmony: { channel: 2, program: 5,  variants: { 1: "...", 2: "..." } }
  melody:  { channel: 1, program: 74 }                             # no variants: just variant 1
chart:                        # the lead sheet
  pickup: "| N.C. |"          # required when pickup_beats > 0: exactly one bar (bar 0)
  verse: |
    | G | G7 | C  | G | G  | Em | D | D7     |
    | G | G7 | C  | G | Em | D  | G | G . D7 |
  ending: "| G |"             # optional: replaces the song's last bar(s)
```

**Bar numbers** are what a musician counts: bar 0 is the pickup bar (only when `pickup_beats > 0`), bar 1
the first full bar. All messages use `bar:beat` (`12:3`, or `12:3+240` for 240 ticks after beat 3).

**Sections.** `section` is the name the device shows (printable ASCII, up to 15 characters). `chart` names
the chart part; if left out it is the section name in lower case without a trailing number ("Verse 2" →
`verse`). The chart part must have exactly as many bars as the section. `pickup_beats` on a section is its
lead-in: the beats before its first bar that belong to it musically (default: the song's `pickup_beats`).
The device uses it for loops (§3.3).

**Layers.** `channel` 1..16 (the device plays the layer on this channel, whatever the track says);
`program` is the General MIDI number 1..128 as printed in GM lists (stored 0-based); optional `volume` and
`pan` 0..127 (defaults 100, 64). `variants` maps 1..3 to a short description; every variant needs a track
in `song.mid` and vice versa. Variant 1 is the simplest (the default on the device).

**Chart syntax.** Bars are separated by `|`; every line starts and ends with `|`; lines just continue.
In a bar:
- one token per chord; a bar with several tokens and no `.` is split evenly (the count must divide the
  beats: `| C G |` in 4/4 = two beats each; three chords in 4/4 is an error);
- `.` holds the chord before it for one more beat; a bar with `.` must have one token per beat
  (`| G . D7 |` in 3/4 = G for two beats, D7 on beat 3);
- `N.C.` = no chord.

**Chord symbols**: root `A`..`G`, optional `#`/`b`, a quality, optional `/bass` note; at most 7
characters. Qualities: (none) `m` `dim` `aug` `+` `sus2` `sus4` `sus` `5` `6` `m6` `7` `maj7` `m7` `mmaj7`
`m7b5` `dim7` `7sus4` `9` `maj9` `m9` `add9`. Repeated chords merge: `| G | G |` is one chord event that
lasts two bars (so "next chord" is the real next change).

### 1.2 `song.mid`

- Standard MIDI File, format 1 (or 0), 24..960 ticks per quarter note.
- **Tick 0 is the start of the first bar in the file**: bar 0 (the full pickup bar, of which only the last
  `pickup_beats` beats may contain notes) if there is a pickup, otherwise bar 1.
- Tempo and time signature events must agree with `song.yaml` (one of each, v1).
- Layer tracks are named `drums`, `bass`, `harmony`, `melody`, each optionally with a variant: `bass:1`,
  `bass:2`, `bass:3` (a plain name = variant 1). Variants must be 1, 1-2 or 1-3.
- Other tracks (a `conductor`, the optional D014 `chords` text track, ...) are ignored; the `chords` track,
  if present, is cross-checked against the chart (a warning if they differ).
- Kept per layer: notes, controllers 1 7 10 11 64 91 93, pitch bend. Dropped with a warning: other
  controllers, aftertouch, program changes that differ from `layers.<name>.program` (if `program` is not
  set, the track's first program change is used). SysEx and other meta events are skipped.

**Errors** (the song is refused): a note with no note-off, a note that starts after the end of the song or
before the pickup, wrong tempo/metre, track/variant mismatches, more than the limits in §2.4.
**Warnings**: events on another channel than `song.yaml` says, a note retriggered before its note-off (the
first one is ended there), notes ringing past the end (cut at the end), unused chart parts.

## 2. Compiled file (`.fm1s`), versions 1.0, 1.1 and 1.2

Little-endian, every chunk starts on a 4-byte boundary (zero padding between chunks). Text fields are
fixed-size, NUL-padded, printable ASCII, and must contain at least one NUL. The reader refuses anything
that is not exactly as described; `fm1song info`/`dump` use the same checks as the device parser.

### 2.1 Header (32 bytes)

| off | type | field | rule |
|---|---|---|---|
| 0 | char[4] | magic | `FM1S` |
| 4 | u8, u8 | version major, minor | major must be 1; minor 0, 1 or 2 (1.1: `EZ` event chunks; 1.2: also the `CATG` chunk, below). `fm1song compile` writes 1.2 (`--format 1.1` / `1.0` for older devices); the M3 device reads all three, the M2 device 1.0 and 1.1, M1 only 1.0; an older device refuses a newer file safely (VERSION or EVENTS) |
| 6 | u16 | chunk_count | 1..24 |
| 8 | u32 | file_size | = the file's length, ≤ 256 KiB |
| 12 | u32 | crc32 | CRC-32 (zlib/IEEE) of the whole file with these 4 bytes as zero |
| 16 | u16 | ppq | 24..960 ticks per beat |
| 18 | u8, u8 | beats_per_bar, beat_unit | 2..7, 4 |
| 20 | u32 | tempo_us | µs per beat, 240000..2000000 (250..30 bpm) |
| 24 | u32 | end_tick | = bar_count × beats_per_bar × ppq |
| 28 | u16 | bar_count | 1..999, including bar 0 |
| 30 | u8 | pickup_beats | < beats_per_bar |
| 31 | u8 | flags | bit 0 swing; other bits 0 |

Then the **directory**: chunk_count × {char[4] id, u32 offset, u32 size}, entries in file order; chunks
must not overlap, must lie inside the file and appear once each. Unknown ids are skipped (forward
compatible). Playback starts at `start_tick` = (beats_per_bar − pickup_beats) × ppq when there is a pickup,
else 0. Bar *index* i starts at i × beats_per_bar × ppq; its *number* is i (pickup songs) or i + 1.

### 2.2 Chunks

All required except `EV..` chunks of layers that have no variants.

| id | size | content |
|---|---|---|
| `CATG` | 16 | **1.2 only, required there**: char category[16] (printable ASCII, NUL-terminated; empty = no folder) |
| `META` | 76 | u8 key_root (0..11), u8 key_mode (0..6, as §1.1's list), u16 0, char slug[24], char title[32], char key_name[16] ("G", "Em", "D dorian") |
| `SECT` | 4 + 24n | u16 n (1..32), u16 0; per section: u16 first_bar (index), u16 bar_count (≥1), u16 lead_ticks (< one bar), u16 0, char name[16]. Sections follow each other with no gaps from bar index 1 (pickup songs) or 0 to the last bar |
| `CHRD` | 4 + 12n | u16 n (1..64), u16 0; per chord: char symbol[8], u8 root (0..11, 0xFF = N.C.), u8 bass (0..11, 0xFF = none), u16 pitch-class mask (bit k = pitch class k; 0 only for N.C.) |
| `CEVT` | 4 + 16n | u16 n (1..2048), u8 levels (= 2), u8 notes (= 5); per chord change: u32 tick (strictly increasing, first = 0, < end_tick), u8 chord (index into CHRD), u8 flags (bit 0: on a bar line; must match the tick), u8 voicing_level1[5], u8 voicing_level2[5] (MIDI notes, 0 = unused) |
| `BARS` | 4 + 4n | u16 n (= bar_count), u16 0; per bar: u16 chord_event (the event sounding on the bar's first beat), u8 section (index), u8 flags (bit 0: a chord change on beat 1, bit 1: a section starts here) |
| `LAYR` | 4 + 8×4 | u8 4, u8 0, u16 0; for drums, bass, harmony, melody in that order: u8 channel (0..15; 0xFF if absent), u8 program (0..127, 0xFF = none), u8 variants (0..3), u8 volume, u8 pan, u8 flags (0), u16 0 |
| `EV` + L + V | 4 + 8n | layer letter L = `D` `B` `H` `M`, variant digit V = `1`..`3` (e.g. `EVB2` = bass:2). u32 n (≤ 65535); per event: u32 tick (non-decreasing, ≤ end_tick), u8 status (0x80 note-off, 0x90 note-on, 0xB0 controller, 0xE0 pitch bend; no channel bits: the layer's channel is used), u8 data1, u8 data2 (both ≤ 127; note-on velocity ≥ 1 and tick < end_tick), u8 0. At one tick: note-offs, then controllers, bends, note-ons |
| `EZ` + L + V | 4 + … | **1.1 and 1.2**, instead of the same stream's `EV` chunk (never both). The same events, delta-coded: u32 n, then per event: a first byte (bit 7: more delta bytes follow; bit 6: data2 is 0 and left out; bits 5–4: the status, 0x80 0x90 0xB0 0xE0; bits 3–0: the tick delta's low 4 bits), up to 3 more delta bytes (7 bits each, low first, bit 7: another follows), data1, then data2 unless left out. The delta is from the previous event's tick (the first: from 0) and never takes the tick past end_tick. The chunk ends exactly after event n. The same rules as `EV` apply to the decoded events. All `EZ` streams together: at most 8192 events (the device decodes them into a 64 KiB buffer) |

**Why this shape.** Fixed-size records make every bound a multiplication the parser can check before
reading; absolute ticks let the player binary-search a stream when it jumps (loop, variant change); the bar
table gives the "this bar + next 3 bars" strip without scanning; voicings are computed on the laptop so the
device does no music theory. 1.1's delta coding exists because events are 95 % of a file: a note-off becomes 2–3
bytes, a note-on 3–4, instead of 8; the device decodes them once, at open, into the same 8-byte records, so the
player is unchanged.

### 2.3 Keyboard voicings (computed by the compiler)

- Level 1: root position, root between D3 and C#4 (MIDI 50..61), up to 5 notes.
- Level 2: the closed inversion (lowest note E3..D#4, 9ths dropped) that moves least from the previous
  level-2 voicing; the first chord takes the one nearest level 1.
- Harmony variant 1 shows level 1; variants 2 and 3 show level 2. Slash basses are left to the bass layer.

### 2.4 Limits (v1)

999 bars, 32 sections, 64 different chords, 2048 chord changes, 65535 events per layer variant (1.1: 8192 in all
`EZ` streams together), 256 KiB per file (the FM-1's song store: 64 KiB), 3 variants per layer, 5 notes per
voicing. The five starter songs are 15–32 KiB each in 1.0, 6–12 KiB in 1.1.

## 3. What the device does with it (M1b; M2)

### 3.1 Playback
Events are sent to EAS on the layer's channel at the audio block where their tick falls. Position =
ticks from samples played, exact at any tempo (no accumulated rounding), so tempo changes do not drift. M2: the
tempo in BPM, 30–250; a song's own tempo is its `tempo_us` rounded to whole BPM.

### 3.2 Count-in
One bar of clicks, plus the empty beats of the pickup bar: the first note comes where the music says (for
Amazing Grace in 3/4 with a 1-beat pickup: "1 2 3 1 2", the pickup on 3). M2: the same from any start point: the
clicks run from the bar before the start's bar up to the start (a section with a pickup: the pickup's beats are the
last clicks' place).

### 3.3 Loops and pickups
A loop of sections s..t runs from `start(s) − lead(s)` to `end(t) − lead(s)`: the lead-in of the first
looped section is played at the top, and the same number of beats is cut from the end, exactly like a
repeat with a pickup in printed music. Whole-song loop = all sections. Held notes are released at the jump.
M2 adds a loop of bars A–B: from A's beat 1 to the end of B, no pickup. A loop the music is outside of (moved
while playing) is jumped to at the next bar line.

## 4. On the FM-1: the song store (M2)

The songs live in the flash's data area, **0x97000–0xDBFFF** (69 sectors of 4 KiB; fm1-base's data area below the
panel calibration at 0xDC000). M-VAVE's stock firmware keeps nothing there (our unit's full dump: all 0xFF from
0x97000 to 0xDFFFF; stock's own data is at 0x93000 and 0xE9000–0xFBFFF), so going back to stock is not affected.
`firmware/app/store.c`:

- One **record** per song, on whole sectors: a 32-byte head (`FM1R`, u32 sequence number, u32 file length, u32 the
  file's CRC-32, 12 bytes 0xFF, u32 CRC-32 of the head's first 28 bytes), then the `.fm1s` file.
- Writing a song: the whole file is received into RAM and checked first (CRC, then everything `song_open`
  checks); then free sectors are erased, the file written and read back, and the **head written last**. Only then
  is an older copy of the same song (same slug) erased, head sector first. Two copies (a cut came between): the
  higher sequence number wins, and the older is tidied away at the next send.
- No room beside the older copy: the older copy makes way first, but only if that makes room (else `FULL`, nothing
  erased). This is the one case in which a power cut loses the song being sent.
- Every erase and write is checked against 0x97000–0xDBFFF first; nothing outside is ever touched.
- Reading writes nothing. The list is a scan of the heads (their own CRC), after the splash, never during boot; a
  song is checked in full when it is opened (the record's CRC and `song_open`); a bad one shows `SONG ERROR`.

**USB** (`tools/fm1song.sh list | send | delete`, `tools/fm1song/device.py`; the FM-1 side `firmware/app/library.c`):
SysEx in the update's framing (fm1-base `APP_USB_CMD`), commands 0x70 LIST, 0x73 SEND, 0x74 DELETE; the FM-1
pulls a sent file with the update's 512-byte read requests and answers 0x7F RESULT. `fm1song` sends only to a
firmware whose identity starts `FM-1_46` (ours). A send or delete stops the song playing and returns to the list.

Tested: `tools/store_test.sh` (store.c on a simulated NOR flash, a power cut at every erase / write of an add, a
replace, a replace with no room for both copies, a delete) and `tools/m2_test.py` (the firmware in fm1-emu: sends
over emulated USB, junk in the area, a damaged stored song, damaged files sent unchecked, a stalled transfer,
power cuts during a send).

## 5. Deferred (not in v1)

Tempo changes and fermatas; metre changes and x/8 metres (6/8, 12/8); per-section repeats/volta brackets in
the chart (`%`, `:|`); chord-symbol aliases (`-7`, `Δ`), extended chords (11, 13, alt); lyrics on the
device (the MIDI lyric events are dropped); per-chord voicing overrides in `song.yaml`; more than three
variants; non-ASCII titles; drum-kit selection; signing of song files.
