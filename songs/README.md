# Songs

Each folder is one song in format `fm1-song/1` (docs/song-format.md): `song.yaml` (the lead sheet) +
`song.mid` (the notes), and `previews/` with two mp3s (full band at level 1; harmony muted, level 2).

| folder | song | category (M3) | key, metre | notes from |
|---|---|---|---|---|
| amazing-grace | Amazing Grace | Hymns | G, 3/4, 1-beat pickup | `amazing-grace/make_amazing_grace.py` (the planning chat's sample) |
| twelve-bar-blues | Twelve-Bar Blues in C | Blues | C, 4/4 shuffle | `make_songs.py` |
| saints | When the Saints Go Marching In | Hymns | F, 4/4, 3-beat pickup | `make_songs.py` |
| four-chords | Four Chords in G (I-vi-IV-V) | Practice forms | G, 4/4 straight 8ths | `make_songs.py` |
| scarborough-fair | Scarborough Fair | Folk | D dorian, 3/4 | `make_songs.py` |
| greensleeves | Greensleeves (M2) | Folk | A minor, 3/4, 1-beat pickup, 88 BPM | `make_songs.py` |
| danny-boy | Londonderry Air (Danny Boy) (M2) | Folk | C, 4/4, slow (66 BPM), slash chords (C/E, C/G, D7/F#, G/F) | `make_songs.py` |
| oh-susanna | Oh! Susanna (M2) | Folk | D, 4/4, fast (150 BPM) | `make_songs.py` |
| happy-birthday | Happy Birthday to You (M3) | Celebrations | G, 3/4, 2-eighth pickup, 104 BPM | `make_songs.py` |

**Categories (M3):** `category:` in song.yaml puts the song in a folder of the FM-1's list (format 1.2). Songs
without one are in "Other"; every song is also in "All songs".

**On the FM-1 (M2):** songs are no longer built into the firmware: send them with `tools/fm1song.sh send songs/*/`
(the FM-1 on USB, running FM-1 Accompaniment M2 or later; an M2 FM-1 gets format 1.1, without the category).
**Drums, level 1 vs 2:** the M2 and M3 songs make level 1 the plain beat (kick, snare backbeat, hi-hat; M3: M2's
kick and soft hi-hat alone were lost under the band) and level 2 a full groove with fills (the M1 songs: 1.5–1.8
times the hits of level 1; they stay as they are for now, D004 decides their future).

**All of these are generated (D004).** No note was recorded, transcribed from a recording or copied from a
score file: the melodies are typed into the scripts as note lists (traditional tunes as we know them, or
our own), and the drums, bass and harmony are built from the chord chart by simple patterns. Edit the
scripts, not the generated `song.yaml`/`song.mid` (`make_amazing_grace.py` writes only `song.mid`; that
song's `song.yaml` is hand-written).

Regenerate and check:

```bash
.venv/bin/python songs/make_songs.py --preview
tools/fm1song.sh validate songs/*/
```

Rights: Amazing Grace, the Saints and Scarborough Fair are traditional / public domain tunes (bases noted in
each `song.yaml`; Scarborough Fair's widely known modern versions are copyrighted arrangements, ours is a
simplified version of the traditional tune). The blues riff and the Four Chords melody are our own. M2:
Greensleeves (traditional, 16th century), the Londonderry Air (traditional Irish, printed 1855; no lyrics: the
1913 "Danny Boy" words are not used) and Oh! Susanna (Stephen Foster, 1848; no lyrics) are public domain tunes;
our melodies are simplified versions typed from memory, so each is marked **check before publishing** (the
melody against a public-domain source; well-known modern arrangements are copyrighted, ours are our own). M3:
Happy Birthday to You uses the melody of "Good Morning to All" (Mildred J. Hill and Patty Hill, 1893, public
domain), no lyrics; also typed from memory and marked **check before publishing**. The arrangement licence of all
of them is still TBD.
