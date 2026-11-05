# Songs

Each folder is one song in format `fm1-song/1` (docs/song-format.md): `song.yaml` (the lead sheet) +
`song.mid` (the notes), and `previews/` with two mp3s (full band at level 1; harmony muted, level 2).

| folder | song | key, metre | notes from |
|---|---|---|---|
| amazing-grace | Amazing Grace | G, 3/4, 1-beat pickup | `amazing-grace/make_amazing_grace.py` (the planning chat's sample) |
| twelve-bar-blues | Twelve-Bar Blues in C | C, 4/4 shuffle | `make_songs.py` |
| saints | When the Saints Go Marching In | F, 4/4, 3-beat pickup | `make_songs.py` |
| four-chords | Four Chords in G (I-vi-IV-V) | G, 4/4 straight 8ths | `make_songs.py` |
| scarborough-fair | Scarborough Fair | D dorian, 3/4 | `make_songs.py` |
| greensleeves | Greensleeves (M2) | A minor, 3/4, 1-beat pickup, 88 BPM | `make_songs.py` |
| danny-boy | Londonderry Air (Danny Boy) (M2) | C, 4/4, slow (66 BPM), slash chords (C/E, C/G, D7/F#, G/F) | `make_songs.py` |
| oh-susanna | Oh! Susanna (M2) | D, 4/4, fast (150 BPM) | `make_songs.py` |

**On the FM-1 (M2):** songs are no longer built into the firmware: send them with `tools/fm1song.sh send songs/*`
(the FM-1 on USB, running FM-1 Accompaniment M2). **Drums, level 1 vs 2:** the three M2 songs make level 1 just
the pulse (kick and hi-hat) and level 2 a full groove with fills: 2.2–3.6 times the drum hits of level 1, 7 drum
sounds instead of 3 (the M1 songs: 1.5–1.8 times; they stay as they are for now, D004 decides their future).

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
melody against a public-domain source; well-known modern arrangements are copyrighted, ours are our own). The
arrangement licence of all of them is still TBD.
