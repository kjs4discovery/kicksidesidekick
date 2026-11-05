# fm1song command line. Licence: TBD (see D017)
#
#   fm1song validate SONG_DIR...             check; errors and warnings with bar:beat; exit 1 on errors
#   fm1song compile SONG_DIR [-o OUT.fm1s] [--format 1.0]   default OUT: build/songs/<slug>.fm1s
#   fm1song info FILE.fm1s|SONG_DIR          a short summary
#   fm1song dump --json FILE.fm1s|SONG_DIR   everything in the compiled file, as JSON
#   fm1song preview SONG_DIR OUT.mp3 [--mute harmony] [--level bass=2 ...] [--no-count-in] [--tempo 80]
#   fm1song list [--port NAME]               the songs stored on the FM-1 (USB)
#   fm1song send SONG_DIR|FILE.fm1s... [--port NAME]     compile / check, then store each on the FM-1 (WRITES its
#                                            flash song area; a song with the same slug is replaced)
#   fm1song delete SLUG... [--port NAME]     remove songs from the FM-1 (WRITES its flash song area)
# Exit codes of list / send / delete: 0 done, 1 a song refused or failed, 2 bad arguments or a song that does not
# compile, 3 no FM-1 found, 4 connection lost / no answer, 6 the FM-1 runs another firmware (nothing sent).
import argparse
import json
import sys
from pathlib import Path

from . import LAYERS, SongError, compile_folder, read_compiled
from .chords import note_name

ROOT = Path(__file__).resolve().parents[2]


def _load(path, version=None):
    """a .fm1s file or a song folder -> (compiled dict, bytes, diagnostics or [])"""
    p = Path(path)
    if p.is_dir():
        blob, diag = compile_folder(p, version)
        return read_compiled(blob), blob, diag.items
    blob = p.read_bytes()
    return read_compiled(blob), blob, []


def _print_diags(items, name):
    for d in items:
        print(f"{name}: {d}", file=sys.stderr)


def cmd_validate(args):
    status = 0
    for folder in args.songs:
        try:
            _, _, items = _load(folder)
            _print_diags(items, folder)
            print(f"{folder}: ok" + (f" ({len(items)} warning(s))" if items else ""))
        except SongError as e:
            _print_diags(e.diagnostics, folder)
            print(f"{folder}: FAILED", file=sys.stderr)
            status = 1
    return status


def cmd_compile(args):
    try:
        compiled, blob, items = _load(args.song, (1, 0) if args.format == "1.0" else None)
    except SongError as e:
        _print_diags(e.diagnostics, args.song)
        return 1
    _print_diags(items, args.song)
    out = Path(args.output) if args.output else ROOT / "build/songs" / f"{compiled['slug']}.fm1s"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(blob)
    print(f"{out}: {len(blob)} bytes, crc32 {compiled['crc32']}")
    return 0


def summary(c):
    bpb = int(c["time_signature"].split("/")[0])
    bar_ticks = bpb * c["ppq"]
    first_number = 0 if c["pickup_beats"] else 1
    secs = (c["end_tick"] - ((bpb - c["pickup_beats"]) * c["ppq"] if c["pickup_beats"] else 0)) \
        * c["tempo_us"] / c["ppq"] / 1e6
    lines = [f"{c['title']} ({c['slug']})  format {c['version']}, {c['size']} bytes, crc32 {c['crc32']}",
             f"key {c['key']}, {c['time_signature']}, {c['tempo_bpm']:g} bpm"
             f"{', swing' if c['swing'] else ''}, pickup {c['pickup_beats']} beat(s), "
             f"{c['bar_count'] - (1 if c['pickup_beats'] else 0)} bars, {int(secs // 60)}:{int(secs % 60):02d}",
             "sections: " + ", ".join(f"{s['name']} {s['first_bar'] + first_number}-"
                                      f"{s['first_bar'] + s['bars'] - 1 + first_number}" for s in c["sections"])]
    chords = c["chords"]
    lines.append(f"chords ({len(chords)}): " + " ".join(x["symbol"] for x in chords)
                 + f"; {len(c['chord_events'])} changes")
    for name in LAYERS:
        ly = c["layers"].get(name)
        if ly:
            counts = ", ".join(f":{v} {sum(1 for e in evs if e[1] == 0x90)} notes" for v, evs in ly["variants"].items())
            prog = "" if ly["program"] is None else f", program {ly['program'] + 1}"
            lines.append(f"  {name:8} ch {ly['channel'] + 1}{prog}: {counts}")
        else:
            lines.append(f"  {name:8} (none)")
    first = c["chord_events"][:4]
    lines.append("voicings: " + "; ".join(
        f"{chords[e['chord']]['symbol']} L1 " + " ".join(note_name(n) for n in e["voicing"][0])
        + " / L2 " + " ".join(note_name(n) for n in e["voicing"][1]) for e in first if e["voicing"][0]))
    del bar_ticks
    return "\n".join(lines)


