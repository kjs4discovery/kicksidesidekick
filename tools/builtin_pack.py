#!/usr/bin/env python3
# builtin_pack.py -- the built-in starter library: songs/library.txt -> a C header (firmware/app/builtin.h's data).
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   builtin_pack.py OUT.h                 pack every entry of songs/library.txt (run by fm1.toml's gen at every build)
#   builtin_pack.py --check PATH          exit 0 if PATH may be packed (listed in songs/library.txt, not private), else 1
#
# M10.4 (D070 option 1). It reads ONLY songs/library.txt and the song folders that list names; it refuses (exit 1) any path under
# songs/private/ and any path that is not in the list. Each entry is compiled with the entry point `fm1song send` uses (fm1song.cli._load)
# and the result is checked here to be byte-identical to a second compile and to what compile_folder gives (what send sends), readable by
# fm1song's own reader, and to survive pack -> unpack. Per entry the header has the slug, the raw length, the CRC-32 the store records
# (zlib's, over the whole file), the packed bytes, and the list's metadata (read from the compiled file by fm1song's reader).
#
# Compression: our own LZ77 (firmware/app/builtin.c documents the format) with ONE shared dictionary: a few of the library's own files
# (chosen greedily by a zlib estimate: the files that make the others pack smallest), used as the history before each file.
import argparse
import posixpath
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

LIBRARY = "songs/library.txt"
RAW_STEP = 256            # BUILTIN_MAX: the largest raw file rounded up to this
MAX_DICT_FILES = 4        # the dictionary: at most this many whole files
MIN_GAIN = 300            # bytes: a further dictionary file must save at least this much (packed + dictionary)


class PackError(Exception):
    pass


def library_entries(lib=None):
    """the list's paths, in order (comments and blank lines left out); every one is checked: relative, in songs/, never private"""
    lib = Path(lib) if lib else ROOT / LIBRARY
    out = []
    for line in lib.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        out.append(check_path(line, None))
    if len(set(out)) != len(out):
        raise PackError(f"{lib}: a path is listed twice")
    return out


def check_path(path, listed):
    """the normalised path if it may be packed, else PackError. listed: the list's paths (None: only the shape is checked)"""
    p = posixpath.normpath(path)
    parts = p.split("/")
    if p.startswith("/") or ".." in parts or parts[0] != "songs" or len(parts) < 2:
        raise PackError(f"{path}: not a path inside songs/")
    if "private" in parts:
        raise PackError(f"{path}: private songs are never packed into the firmware")
    if listed is not None and p not in listed:
        raise PackError(f"{path}: not in {LIBRARY}")
    return p


# ------------------------------------------------------------------------------------------------ the codec
def lz_pack(data, dic):
    """data -> the LZ77 stream of firmware/app/builtin.c, with dic as the history before data"""
    hist = bytes(dic) + bytes(data)
    base, n = len(dic), len(hist)
    heads = {}
    out = bytearray()

    def add(p):
        if p + 4 <= n:
            heads.setdefault(hist[p:p + 4], []).append(p)

    def emit(lits, mlen, off):
        ll = len(lits)
        out.append(min(ll, 15) << 4 | (min(mlen - 4, 15) if mlen else 0))
        if ll >= 15:
            r = ll - 15
            while r >= 255:
                out.append(255)
                r -= 255
            out.append(r)
        out.extend(lits)
        if mlen:
            out.append(off & 255)
            out.append(off >> 8)
            if mlen - 4 >= 15:
                r = mlen - 4 - 15
                while r >= 255:
                    out.append(255)
                    r -= 255
                out.append(r)

    for p in range(base):
        add(p)
    i = lit0 = base
    while i < n:
        best, bo = 0, 0
        if i + 4 <= n:
            for p in reversed(heads.get(hist[i:i + 4], [])[-256:]):
                if i - p > 65535:
                    break
                k = 4
                while i + k < n and hist[p + k] == hist[i + k]:
                    k += 1
                if k > best:
                    best, bo = k, i - p
        if best >= 4:
            emit(hist[lit0:i], best, bo)
            for k in range(i, i + best):
                add(k)
            i += best
            lit0 = i
        else:
            add(i)
            i += 1
    emit(hist[lit0:n], 0, 0)          # the last sequence: the literals left (none when the data ended in a match)
    return bytes(out)


def lz_unpack(dic, packed, rawlen):
    """the reference decoder (the same rules as builtin.c's bi_unpack) -> bytes; PackError on any violation"""
    ip, ie = 0, len(packed)
    out = bytearray()
    while ip < ie:
        t = packed[ip]
        ip += 1
        ll, ml = t >> 4, (t & 15) + 4
        if ll == 15:
            while True:
                if ip >= ie:
                    raise PackError("input ends in a literal length")
                b = packed[ip]
                ip += 1
                ll += b
                if b != 255:
                    break
        if ll > ie - ip:
            raise PackError("literals run past the input")
        out += packed[ip:ip + ll]
        ip += ll
        if ip == ie:
            break
        if ie - ip < 2:
            raise PackError("input ends in an offset")
        off = packed[ip] | packed[ip + 1] << 8
        ip += 2
        if ml == 19:
            while True:
                if ip >= ie:
                    raise PackError("input ends in a match length")
                b = packed[ip]
                ip += 1
                ml += b
                if b != 255:
                    break
        if off == 0 or off > len(dic) + len(out):
            raise PackError("bad offset")
        for _ in range(ml):
            p = len(dic) + len(out) - off
            out.append(dic[p] if p < len(dic) else out[p - len(dic)])
    if len(out) != rawlen:
        raise PackError(f"unpacked {len(out)} bytes, expected {rawlen}")
    return bytes(out)


def choose_dictionary(blobs):
    """a few whole files, greedily: each pick is the file that makes the packed total plus the dictionary smallest, by zlib's
    estimate with a preset dictionary (fast); stops at MAX_DICT_FILES or when a pick saves less than MIN_GAIN. Ties: the earlier file"""
    def cost(d):
        tot = len(d)
        for b in blobs:
            c = zlib.compressobj(9, zlib.DEFLATED, -15, 9, zlib.Z_DEFAULT_STRATEGY, d) if d else \
                zlib.compressobj(9, zlib.DEFLATED, -15)
            tot += len(c.compress(b) + c.flush())
        return tot
    d, chosen, now = b"", [], cost(b"")
    for _ in range(MAX_DICT_FILES):
        best = None
        for k, b in enumerate(blobs):
            if k in chosen or len(d) + len(b) > 32768:
                continue
            c = cost(d + b)
            if best is None or c < best[0]:
                best = (c, k)
        if best is None or now - best[0] < MIN_GAIN:
            break
        now, k = best
        chosen.append(k)
        d += blobs[k]
    return d, chosen


# ------------------------------------------------------------------------------------------------ the library
def source_key(folder):
    """a hash of everything a compile reads: the song folder's own files and the compiler's sources"""
    import hashlib
    h = hashlib.sha256()
    for f in sorted(p for p in folder.iterdir() if p.is_file()) + sorted((ROOT / "tools" / "fm1song").glob("*.py")):
        h.update(f.name.encode() + b"\0" + f.read_bytes() + b"\0")
    return h.hexdigest()


def compile_entry(path, cache):
    """the compiled file of an entry, the way `fm1song send` gets it (fm1song.cli._load), checked against a second way. cache: a dict
    {path: (source key, blob)}, kept between builds (the compile is the slow part: 0.2 s a song); a hit skips the two compiles but not the
    reader check, and a changed song or compiler source misses"""
    from fm1song import cli, compile_folder, read_compiled
    folder = ROOT / path
    if not (folder.is_dir() and (folder / "song.yaml").is_file() and (folder / "song.mid").is_file()):
        raise PackError(f"{path}: not a song folder with song.yaml and song.mid")
    key = source_key(folder)
    if path in cache and cache[path][0] == key:
        return cache[path][1], read_compiled(cache[path][1])
    compiled, blob, _ = cli._load(str(folder))
    if blob != compile_folder(folder)[0]:
        raise PackError(f"{path}: compiling twice, or by the two routes, gives different bytes")
    if read_compiled(blob) != compiled:
        raise PackError(f"{path}: fm1song's reader disagrees about the compiled file")
    cache[path] = (key, blob)
    return blob, compiled


def c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def c_bytes(b, per=24):
    return ",\n".join("    " + ", ".join(f"0x{x:02x}" for x in b[i:i + per]) for i in range(0, len(b), per))


KINDS = {"song": "SONG_KIND_SONG", "backtrack": "SONG_KIND_BACKTRACK"}
SCALES = {"major": "SONG_SCALE_MAJOR", "minor": "SONG_SCALE_MINOR", "blues": "SONG_SCALE_BLUES"}


def pack_library(entries, cache):
    blobs, metas, slugs = [], [], []
    for path in entries:
        blob, c = compile_entry(path, cache)
        if c["slug"] in slugs:
            raise PackError(f"{path}: the slug {c['slug']} is already in the library")
        slugs.append(c["slug"])
        if c["version"] < "2.0":
            raise PackError(f"{path}: format {c['version']}: the library holds 2.x files")
        tempo_us = c["tempo_us"]
        metas.append(dict(title=c["title"], key=c["key"], category=c["category"] or "",
                          bpm=(60000000 + tempo_us // 2) // tempo_us,       # player.c player_song_bpm
                          beats_per_bar=int(c["time_signature"].split("/")[0]), numerals=c["numerals"] or "",
                          kind=KINDS[c["kind"]], number=c["number"], scale=SCALES[c["scale"]], key_mode=c["key_mode"]))
        blobs.append(blob)
    dic, chosen = choose_dictionary(blobs)
    packs = []
    for path, blob in zip(entries, blobs):
        p = lz_pack(blob, dic)
        if lz_unpack(dic, p, len(blob)) != blob:
            raise PackError(f"{path}: pack -> unpack is not the file")
        packs.append(p)
    return blobs, metas, slugs, packs, dic, chosen


def write_header(out, entries, blobs, metas, slugs, packs, dic, chosen):
    big = max(len(b) for b in blobs)
    bmax = (big + RAW_STEP - 1) // RAW_STEP * RAW_STEP
    pack = b"".join(packs)
    lines = ["/* SPDX-License-Identifier: GPL-3.0-or-later */",
             "/* generated by tools/builtin_pack.py from songs/library.txt: do not edit (firmware/app/builtin.h) */",
             '#include "builtin.h"', '#include "song.h"', "",
             f"#define BUILTIN_N {len(blobs)}u              /* the library's songs and backtracks */",
             f"#define BUILTIN_MAX {bmax}u            /* the largest raw file ({big} B) rounded up to {RAW_STEP} */",
             f"#define BUILTIN_DICT_LEN {len(dic)}u", f"#define BUILTIN_PACK_LEN {len(pack)}u", "",
             f"/* the dictionary: {', '.join(entries[k] for k in chosen)} */",
             "static const uint8_t bi_dict[BUILTIN_DICT_LEN] = {", c_bytes(dic), "};",
             "static const uint8_t bi_pack[BUILTIN_PACK_LEN] = {", c_bytes(pack), "};",
             "static const struct bi_file bi_files[BUILTIN_N] = {"]
    off = 0
    for s, b, p in zip(slugs, blobs, packs):
        lines.append(f"    {{{c_str(s)}, {off}u, {len(p)}u, {len(b)}u, 0x{zlib.crc32(b):08x}u}},")
        off += len(p)
    lines += ["};", "/* the list's metadata, in struct lib_item's order (title, key, category, bpm, beats a bar, numerals, kind, number, scale, key mode) */",
              "static const struct lib_item bi_items[BUILTIN_N] = {"]
    for m in metas:
        lines.append(f"    {{{c_str(m['title'])}, {c_str(m['key'])}, {c_str(m['category'])}, {m['bpm']}u, {m['beats_per_bar']}u, "
                     f"{c_str(m['numerals'])}, {m['kind']}, {m['number']}, {m['scale']}, {m['key_mode']}}},")
    lines += ["};", ""]
    Path(out).write_text("\n".join(lines))
    return big, bmax, len(pack)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out", nargs="?")
    ap.add_argument("--check", metavar="PATH")
    args = ap.parse_args()
    try:
        entries = library_entries()
        if args.check:
            check_path(args.check, entries)
            print(f"{args.check}: may be packed")
            return 0
        if not args.out:
            ap.error("OUT.h is required")
        import pickle
        cache_file = Path(args.out).with_suffix(".cache")
        try:
            cache = pickle.loads(cache_file.read_bytes())
        except Exception:
            cache = {}
        blobs, metas, slugs, packs, dic, chosen = pack_library(entries, cache)
        cache_file.write_bytes(pickle.dumps({p: cache[p] for p in entries}))
        big, bmax, plen = write_header(args.out, entries, blobs, metas, slugs, packs, dic, chosen)
    except PackError as e:
        print(f"builtin_pack: {e}", file=sys.stderr)
        return 1
    print(f"builtin_pack: {len(blobs)} files, {sum(map(len, blobs))} B raw (largest {big} B -> BUILTIN_MAX {bmax}), "
          f"{plen} B packed + {len(dic)} B dictionary ({len(chosen)} files) = {plen + len(dic)} B")
    return 0


if __name__ == "__main__":
    sys.exit(main())
