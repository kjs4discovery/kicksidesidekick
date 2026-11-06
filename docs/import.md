# Importing a MIDI file (fm1song import)

`fm1song import` turns a Standard MIDI File into a song folder (`song.mid` + `song.yaml`) with the full level set the
FM-1 Accompaniment plays. It runs on the laptop only; sending the result to the FM-1 is `fm1song send`, which only
you run (CLAUDE.md). The core is `tools/fm1song/importer.py`, separate from the command line, so the website can
reuse it later (D010).

## Quick start

```bash
tools/fm1song.sh import MySong.mid --list
```
lists the file's tracks (number, name, channel, GM program, notes, how many notes start together) and the role each
gets by default:
- channel 10 → **drums**;
- the lowest-pitched part → **bass**;
- the highest single line → **melody**;
- the rest → **harmony**;
- a track with only a few notes (effects) → **drop**.

Then import, overriding the guess where it is wrong:
```bash
tools/fm1song.sh import MySong.mid --out songs/my-song --title "My Song" \
    --melody 4,5 --harmony 1,3 --bass 2 --drums 10 --drop 7,8
```
Several tracks for one role are merged: the melody tracks of a verse, chorus and refrain become one melody, and two
guitars one harmony part. The folder also gets `IMPORT-REPORT.txt`: what was mapped, merged, simplified and
generated.

Other options:
- `--category` (the FM-1's folder, default "Imported");
- `--rights` (song.yaml's rights.composition);
- `--credit`;
- `--style waltz|ballad|shuffle|march|pop|train`: the rhythm of the generated levels. By default it comes from the
  metre and tempo.

## What it does

1. **Reads** the tracks and their notes at the file's resolution, converted to 480 ticks a beat. Tempo and time
   signature are the file's first; the song format has one tempo and x/4 metres.
2. **Simplifies:**
   - every note starts and ends on a sixteenth; doubled notes (same pitch, same time) are merged;
   - the melody is one note at a time (the highest when several start together; a note ends where the next starts);
   - the bass is its lowest line;
   - the harmony has at most 5 notes starting together, within C2–C7.
3. **Trims** empty bars at the start. A melody that starts in the bar before the accompaniment becomes a pickup.
4. **Finds the key** (from all the pitched notes) and **proposes a chord chart**: one chord a bar, or two in 4/4
   when the halves differ. The chords are recognized from the harmony and bass notes (major, minor, 7, m7, maj7, sus4,
   dim). **Review the chart in song.yaml**: it is a proposal.
5. **Makes every level** (D004, hybrid; D026):

   | layer | levels | from |
   |---|---|---|
   | melody | L1 | the file's melody: quantized, doubled notes removed, nothing added |
   | harmony | L1 held, L2 rhythm, L3 inversions, L4 broken chords, L5 arpeggios | generated from the chart (tools/fm1song/arrange.py); **the file's own harmony part replaces the level it resembles** (auto-detected) |
   | bass | L1 held root, L2 rhythmic root, L3 root–fifth, L4 walking | the same: the file's bass replaces the level it resembles |
   | drums | L1, L2, L3 | **L2 = the file's drums**; L1 = simplified from them (kick, snare and hi-hat on the beats); L3 = the file's drums with generated fills every 4 bars |

   With no drums (or bass, or harmony) in the file, those levels are all generated.
6. **Fits the FM-1:** a song may have at most about 15,000 events with every level, and its file must stay under the
   FM-1's 64 KiB.
   - A long song first gets its file harmony and drums on an eighth grid, with at most 4 notes at once.
   - If it is still too big, only the first whole 8-bar sections that fit are kept. The report says so.
   - The song store holds 69 sectors (about 276 KB) for all songs: our 9 songs take 45. Send imported songs as
     space allows (`fm1song list` shows the free sectors; `fm1song delete` makes room).
7. **Writes** `song.mid`, `song.yaml` (sections of 8 bars named Part 1, Part 2, …) and the report. The folder is
   checked by compiling it.

## After importing

- **Check the chart** (`chart:` in song.yaml) by ear: open the folder's preview with
  `tools/fm1song.sh preview songs/my-song` if you like, or play it on the FM-1.
- Correct chords in song.yaml. The generated levels follow the chart, so run the import again with the same options,
  or edit song.mid's tracks yourself (D004: hand-edited is fine).
- Compile and validate: `tools/fm1song.sh validate songs/my-song/`.

## The user's private test songs (M4)

`import/private/` holds five MIDI files the user gave for testing (copyrighted pop songs). `songs/private/` holds
their conversions. Both are in .gitignore: never committed, and to be removed before beta. They were converted
with `songs/private/import_all.sh` (also ignored), using only the files' content, category "Private test", rights
"private test only, remove before beta". The roles chosen per file and the reasons are in docs/LOG.md (M4 Part 5).
