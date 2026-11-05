# fm1song: the FM-1 Accompaniment song compiler (format fm1-song/1). Licence: TBD (see D017)
#
# Core API (no printing, no exiting; a website can reuse it):
#   load_song(folder)          -> (Song, Diagnostics)     raises SongError
#   compile_song(song)         -> bytes (.fm1s)
#   read_compiled(bytes)       -> dict (checked like the device parser does)
#   compile_folder(folder)     -> (bytes, Diagnostics)
# The command line is fm1song.cli (tools/fm1song.sh).
from .binformat import compile_song, read_compiled
from .errors import Diagnostic, Diagnostics, SongError
from .source import FORMAT, LAYERS, load_song, song_from_data

__all__ = ["compile_song", "read_compiled", "load_song", "song_from_data", "compile_folder",
           "Diagnostic", "Diagnostics", "SongError", "FORMAT", "LAYERS"]


def compile_folder(folder):
    song, diag = load_song(folder)
    blob = compile_song(song)
    read_compiled(blob)                     # never write a file the device would refuse
    return blob, diag
