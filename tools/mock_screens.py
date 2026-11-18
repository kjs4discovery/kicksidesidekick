#!/usr/bin/env python3
# mock_screens.py -- design mock-ups of new screens for docs/controls.md (drawn before the firmware draws them), with
# the firmware's fonts (fm1-base's Barlow) and colours (the AMBER palette).
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   mock_screens.py OUT_DIR      -> OUT_DIR/mock-*.png at 2x (480 x 480)
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
FONTS = ROOT / "firmware/vendor/fm1-base/assets/fonts"
S = 2
BLACK, WHITE = (0, 0, 0), (255, 255, 255)
LINE, DIM, GRAY, AMB, HI = (34, 34, 40), (74, 74, 82), (150, 150, 158), (232, 136, 30), (255, 184, 70)
CREAM = (250, 238, 200)


def font(px, w="SemiBold"):
    return ImageFont.truetype(str(FONTS / f"BarlowSemiCondensed-{w}.ttf"), px * S)


XS, B, M, L = font(13, "Medium"), font(17, "Bold"), font(23), font(31, "Bold")


class Screen:
    def __init__(self):
        self.im = Image.new("RGB", (240 * S, 240 * S), BLACK)
        self.d = ImageDraw.Draw(self.im)

    def rect(self, x, y, w, h, c, fill=True):
        box = (x * S, y * S, (x + w) * S - 1, (y + h) * S - 1)
        self.d.rectangle(box, fill=c) if fill else self.d.rectangle(box, outline=c, width=S)

    def text(self, x, y, s, f, c, center_w=None, right=None):
        w = self.d.textlength(s, font=f) / S
        if center_w is not None:
            x += (center_w - w) / 2
        if right is not None:
            x = right - w
        self.d.text((x * S, y * S), s, font=f, fill=c)

    def save(self, path):
        self.im.save(path)


def play_top(s, bar, bpm, own, flags):
    s.text(6, 1, "Twelve-Bar Blues in C", XS, GRAY)
    s.text(6, 16, f"{bpm} BPM  C  " + flags, XS, AMB if not own else DIM)
    s.text(0, 3, bar, M, WHITE, right=234)


def strip(s, y, bars, cur, loop=None, start=None):
    for i, (n, ch) in enumerate(bars):
        x = 2 + i * 60
        inloop = loop and loop[0] <= n <= loop[1]
        s.rect(x, y, 56, 44, HI if i == cur else DIM, fill=False)
        s.text(x + 4, y + 1, str(n), XS, DIM)
        s.text(x, y + 16, ch, M, WHITE if i == cur else AMB, center_w=56)
        if inloop:
            s.rect(x, y + 46, 56 if n != loop[1] else 56, 3, AMB)
        if start == n:
            s.text(x + 44, y + 1, "A", XS, HI)


def layers(s, y, states):
    names = ["DRUMS", "BASS", "HARM", "MELODY"]
    for i, (nm, st) in enumerate(zip(names, states)):
        x = 4 + i * 59
        if st == "MUTE":
            s.rect(x, y, 55, 30, AMB, fill=False)
        else:
            s.rect(x, y, 55, 30, LINE)
        s.text(x, y + 1, nm, XS, AMB if st == "MUTE" else WHITE, center_w=55)
        s.text(x, y + 15, st, XS, AMB if st == "MUTE" else HI, center_w=55)


def legend(s, text):
    s.rect(0, 216, 240, 1, LINE)
    s.text(4, 221, text, XS, DIM)


def mock_stopped(path):
    s = Screen()
    play_top(s, "STOP", 80, False, "CNT  LOOP 5-8")
    s.text(0, 40, "Twelve-Bar Blues in C", XS, GRAY, center_w=176)
    s.text(0, 58, "PRESS PLAY", M, WHITE, center_w=176)
    s.text(0, 86, "from bar 5 (verse 1)", XS, DIM, center_w=176)
    s.rect(176, 38, 1, 64, LINE)
    s.text(184, 36, "FIRST", XS, DIM)
    s.text(184, 52, "F7", M, AMB)
    strip(s, 108, [(5, "F7"), (6, "F7"), (7, "C7"), (8, "C7")], 0, loop=(5, 8), start=5)
    layers(s, 162, ["L2", "L1", "MUTE", "L1"])
    s.text(6, 197, "Chorus 1", XS, GRAY)
    s.text(0, 197, "LOOP BARS 5-8", XS, HI, right=234)
    legend(s, "PLAY start  PRESETS/ALGO: A/B  SEQ  GLO ?")
    s.save(path)


