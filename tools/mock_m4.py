#!/usr/bin/env python3
# mock_m4.py -- design mock-ups of M4's new screens for docs/controls.md (drawn before the firmware draws them), with
# tools/mock_screens.py's and mock_m3.py's pieces.
# SPDX-License-Identifier: GPL-3.0-or-later
#
#   mock_m4.py [OUT_DIR [NAME...]]      -> OUT_DIR/mock-*.png at 2x (480 x 480); default docs/step-M4
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mock_screens import AMB, B, BLACK, DIM, GRAY, HI, L, LINE, M, ROOT, S, WHITE, XS, Screen, strip  # noqa: E402
from mock_m3 import big_band, legend2, top  # noqa: E402

QUIET_BG = (52, 46, 30)


def layers4(s, states, yours, y=180):
    """M4 layer boxes: name, then level + FULL / QUIET / MUTE; your part: an amber bar on top and a double frame"""
    names = ["DRUMS", "BASS", "HARM", "MELODY"]
    for i, (nm, (lvl, vol)) in enumerate(zip(names, states)):
        x = 4 + i * 59
        if vol == "MUTE":
            s.rect(x, y, 55, 30, AMB, fill=False)
        else:
            s.rect(x, y, 55, 30, QUIET_BG if vol == "QUIET" else LINE)
        if i == yours:
            s.rect(x, y - 3, 55, 3, HI)
            s.rect(x + 1, y + 1, 53, 28, HI, fill=False)
        s.text(x, y + 1, nm, XS, HI if i == yours else WHITE if vol != "MUTE" else AMB, center_w=55)
        s.text(x, y + 15, f"{lvl} {vol}" if vol != "MUTE" else "MUTE", XS,
               AMB if vol == "MUTE" else GRAY if vol == "QUIET" else HI, center_w=55)


def mock_yours(path):
    s = Screen()
    top(s, "Amazing Grace", "84 BPM  G", "5:2", "BASS - YOURS")
    big_band(s, "G2", "C3", "chord G7", "half")
    s.rect(2, 108, 236, 44, BLACK)
    for i, (n, nm) in enumerate([(5, "G2"), (6, "C3"), (7, "G2 D3"), (8, "G2")]):
        x = 2 + i * 60
        s.rect(x, 108, 56, 44, HI if i == 0 else DIM, fill=False)
        s.text(x + 4, 109, str(n), XS, DIM)
        s.text(x, 126, nm, B, WHITE if i == 0 else AMB, center_w=56)
    s.text(6, 160, "hold a layer button: your part   press: FULL > QUIET > MUTE", XS, DIM)
    layers4(s, [("L1", "FULL"), ("L1", "QUIET"), ("L2", "MUTE"), ("L1", "FULL")], 1)
    legend2(s, [("PLAY", "pause"), ("EDIT", "view"), ("SEQ", "loop"), ("GLO", "help")])
    s.save(path)


def staff(s, top, sp, x0=4, x1=236):
    for i in range(5):
        s.rect(x0, top + i * sp, x1 - x0, 1, GRAY)


def head(s, x, y, c, hollow=False):
    box = [(x - 6) * S, (y - 4) * S, (x + 6) * S, (y + 4) * S]
    s.d.ellipse(box, outline=c, width=2 * S) if hollow else s.d.ellipse(box, fill=c)


