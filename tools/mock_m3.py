#!/usr/bin/env python3
# mock_m3.py -- design mock-ups of M3's new screens for docs/controls.md (drawn before the firmware draws them), with
# tools/mock_screens.py's fonts, colours and pieces. Licence: TBD (see D017)
#
#   mock_m3.py [OUT_DIR]      -> OUT_DIR/mock-*.png at 2x (480 x 480); default docs/step-M3
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mock_screens import (AMB, B, BLACK, DIM, GRAY, HI, L, LINE, M, ROOT, S, WHITE, XS, Screen, keyboard,  # noqa: E402
                          layers, legend, strip)

DARK_AMB = (150, 88, 20)


def top(s, title, line2, bar, part=None):
    s.text(6, 1, title, XS, GRAY)
    x = 6
    if part:
        s.text(6, 16, part, XS, HI)
        x += s.d.textlength(part, font=XS) / S + 8
    s.text(x, 16, line2, XS, DIM)
    s.text(0, 3, bar, M, WHITE, right=234)


def cue(s, x, y, kind, c):
    """the rhythm cue at half M2's height: a note head 11 x 9, a stem 25"""
    d = s.d
    head = [x * S, (y + 18) * S, (x + 11) * S, (y + 26) * S]
    d.ellipse(head, fill=c) if kind not in ("half", "whole") else d.ellipse(head, outline=c, width=2 * S)
    if kind != "whole":
        s.rect(x + 9, y, 2, 22, c)
    if kind == "eighth":
        d.line([(x + 10) * S, y * S, (x + 16) * S, (y + 8) * S], fill=c, width=2 * S)


def big_band(s, big, nxt, small, kind, bright=True, label="NEXT", count=None):
    s.text(0, 40, big, L, HI, center_w=150)
    cue(s, 152, 52, kind, HI if bright else GRAY)
    s.rect(176, 38, 1, 64, LINE)
    s.text(184, 36, label, XS, DIM)
    s.text(184, 52, nxt, M, AMB)
    if count:
        s.text(184, 78, count, M, WHITE)
    elif small:
        s.text(184, 82, small, XS, GRAY)


def mock_harmony(path):
    s = Screen()
    top(s, "Amazing Grace", "84 BPM  G", "5:2", "HARMONY · YOUR PART")
    big_band(s, "G7", "C", None, "half", count="IN 2")
    strip(s, 108, [(5, "G7"), (6, "C"), (7, "G"), (8, "G")], 0)
    layers(s, 162, ["L1", "L1", "MUTE", "L1"])
    s.text(6, 197, "Verse 1", XS, GRAY)
    legend(s, "PLAY: PAUSE  EDIT: VIEW  SAVE: KEY  GLO: ?")
    s.save(path)


def mock_bass(path):
    s = Screen()
    top(s, "Londonderry Air (Danny Boy)", "66 BPM  C", "2:1", "BASS · YOUR PART")
    big_band(s, "E2", "F2", "chord C/E", "half")
    for i, (n, notes) in enumerate([(2, "E2"), (3, "F2"), (4, "G2"), (5, "C2  G2")]):
        x = 2 + i * 60
        s.rect(x, 108, 56, 44, HI if i == 0 else DIM, fill=False)
        s.text(x + 4, 109, str(n), XS, DIM)
        s.text(x, 126, notes, B, WHITE if i == 0 else AMB, center_w=56)
    layers(s, 162, ["L1", "MUTE", "L1", "L1"])
    s.text(6, 197, "A1", XS, GRAY)
    legend(s, "PLAY: PAUSE  EDIT: VIEW  SAVE: KEY  GLO: ?")
    s.save(path)


def mock_melody(path):
    s = Screen()
    top(s, "Amazing Grace", "84 BPM  G", "3:1", "MELODY · YOUR PART")
    big_band(s, "B4", "G4", "chord G", "quarter")
    for i, (n, notes) in enumerate([(3, "B G B"), (4, "A G"), (5, "E D"), (6, "D")]):
        x = 2 + i * 60
        s.rect(x, 108, 56, 44, HI if i == 0 else DIM, fill=False)
        s.text(x + 4, 109, str(n), XS, DIM)
        s.text(x, 126, notes, B, WHITE if i == 0 else AMB, center_w=56)
    layers(s, 162, ["L1", "L1", "L1", "MUTE"])
    s.text(6, 197, "Verse 1", XS, GRAY)
    legend(s, "PLAY: PAUSE  EDIT: VIEW  SAVE: KEY  GLO: ?")
    s.save(path)


def mock_drums(path):
    s = Screen()
    top(s, "Saints", "112 BPM  F", "3:2", "DRUMS · YOUR PART")
    big_band(s, "SNARE", "KICK", "+ HAT", "quarter")
    rows = ("K", "S", "H")
    for i, n in enumerate((3, 4, 5, 6)):
        x = 2 + i * 60
        s.rect(x, 108, 56, 44, HI if i == 0 else DIM, fill=False)
        s.text(x + 4, 109, str(n), XS, DIM)
        for r, name in enumerate(rows):
            for b in range(4):
                hit = (r == 0 and b in (0, 2)) or (r == 1 and b in (1, 3)) or r == 2
                if hit:
                    s.rect(x + 6 + b * 12, 126 + r * 8, 8, 5, WHITE if i == 0 else AMB)
    layers(s, 162, ["MUTE", "L1", "L1", "L1"])
    s.text(6, 197, "Verse 1", XS, GRAY)
    legend(s, "PLAY: PAUSE  EDIT: VIEW  SAVE: KEY  GLO: ?")
    s.save(path)


def mock_keys_blink(path):
    s = Screen()
    top(s, "Amazing Grace", "84 BPM  G", "5:2", "HARMONY · YOUR PART")
    s.text(6, 32, "G7", L, HI)
    s.text(0, 38, "next C  IN 2", B, AMB, right=234)
    keyboard(s, 4, 70, 48, 83, lit=(), dim=(48, 52, 55))
    for n, x in ((55, 4 + 4 * 11), (59, 4 + 6 * 11), (62, 4 + 8 * 11), (65, 4 + 10 * 11)):
        s.rect(x, 70, 10, 60, DARK_AMB)
    s.text(6, 140, "between attacks: the keys darker; at each attack: bright", XS, DIM)
    layers(s, 162, ["L1", "L1", "MUTE", "L1"])
    legend(s, "PLAY: PAUSE  EDIT: VIEW  SAVE: KEY  GLO: ?")
    s.save(path)


MOCKS = {"p2-harmony": mock_harmony, "p2-bass": mock_bass, "p2-melody": mock_melody, "p2-drums": mock_drums,
         "p2-keys-blink": mock_keys_blink}

if __name__ == "__main__":
    out = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "docs/step-M3")
    out.mkdir(parents=True, exist_ok=True)
    only = sys.argv[2:] or list(MOCKS)
    for name in only:
        MOCKS[name](out / f"mock-{name}.png")
    print(f"mock_m3: {out}/mock-*.png")