def mock_playing(path):
    s = Screen()
    play_top(s, "6:3", 96, True, "CNT  LOOP SECT")
    s.text(0, 30, "", XS, GRAY)
    s.text(60, 38, "F7", L, HI)
    s.rect(176, 38, 1, 64, LINE)
    s.text(184, 36, "NEXT", XS, DIM)
    s.text(184, 52, "C7", M, AMB)
    s.text(184, 78, "IN 2", M, WHITE)
    strip(s, 108, [(6, "F7"), (7, "C7"), (8, "C7"), (9, "G7")], 0, loop=(1, 12))
    layers(s, 162, ["L1", "L2", "MUTE", "L1"])
    s.text(6, 197, "Chorus 1", XS, GRAY)
    s.text(0, 197, "LOOP SECTION", XS, HI, right=234)
    legend(s, "PLAY pause  HOME stop  SEQ loop  GLO help")
    s.save(path)


def mock_paused(path):
    s = Screen()
    play_top(s, "PAUSE", 96, True, "CNT")
    s.text(0, 40, "PAUSED at 6:3", M, WHITE, center_w=176)
    s.text(0, 72, "PLAY: go on (count-in, from bar 6)", XS, DIM, center_w=176)
    s.text(0, 86, "HOME: stop, back to bar 1", XS, DIM, center_w=176)
    s.rect(176, 38, 1, 64, LINE)
    s.text(184, 36, "NEXT", XS, DIM)
    s.text(184, 52, "C7", M, AMB)
    strip(s, 108, [(6, "F7"), (7, "C7"), (8, "C7"), (9, "G7")], 0)
    layers(s, 162, ["L1", "L2", "MUTE", "L1"])
    s.text(6, 197, "Chorus 1", XS, GRAY)
    legend(s, "PLAY go on  HOME stop  SEQ loop  GLO help")
    s.save(path)


def mock_help(path):
    s = Screen()
    s.text(6, 2, "CONTROLS", B, WHITE)
    s.text(0, 6, "GLO: close", XS, DIM, right=234)
    s.rect(0, 24, 240, 1, LINE)
    left = [("MASTER", "volume"), ("SELECT", "tempo (BPM)"), ("PRESETS", "start: section / A"),
            ("ALGORITHM", "start: bar / B"), ("OCT- OCT+", "keys' octave")]
    for i, (k, v) in enumerate(left):
        s.text(6, 30 + i * 28, k, XS, GRAY)
        s.text(6, 43 + i * 28, v, XS, HI)
    s.text(124, 30, "K1..K4", XS, GRAY)
    s.text(124, 43, "levels (L1, L2)", XS, HI)
    grid = [["FX", "SEL", "ENV", "LFO", "EDIT", "GLO"], ["HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"]]
    what = [["drum", "bass", "harm", "melo", "-", "help"], ["stop", "-", "count", "loop", "play", "-"]]
    for r in range(2):
        for c in range(6):
            x, y = 124 + c * 19, 64 + r * 40
            s.rect(x, y, 18, 36, LINE)
            s.text(x, y + 2, grid[r][c][:4], font(8, "Medium"), GRAY, center_w=18)
            s.text(x, y + 18, what[r][c], font(8, "Medium"), HI, center_w=18)
    s.text(124, 150, "FX..LFO: mute a layer", XS, DIM)
    s.text(124, 164, "the keys: play along", XS, DIM)
    s.rect(0, 200, 240, 1, LINE)
    s.text(6, 206, "CPU 9%   voices 8/24", XS, DIM)
    s.text(0, 206, "v0.3.0-m2", XS, DIM, right=234)
    s.text(6, 222, "FM-1 Accompaniment", XS, DIM)
    s.save(path)


def mock_splash(path):
    s = Screen()
    s.rect(0, 0, 240, 240, CREAM)
    logo = Image.open(ROOT / "firmware/assets/src/kickside-logo.jpg").convert("RGB").resize((150 * S, 100 * S))   # the placeholder tmp-logo.jpg is gone
    s.im.paste(logo, (45 * S, 14 * S))
    s.text(0, 190, "FM-1 Accompaniment", B, (60, 50, 40), center_w=240)
    s.text(0, 212, "v0.3.0-m2", XS, (150, 136, 118), center_w=240)
    s.save(path)


def keyboard(s, x0, y0, lo, hi, lit, dim=(), bass=()):
    """a keyboard from MIDI note lo to hi (whites 11 px), lit notes HI, dim ones outlined, bass AMB"""
    whites = [n for n in range(lo, hi + 1) if n % 12 in (0, 2, 4, 5, 7, 9, 11)]
    w = 11
    for i, n in enumerate(whites):
        c = HI if n in lit else AMB if n in bass else (220, 220, 225)
        s.rect(x0 + i * w, y0, w - 1, 60, c)
        if n in dim and n not in lit:
            s.rect(x0 + i * w + 2, y0 + 44, w - 5, 12, AMB, fill=False)
    for i, n in enumerate(whites):
        if n % 12 in (0, 2, 5, 7, 9) and n + 1 <= hi:
            b = n + 1
            c = HI if b in lit else AMB if b in bass else (40, 40, 46)
            s.rect(x0 + i * w + 7, y0, 7, 36, c)
            if b in dim and b not in lit:
                s.rect(x0 + i * w + 8, y0 + 24, 5, 10, AMB, fill=False)