def cmd_info(args):
    try:
        compiled, _, items = _load(args.file)
    except SongError as e:
        _print_diags(e.diagnostics, args.file)
        return 1
    _print_diags(items, args.file)
    print(summary(compiled))
    return 0


def cmd_dump(args):
    try:
        compiled, _, _ = _load(args.file)
    except SongError as e:
        _print_diags(e.diagnostics, args.file)
        return 1
    print(json.dumps(compiled, indent=1 if args.json else None, sort_keys=False))
    return 0


def cmd_preview(args):
    from .preview import render, write_audio
    try:
        compiled, _, _ = _load(args.song)
    except SongError as e:
        _print_diags(e.diagnostics, args.song)
        return 1
    levels = {name: 1 for name in compiled["layers"]}
    for item in args.level:
        name, _, n = item.partition("=")
        if name not in levels or not n.isdigit():
            print(f"--level {item}: expected LAYER=N for a layer of this song", file=sys.stderr)
            return 2
        levels[name] = int(n)
    for name in args.mute:
        levels.pop(name, None)
    write_audio(render(compiled, levels, count_in=not args.no_count_in, tempo_pct=args.tempo), args.output)
    print(f"{args.output}: " + ", ".join(f"{k}:{v}" for k, v in levels.items()))
    return 0


def _device(args):
    from . import device
    backend = device.PipeBackend() if args.emu_pipe else None
    if backend is None:
        try:
            backend = device.I.MidoBackend()
        except ImportError:
            raise device.DeviceError(2, "needs mido and python-rtmidi in the venv")
    dev = device.connect(backend, args.port)
    print(f"FM-1: {dev.id.text} ({dev.name})", file=args.out)
    return device, dev


def cmd_list(args):
    device, dev = _device(args)
    entries, end = device.song_list(dev)
    if end["result"]:
        print("the FM-1 cannot keep songs: its flash is not usable (RAM ONLY)", file=args.out)
        return 1
    for e in entries:
        print(f"  {e['slug']:24} {e['len']:6} bytes  {e['sectors']:2} sectors  {e['status']}", file=args.out)
    print(f"{end['count']} song(s); {end['free']} of {end['sectors']} sectors free (longest run {end['largest']}); "
          f"reads format 1.0..1.{end['minor']}", file=args.out)
    return 0


