# fm1song: chord symbols and keyboard voicings.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# A chord symbol is root [A-G] + optional # or b, a quality from QUALITIES, and an optional "/bass".
# "N.C." means no chord. Voicings are what the keyboard view shows (and what our generated harmony plays):
#   level 1: root position, root between D3 and C#4
#   level 2: the inversion closest to the previous level-2 voicing (smooth voice leading)
import re

NOTE_NAMES = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
PITCH = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
NC = "N.C."
MAX_SYMBOL = 7          # characters; the compiled chord table has 8 bytes with a NUL
MAX_NOTES = 5           # per voicing in the compiled file
LEVELS = 2              # voicing levels in the compiled file (harmony variant 3 shows level 2)

# quality -> semitones above the root. Longest match wins when parsing.
QUALITIES = {
    "": (0, 4, 7), "m": (0, 3, 7), "dim": (0, 3, 6), "aug": (0, 4, 8), "+": (0, 4, 8),
    "sus2": (0, 2, 7), "sus4": (0, 5, 7), "sus": (0, 5, 7),
    "5": (0, 7),
    "6": (0, 4, 7, 9), "m6": (0, 3, 7, 9),
    "7": (0, 4, 7, 10), "maj7": (0, 4, 7, 11), "m7": (0, 3, 7, 10), "mmaj7": (0, 3, 7, 11),
    "m7b5": (0, 3, 6, 10), "dim7": (0, 3, 6, 9), "7sus4": (0, 5, 7, 10),
    "9": (0, 4, 7, 10, 14), "maj9": (0, 4, 7, 11, 14), "m9": (0, 3, 7, 10, 14), "add9": (0, 4, 7, 14),
}
_SYMBOL = re.compile(r"^([A-G])([#b]?)(.*?)(?:/([A-G])([#b]?))?$")


class Chord:
    __slots__ = ("symbol", "root", "bass", "intervals")

    def __init__(self, symbol, root, bass, intervals):
        self.symbol, self.root, self.bass, self.intervals = symbol, root, bass, intervals

    @property
    def pitch_classes(self):
        return sorted({(self.root + i) % 12 for i in self.intervals} | ({self.bass} if self.bass is not None else set()))

    @property
    def mask(self):
        return sum(1 << pc for pc in self.pitch_classes)


def _pc(letter, accidental):
    return (PITCH[letter] + {"#": 1, "b": -1, "": 0}[accidental]) % 12


def parse_chord(symbol):
    """'F#m7/C#' -> Chord; None for 'N.C.'; raises ValueError with a readable reason"""
    if symbol == NC:
        return None
    if len(symbol) > MAX_SYMBOL:
        raise ValueError(f"chord {symbol!r} is longer than {MAX_SYMBOL} characters")
    m = _SYMBOL.match(symbol)
    if not m:
        raise ValueError(f"{symbol!r} is not a chord symbol (expected e.g. G, Em, D7, F#m7b5, C/E or N.C.)")
    root = _pc(m.group(1), m.group(2))
    quality = m.group(3)
    if quality not in QUALITIES:
        raise ValueError(f"chord {symbol!r}: unknown quality {quality!r} "
                         f"(known: {', '.join(repr(q) for q in QUALITIES if q)})")
    bass = _pc(m.group(4), m.group(5)) if m.group(4) else None
    return Chord(symbol, root, bass, QUALITIES[quality])


def voicing_level1(chord):
    """root position, root in D3..C#4 (MIDI 50..61), at most MAX_NOTES notes"""
    root = 48 + chord.root if 48 + chord.root >= 50 else 60 + chord.root
    return [root + i for i in chord.intervals][:MAX_NOTES]


def _closed_inversions(chord):
    """every closed-position inversion with its lowest note in E3..D#4 (MIDI 52..63)"""
    pcs = sorted({(chord.root + i) % 12 for i in chord.intervals[:4]})   # 9ths dropped in inversions
    out = []
    for low in range(52, 64):
        if low % 12 in pcs:
            start = pcs.index(low % 12)
            notes, last = [], None
            for k in range(len(pcs)):
                pc = pcs[(start + k) % len(pcs)]
                n = low if last is None else last + ((pc - last) % 12 or 12)
                notes.append(n)
                last = n
            out.append(notes)
    return out


def voicing_level2(chord, previous):
    """the inversion that moves least from `previous` (a level-2 voicing or None)"""
    options = _closed_inversions(chord)
    if previous is None:
        target = voicing_level1(chord)
        return min(options, key=lambda v: (abs(sum(v) / len(v) - sum(target) / len(target)), v))

    def cost(v):
        a, b = sorted(v), sorted(previous)
        # pair voices from the bottom; extra voices pair with the nearest note
        return sum(min(abs(x - y) for y in b) for x in a) + sum(min(abs(x - y) for x in a) for y in b)
    return min(options, key=lambda v: (cost(v), v))


def voicings(chords):
    """[Chord or None] in song order -> [(level1, level2)] (empty lists for N.C.)"""
    out, prev = [], None
    for c in chords:
        if c is None:
            out.append(([], []))
            continue
        v2 = voicing_level2(c, prev)
        prev = v2
        out.append((voicing_level1(c), v2))
    return out


def note_name(n):
    return f"{NOTE_NAMES[n % 12]}{n // 12 - 1}"