def note_icon(s, x, y, kind, c):
    """a note symbol: whole, half, quarter, eighth (drawn, there is no music font)"""
    d = s.d
    head = [(x) * S, (y + 22) * S, (x + 11) * S, (y + 30) * S]
    if kind in ("whole", "half"):
        d.ellipse(head, outline=c, width=2 * S)
    else:
        d.ellipse(head, fill=c)
    if kind != "whole":
        s.rect(x + 9, y, 2, 26, c)
    if kind == "eighth":
        d.line([((x + 10) * S, y * S), ((x + 17) * S, (y + 9) * S)], fill=c, width=2 * S)


def mock_keyboard_view(path):
    s = Screen()
    play_top(s, "7:2", 96, True, "CNT  KEY D (+2)")
    s.text(6, 36, "D7", L, HI)
    s.text(80, 44, "next G7  IN 3", B, AMB)
    s.text(6, 74, "harmony L1: play these keys", XS, DIM)
    keyboard(s, 4, 92, 48, 83, lit={50, 54, 57, 60}, dim={55, 59, 62, 65}, bass=set())
    layers(s, 162, ["L1", "L1", "MUTE", "L1"])
    s.text(6, 197, "Chorus 1", XS, GRAY)
    legend(s, "EDIT: VIEW  SAVE: KEY  PLAY: PAUSE  GLO: ?")
    s.save(path)


def mock_chart_view(path):
    s = Screen()
    play_top(s, "7:2", 96, True, "CNT")
    s.text(6, 33, "Chorus 1  (bars 1-12)", XS, GRAY)
    chords = ["C7", "F7", "C7", "C7", "F7", "F7", "C7", "C7", "G7", "F7", "C7", "G7"]
    for i, ch in enumerate(chords):
        x, y = 2 + (i % 4) * 60, 50 + (i // 4) * 36
        cur = i == 6
        s.rect(x, y, 56, 33, HI if cur else DIM, fill=False)
        s.text(x + 3, y + 1, str(i + 1), font(10, "Medium"), DIM)
        s.text(x, y + 8, ch, B, WHITE if cur else AMB, center_w=56)
    layers(s, 162, ["L1", "L1", "MUTE", "L1"])
    s.text(6, 197, "BASS: C2  then E2", XS, HI)
    legend(s, "EDIT: VIEW  SAVE: KEY  PLAY: PAUSE  GLO: ?")
    s.save(path)


def mock_chord_cue(path):
    s = Screen()
    play_top(s, "7:2", 96, True, "CNT")
    note_icon(s, 10, 44, "quarter", HI)
    s.text(8, 80, "L2", XS, DIM)
    s.text(70, 38, "C7", L, HI)
    s.rect(176, 38, 1, 64, LINE)
    s.text(184, 36, "NEXT", XS, DIM)
    s.text(184, 52, "G7", M, AMB)
    s.text(184, 78, "IN 2", M, WHITE)
    strip(s, 108, [(7, "C7"), (8, "C7"), (9, "G7"), (10, "F7")], 0)
    layers(s, 162, ["L1", "MUTE", "MUTE", "L1"])
    s.text(6, 197, "BASS: C2  then E2", XS, HI)
    legend(s, "EDIT: VIEW  SAVE: KEY  PLAY: PAUSE  GLO: ?")
    s.save(path)


def mock_key_mode(path):
    s = Screen()
    s.text(6, 1, "Twelve-Bar Blues in C", XS, GRAY)
    s.rect(4, 15, 120, 16, HI)
    s.text(8, 16, "KEY: D (+2)  SELECT", XS, BLACK)
    s.text(0, 3, "STOP", M, GRAY, right=234)
    s.text(0, 40, "Twelve-Bar Blues in C", XS, GRAY, center_w=176)
    s.text(0, 58, "PRESS PLAY", M, WHITE, center_w=176)
    s.text(0, 86, "in D (2 up)", XS, DIM, center_w=176)
    s.rect(176, 38, 1, 64, LINE)
    s.text(184, 36, "FIRST", XS, DIM)
    s.text(184, 52, "D7", M, AMB)
    strip(s, 108, [(1, "D7"), (2, "G7"), (3, "D7"), (4, "D7")], 0)
    layers(s, 162, ["L1", "L1", "L1", "L1"])
    legend(s, "SELECT: KEY   SAVE: DONE")
    s.save(path)


if __name__ == "__main__":
    out = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "docs/step-M2")
    out.mkdir(parents=True, exist_ok=True)
    for name, fn in (("stopped-loop-bars", mock_stopped), ("playing", mock_playing), ("paused", mock_paused),
                     ("help", mock_help), ("splash", mock_splash), ("keyboard-view", mock_keyboard_view),
                     ("chart-view", mock_chart_view), ("chord-cue", mock_chord_cue), ("key-mode", mock_key_mode)):
        fn(out / f"mock-{name}.png")
    print(f"mock_screens: {out}/mock-*.png")