def cmd_send(args):
    blobs = []
    for path in args.songs:               # every song checked on the laptop first: nothing is sent if one fails
        p = Path(path)
        if not (p.is_dir() and (p / "song.yaml").is_file() and (p / "song.mid").is_file()) and p.suffix != ".fm1s":
            print(f"{path}: skipped (not a song folder with song.yaml and song.mid, nor a .fm1s file)", file=args.out)
            continue                      # (songs/* also matches make_songs.py and README.md)
        try:
            if args.no_validate:
                blob = p.read_bytes()
            else:
                compiled, blob, items = _load(path)
                _print_diags(items, path)
        except SongError as e:
            _print_diags(e.diagnostics, path)
            print(f"{path}: does not compile: nothing sent", file=sys.stderr)
            return 2
        blobs.append((path, blob))
    if not blobs:
        print("no songs to send", file=sys.stderr)
        return 2
    device, dev = _device(args)
    status = 0
    for path, blob in blobs:
        r = device.song_send(dev, blob)
        ok = r["result"] == 0
        print(f"{path}: {'stored' if ok else 'NOT stored: ' + device.explain(r)} "
              f"({len(blob)} bytes; {r['songs']} song(s) on the FM-1, {r['free']} sectors free)", file=args.out)
        status |= not ok
    return status


def cmd_delete(args):
    device, dev = _device(args)
    status = 0
    for slug in args.slugs:
        r = device.song_delete(dev, slug)
        ok = r["result"] == 0
        print(f"{slug}: {'deleted' if ok else 'NOT deleted: ' + device.explain(r)}", file=args.out)
        status |= not ok
    return status


def main(argv=None):
    ap = argparse.ArgumentParser(prog="fm1song", description="FM-1 Accompaniment song compiler (fm1-song/1)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("validate", help="check song folders")
    p.add_argument("songs", nargs="+")
    p.set_defaults(fn=cmd_validate)
    p = sub.add_parser("compile", help="song folder -> .fm1s")
    p.add_argument("song")
    p.add_argument("-o", "--output")
    p.add_argument("--format", choices=["1.1", "1.0"], default="1.1",
                   help="1.0: the M1 layout (8-byte events), for an M1 device or tests")
    p.set_defaults(fn=cmd_compile)
    p = sub.add_parser("info", help="summary of a .fm1s file or a song folder")
    p.add_argument("file")
    p.set_defaults(fn=cmd_info)
    p = sub.add_parser("dump", help="the compiled file as JSON")
    p.add_argument("file")
    p.add_argument("--json", action="store_true", help="pretty-printed JSON (the only output format)")
    p.set_defaults(fn=cmd_dump)
    p = sub.add_parser("preview", help="render an audio preview (.mp3 or .wav)")
    p.add_argument("song")
    p.add_argument("output")
    p.add_argument("--mute", action="append", default=[], choices=LAYERS)
    p.add_argument("--level", action="append", default=[], metavar="LAYER=N")
    p.add_argument("--no-count-in", action="store_true")
    p.add_argument("--tempo", type=int, default=100, help="percent of the song's tempo")
    p.set_defaults(fn=cmd_preview)
    for name, fn, what in (("list", cmd_list, "the songs on the FM-1"),
                           ("send", cmd_send, "store songs on the FM-1 (writes its song area)"),
                           ("delete", cmd_delete, "remove songs from the FM-1 (writes its song area)")):
        p = sub.add_parser(name, help=what)
        if name == "send":
            p.add_argument("songs", nargs="+", metavar="SONG_DIR|FILE.fm1s")
            p.add_argument("--no-validate", action="store_true",
                           help="send the files as they are, unchecked (only for testing the FM-1's own check)")
        elif name == "delete":
            p.add_argument("slugs", nargs="+", metavar="SLUG")
        p.add_argument("--port", metavar="NAME", help="the MIDI port to use (part of its name)")
        p.add_argument("--emu-pipe", action="store_true", help=argparse.SUPPRESS)   # fm1-live FM1_USB_HOST
        p.set_defaults(fn=fn)
    args = ap.parse_args(argv)
    args.out = sys.stderr if getattr(args, "emu_pipe", False) else sys.stdout
    try:
        return args.fn(args)
    except OSError as e:
        print(f"fm1song: {e}", file=sys.stderr)
        return 1
    except Exception as e:                # device.DeviceError: what went wrong, and the exit code it carries
        if type(e).__name__ != "DeviceError":
            raise
        print(f"fm1song: {e}", file=sys.stderr)
        return e.code
