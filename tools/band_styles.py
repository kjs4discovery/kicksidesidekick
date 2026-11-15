#!/usr/bin/env python3
# band_styles.py -- M9 (D052): tools/band_styles.txt (the band's tier tables, edited by ear) -> BOTH
#   firmware/app/band_styles.h              the C tables (gen.c)
#   tools/fm1song/band_styles_data.py       the same data for tools/fm1song/arrange.py
# so the FM-1 and the laptop cannot drift. A bad table is refused with its line number.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   .venv/bin/python -I tools/band_styles.py            write both files
#   .venv/bin/python -I tools/band_styles.py --check    exit 1 if either file is not what the table makes (pytest too)
#
# The format is described at the top of tools/band_styles.txt (docs/plan-M9.md, Part 3).
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TABLE = ROOT / "tools/band_styles.txt"
OUT_C = ROOT / "firmware/app/band_styles.h"
OUT_PY = ROOT / "tools/fm1song/band_styles_data.py"

# the style list on the FM-1 (M8's 11, in M8's order): name in the table -> (STYL index of song.h, arrange.py's name,
# beats a bar). WALTZ is 3/4, the others 4/4
STYLES = {
    "POP": (4, "pop", 4), "ROCK": (6, "rock", 4), "INDIE POP": (7, "indie", 4), "BALLAD": (1, "ballad", 4),
    "SHUFFLE": (2, "shuffle", 4), "STRIDE": (3, "march", 4), "WALTZ": (0, "waltz", 3), "COUNTRY": (5, "train", 4),
    "BOSSA NOVA": (8, "bossa", 4), "FUNK": (9, "funk", 4), "REGGAE": (10, "reggae", 4),
}
DRUM_ROWS = "KSHORCYT"                      # kick 36, snare 38, closed hat 42, open hat 46, rim 37, clap 39, ride 51, tom lo 45
VOICINGS = ("CLOSE", "WIDE", "UPPER")
HARM_CHARS = set("xoglmh-.")
BASS_CHARS = set("RFOTNArfotna-.")
DRUM_CHARS = set("xog.")


class TableError(Exception):
    pass


def parse(text):
    """-> {table name: {"steps", "swing", "beats", "tiers": {1..4: {"voicing", "harm": [bar, bar], "bass": [bar, bar],
    "drums": {row: [bar, bar]}}}}}; a line with one bar gets it twice (both alternating bars)"""
    styles, cur, tier, tier_line = {}, None, None, 0

    def bad(n, msg):
        raise TableError(f"{TABLE.name}:{n}: {msg}")

    def bars(n, body, kind):
        parts = [p.strip() for p in body.split(" | ")] if " | " in body else [body.strip()]
        if len(parts) > 2:
            bad(n, f"{len(parts)} bars: one, or two separated by ' | '")
        want = cur["beats"] * cur["steps"]
        ok = {"harm": HARM_CHARS, "bass": BASS_CHARS, "drum": DRUM_CHARS}[kind]
        for p in parts:
            if len(p) != want:
                bad(n, f"a bar of {len(p)} characters, {want} wanted ({cur['name']}: {cur['beats']} beats x "
                       f"{cur['steps']} steps)")
            for ch in p:
                if ch not in ok:
                    bad(n, f"unknown character {ch!r} in {p!r}")
        return parts * 2 if len(parts) == 1 else parts

    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.rstrip()
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        m = re.fullmatch(r"STYLE\s+(.+?)\s+steps\s+(\d+)(\s+swing)?", line)
        if m:
            if cur:
                _close(cur, bad)
            name = m.group(1)
            if name not in STYLES:
                bad(n, f"style {name!r}: not one of the FM-1's 11 ({', '.join(STYLES)})")
            if name in styles:
                bad(n, f"style {name!r} twice")
            if int(m.group(2)) not in (3, 4):
                bad(n, f"steps {m.group(2)}: 3 or 4")
            cur = dict(name=name, line=n, steps=int(m.group(2)), swing=bool(m.group(3)), beats=STYLES[name][2], tiers={})
            styles[name] = cur
            tier = None
            continue
        if cur is None:
            bad(n, "a line before the first STYLE")
        m = re.fullmatch(r"TIER\s+(\d)", line)
        if m:
            k = int(m.group(1))
            if k != len(cur["tiers"]) + 1 or not 1 <= k <= 4:
                bad(n, f"TIER {k}: the tiers come in order 1..4")
            tier = cur["tiers"][k] = dict(voicing=None, harm=None, bass=None, drums={}, line=n)
            continue
        if tier is None:
            bad(n, "a layer line before the first TIER")
        m = re.fullmatch(r"HARM\s+(\S+)\s+(.+)", line)
        if m:
            if tier["harm"]:
                bad(n, "HARM twice in a tier")
            if m.group(1) not in VOICINGS:
                bad(n, f"voicing {m.group(1)!r}: one of {', '.join(VOICINGS)}")
            tier["voicing"], tier["harm"] = m.group(1), bars(n, m.group(2), "harm")
            continue
        m = re.fullmatch(r"BASS\s+(.+)", line)
        if m:
            if tier["bass"]:
                bad(n, "BASS twice in a tier")
            tier["bass"] = bars(n, m.group(1), "bass")
            continue
        m = re.fullmatch(r"([KSHORCYT])\s+(.+)", line)
        if m:
            if m.group(1) in tier["drums"]:
                bad(n, f"drum row {m.group(1)} twice in a tier")
            tier["drums"][m.group(1)] = bars(n, m.group(2), "drum")
            continue
        bad(n, f"cannot read {line!r}")
    if cur:
        _close(cur, bad)
    for name in STYLES:
        if name not in styles:
            raise TableError(f"{TABLE.name}: style {name!r} is missing")
    return styles