def mock_harm_notation(path):
    s = Screen()
    top(s, "Amazing Grace", "84 BPM  G", "5:2", "HARM - YOURS")
    t, b, sp = 56, 110, 9                       # the treble staff's top, the bass staff's top (screen rows)
    staff(s, t, sp)
    staff(s, b, sp)
    s.rect(4, t, 2, b + 4 * sp - t, GRAY)
    s.text(18, t - 2, "G", L, WHITE)
    s.text(16, b - 6, "F", M, WHITE)
    s.rect(84, t - 8, 2, b + 4 * sp - t + 16, AMB)
    s.text(60, 34, "G", B, HI)
    s.text(150, 34, "C", B, WHITE)
    for x, notes, c in ((96, (t + 4 * sp - 3 * sp // 2, t + 4 * sp - 5 * sp // 2), AMB),
                        (160, (t + 4 * sp - sp, t + 4 * sp - 2 * sp, t + 4 * sp - 3 * sp), WHITE)):
        for y in notes:
            head(s, x, y, c, hollow=True)
        s.rect(x + 5, min(notes) - 22, 2, max(notes) - min(notes) + 22, c)
    head(s, 96, b + sp, AMB, hollow=True)
    s.rect(90, b + sp, 2, 24, AMB)
    s.text(6, 160, "harmony L1: chords stacked, treble + bass", XS, GRAY)
    legend2(s, [("PLAY", "pause"), ("EDIT", "view"), ("SEQ", "loop"), ("GLO", "help")])
    s.save(path)


def mock_drum_notation(path):
    s = Screen()
    top(s, "Saints", "112 BPM  F", "3:2", "DRUMS - YOURS")
    t, sp = 66, 12
    staff(s, t, sp)
    s.rect(10, t + sp, 3, 2 * sp, WHITE)
    s.rect(16, t + sp, 3, 2 * sp, WHITE)
    s.rect(84, t - 10, 2, 4 * sp + 22, AMB)
    bot = t + 4 * sp
    for k in range(6):
        x = 96 + k * 26
        c = AMB if k == 0 else WHITE
        yh = bot - 9 * sp // 2
        s.d.line([(x - 4) * S, (yh - 4) * S, (x + 4) * S, (yh + 4) * S], fill=c, width=2 * S)
        s.d.line([(x - 4) * S, (yh + 4) * S, (x + 4) * S, (yh - 4) * S], fill=c, width=2 * S)
        if k % 2:
            head(s, x, bot - 5 * sp // 2, c)
        else:
            head(s, x, bot - sp // 2, c)
            s.rect(x - 6, bot - sp // 2, 2, 24, c)
        s.rect(x + 5, yh - 24, 2, 24 + (5 * sp // 2 if k % 2 else 0), c)
    s.text(6, 160, "drums L1: kick low, snare mid, cymbals x", XS, GRAY)
    legend2(s, [("PLAY", "pause"), ("EDIT", "view"), ("SEQ", "loop"), ("GLO", "help")])
    s.save(path)


def mock_viewset(path):
    s = Screen()
    s.text(6, 4, "VIEWS", B, WHITE)
    s.text(0, 6, "hidden: EDIT skips them", XS, DIM, right=234)
    s.rect(0, 31, 240, 1, LINE)
    rows = [("Drums: DRUM GRID", "SHOWN"), ("Drums: DRUM NOTATION", "SHOWN"), ("Bass: NOTE", "SHOWN"),
            ("Bass: KEYBOARD", "SHOWN"), ("Bass: CHART", "HIDDEN"), ("Bass: BASS TAB", "SHOWN"),
            ("Bass: NOTATION", "SHOWN"), ("Harmony: CHORD", "SHOWN")]
    for i, (n, v) in enumerate(rows):
        y = 34 + i * 22
        if i == 4:
            s.rect(2, y + 1, 236, 20, LINE)
        s.text(10, y + 3, n, XS, WHITE if i == 4 else GRAY)
        s.text(0, y + 3, v, XS, DIM if v == "HIDDEN" else AMB, right=230)
    legend2(s, [("SELECT", "view"), ("ALGO", "show/hide"), ("GLO", "back")])
    s.save(path)


MOCKS = {"p2-yours": mock_yours, "p3-harm-notation": mock_harm_notation, "p3-drum-notation": mock_drum_notation,
         "p3-viewset": mock_viewset}

if __name__ == "__main__":
    out = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "docs/step-M4")
    out.mkdir(parents=True, exist_ok=True)
    for name in sys.argv[2:] or list(MOCKS):
        MOCKS[name](out / f"mock-{name}.png")
    print(f"mock_m4: {out}/mock-*.png")
