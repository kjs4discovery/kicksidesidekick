#!/usr/bin/env python3
# mock_m4.py -- design mock-ups of M4's new screens for docs/controls.md (drawn before the firmware draws them), with
# tools/mock_screens.py's and mock_m3.py's pieces. Licence: TBD (see D017)
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


MOCKS = {"p2-yours": mock_yours}

if __name__ == "__main__":
    out = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "docs/step-M4")
    out.mkdir(parents=True, exist_ok=True)
    for name in sys.argv[2:] or list(MOCKS):
        MOCKS[name](out / f"mock-{name}.png")
    print(f"mock_m4: {out}/mock-*.png")
