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
simplified version of the traditional tune). The blues riff and the Four Chords melody are our own. The
arrangement licence of all of them is still TBD.