def _close(style, bad):
    for k in range(1, 5):
        t = style["tiers"].get(k)
        if t is None:
            bad(style["line"], f"{style['name']}: TIER {k} is missing")
        if not t["harm"] or not t["bass"]:
            bad(t["line"], f"{style['name']} TIER {k}: needs a HARM and a BASS line")


# ------------------------------------------------------------------------------------------------ the outputs
HEAD = "generated by tools/band_styles.py, do not edit"


def c_text(styles):
    q = lambda s: '"' + s + '"'
    out = ["/* SPDX-License-Identifier: GPL-3.0-or-later */", f"/* {HEAD} (from tools/band_styles.txt) */", "#pragma once", "#include <stdint.h>", "",
           "/* the band's tier tables for backtracks (M9, D052), by song.h's STYL index. A line is one bar, or two (the odd",
           " * bars of a section the first, the even the second; a line of one bar holds it twice). Drum rows: K S H O R C Y T",
           " * (36 38 42 46 37 39 51 45), 0: silent. See tools/band_styles.txt for the characters. */",
           "enum { BV_CLOSE, BV_WIDE, BV_UPPER };",
           "struct band_tier {", "    uint8_t voicing;", "    const char *harm[2];", "    const char *bass[2];",
           "    const char *drum[8][2];", "};",
           "struct band_style {", "    uint8_t steps;                      /* a beat's steps: 4 sixteenths, 3 triplet eighths */",
           "    uint8_t beats;                      /* the metre the table is written for */",
           "    struct band_tier tier[4];", "};",
           f"#define BAND_STYLES_N {len(STYLES)}u",
           "static const struct band_style BAND_STYLES[BAND_STYLES_N] = {"]
    for name, (idx, _, _) in sorted(STYLES.items(), key=lambda kv: kv[1][0]):
        st = styles[name]
        out.append(f"    [{idx}] = {{ /* {name} */ {st['steps']}, {st['beats']}, {{")
        for k in range(1, 5):
            t = st["tiers"][k]
            drums = ", ".join(f"{{{q(t['drums'][r][0])}, {q(t['drums'][r][1])}}}" if r in t["drums"] else "{0, 0}"
                              for r in DRUM_ROWS)
            out.append(f"        {{ BV_{t['voicing']}, {{{q(t['harm'][0])}, {q(t['harm'][1])}}}, "
                       f"{{{q(t['bass'][0])}, {q(t['bass'][1])}}},")
            out.append(f"          {{{drums}}} }},")
        out.append("    } },")
    out.append("};")
    return "\n".join(out) + "\n"


def py_text(styles):
    out = ["# SPDX-License-Identifier: GPL-3.0-or-later", f"# {HEAD} (from tools/band_styles.txt)", "# the band's tier tables for backtracks (M9, D052), by arrange.py's style name: steps a beat, beats a bar,",
           "# and per tier 1..4 the voicing, the harmony and bass bars and the drum rows ([bar, bar]: the odd bars of a section",
           "# the first, the even the second). Same data as firmware/app/band_styles.h.",
           f"DRUM_ROWS = {DRUM_ROWS!r}", "BAND_STYLES = {"]
    for name, (_, py, _) in sorted(STYLES.items(), key=lambda kv: kv[1][0]):
        st = styles[name]
        out.append(f"    {py!r}: {{'name': {name!r}, 'steps': {st['steps']}, 'beats': {st['beats']}, 'tiers': {{")
        for k in range(1, 5):
            t = st["tiers"][k]
            out.append(f"        {k}: {{'voicing': {t['voicing']!r}, 'harm': {t['harm']!r}, 'bass': {t['bass']!r},")
            out.append(f"            'drums': {{{', '.join(f'{r!r}: {t['drums'][r]!r}' for r in DRUM_ROWS if r in t['drums'])}}}}},")
        out.append("    }},")
    out.append("}")
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="exit 1 if the generated files are not up to date")
    a = ap.parse_args()
    try:
        styles = parse(TABLE.read_text())
    except TableError as e:
        sys.exit(f"band_styles: {e}")
    want = {OUT_C: c_text(styles), OUT_PY: py_text(styles)}
    if a.check:
        stale = [str(p.relative_to(ROOT)) for p, t in want.items() if not p.exists() or p.read_text() != t]
        if stale:
            sys.exit(f"band_styles: not up to date: {', '.join(stale)} (run tools/band_styles.py)")
        print("band_styles: up to date")
        return
    for p, t in want.items():
        p.write_text(t)
        print(f"wrote {p.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
