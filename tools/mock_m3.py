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


LOOP_BG = (64, 38, 8)


def loop_strip(s, y, bars, cur, a=None, b=None, sel=None):
    """the strip with the loop's bars highlighted (filled), the small A and B marks; sel: the bar being moved"""
    for i, (n, ch) in enumerate(bars):
        x = 2 + i * 60
        inloop = a is not None and b is not None and a <= n <= b
        if inloop:
            s.rect(x, y, 56, 44, LOOP_BG)
        s.rect(x, y, 56, 44, HI if i == cur else DIM, fill=False)
        if sel == n:
            s.rect(x + 1, y + 1, 54, 42, WHITE, fill=False)
        s.text(x + 4, y + 1, str(n), XS, DIM)
        s.text(x, y + 16, ch, M, WHITE if i == cur else AMB, center_w=56)
        if n == a:
            s.text(x + 44, y + 1, "A", XS, HI)
        if n == b:
            s.text(x + 44, y + 1, "B", XS, HI)


def mock_loop_set_a(path):
    s = Screen()
    top(s, "Twelve-Bar Blues in C", "96 BPM  C", "STOP")
    s.rect(4, 15, 120, 16, HI)
    s.text(8, 16, "LOOP: SET A  bar 5", XS, BLACK)
    s.text(0, 40, "SET THE LOOP", M, WHITE, center_w=176)
    s.text(0, 70, "1. turn ALGORITHM to bar A", XS, HI, center_w=176)
    s.text(0, 86, "2. press SEQ", XS, DIM, center_w=176)
    s.rect(176, 38, 1, 64, LINE)
    s.text(184, 36, "A", XS, DIM)
    s.text(184, 52, "bar 5", M, AMB)
    loop_strip(s, 108, [(5, "F7"), (6, "F7"), (7, "C7"), (8, "C7")], 0, a=5, b=5, sel=5)
    layers(s, 162, ["L1", "L1", "MUTE", "L1"])
    s.text(6, 197, "Chorus 1", XS, GRAY)
    legend(s, "ALGO: A   PRESETS: SECTION   SEQ: SET   HOME: X")
    s.save(path)


def mock_loop_set_b(path):
    s = Screen()
    top(s, "Twelve-Bar Blues in C", "96 BPM  C", "STOP")
    s.rect(4, 15, 120, 16, HI)
    s.text(8, 16, "LOOP: SET B  bar 8", XS, BLACK)
    s.text(0, 40, "SET THE LOOP", M, WHITE, center_w=176)
    s.text(0, 70, "A = bar 5", XS, DIM, center_w=176)
    s.text(0, 86, "turn ALGORITHM to bar B, press SEQ", XS, HI, center_w=176)
    s.rect(176, 38, 1, 64, LINE)
    s.text(184, 36, "B", XS, DIM)
    s.text(184, 52, "bar 8", M, AMB)
    loop_strip(s, 108, [(5, "F7"), (6, "F7"), (7, "C7"), (8, "C7")], 0, a=5, b=8, sel=8)
    layers(s, 162, ["L1", "L1", "MUTE", "L1"])
    s.text(6, 197, "Chorus 1", XS, GRAY)
    legend(s, "ALGO: B   PRESETS: SECTION   SEQ: SET   HOME: BACK")
    s.save(path)


def mock_loop_on(path):
    s = Screen()
    top(s, "Twelve-Bar Blues in C", "96 BPM  C  LOOP 5-8", "6:2", "HARMONY - YOURS")
    big_band(s, "F7", "C7", None, "half", count="IN 3")
    loop_strip(s, 108, [(6, "F7"), (7, "C7"), (8, "C7"), (5, "F7")], 0, a=5, b=8)
    layers(s, 162, ["L1", "L1", "MUTE", "L1"])
    s.text(6, 197, "Chorus 1", XS, GRAY)
    s.text(0, 197, "LOOP 5-8", XS, HI, right=234)
    legend(s, "PLAY: PAUSE   SEQ: LOOP OFF   EDIT: VIEW")
    s.save(path)


def legend2(s, pairs, y=214):
    """M3 legend: button names amber, actions gray, a divider between pairs"""
    s.rect(0, y + 1, 240, 1, LINE)
    widths = [s.d.textlength(n + " " + a, font=XS) / S for n, a in pairs]
    gap = 9
    x = (240 - sum(widths) - gap * (len(pairs) - 1)) / 2
    for k, (n, a) in enumerate(pairs):
        if k:
            s.rect(int(x - gap / 2), y + 7, 1, 14, DIM)
        s.text(x, y + 7, n, XS, HI)
        s.text(x + s.d.textlength(n + " ", font=XS) / S, y + 7, a, XS, GRAY)
        x += widths[k] + gap


def layers2(s, states, y=180):
    layers(s, y, states)


def mock_layout(path):
    s = Screen()
    top(s, "Danny Boy", "66 BPM  C", "2:3", "HARMONY - YOURS")
    big_band(s, "C/E", "F", None, "half", count="IN 2")
    strip(s, 108, [(2, "C/E"), (3, "F"), (4, "C/G"), (5, "C")], 0)
    s.text(6, 160, "C/E, 1st inversion", XS, GRAY)
    s.text(0, 160, "L2", XS, DIM, right=234)
    layers2(s, ["L1", "L1", "MUTE", "L1"])
    legend2(s, [("PLAY", "pause"), ("EDIT", "view"), ("SAVE", "key"), ("GLO", "help")])
    s.save(path)


def mock_keys_zoom(path):
    s = Screen()
    top(s, "Danny Boy", "66 BPM  C", "2:3", "HARMONY - YOURS")
    s.text(6, 32, "C/E", L, HI)
    s.text(0, 38, "next F  IN 2", B, AMB, right=234)
    lo, w = 52, 21                                     # E3.. : 11 white keys
    whites = [n for n in range(lo, lo + 19) if n % 12 in (0, 2, 4, 5, 7, 9, 11)][:11]
    lit = (52, 55, 60)
    for i, n in enumerate(whites):
        s.rect(4 + i * w, 70, w - 1, 86, HI if n in lit else (220, 220, 225))
        if n in (53, 57, 60) and n not in lit:
            s.rect(4 + i * w + 4, 136, w - 9, 14, AMB, fill=False)
        if n % 12 == 0:
            s.text(4 + i * w + 3, 140, f"C{n // 12 - 1}", XS, BLACK if n in lit else DIM)
    for i, n in enumerate(whites):
        if n % 12 in (0, 2, 5, 7, 9):
            s.rect(4 + i * w + 14, 70, 13, 52, (40, 40, 46))
    s.text(6, 160, "C/E, 1st inversion  (harmony L2)", XS, GRAY)
    layers2(s, ["L1", "L1", "MUTE", "L1"])
    legend2(s, [("PLAY", "pause"), ("EDIT", "view"), ("SAVE", "key"), ("GLO", "help")])
    s.save(path)


def mock_settings(path):
    s = Screen()
    s.text(6, 4, "SETTINGS", B, WHITE)
    s.text(0, 6, "saved when you leave", XS, DIM, right=234)
    s.rect(0, 31, 240, 1, LINE)
    rows = [("Key lights blink", "ON"), ("Count-in length", "1 BAR"), ("Count-in volume", "HIGH"),
            ("Note names (notation)", "ON"), ("Guide volume (your part)", "OFF")]
    for i, (name, val) in enumerate(rows):
        y = 38 + i * 32
        if i == 1:
            s.rect(2, y - 2, 236, 28, LINE)
        s.text(10, y + 3, name, B, WHITE if i == 1 else GRAY)
        s.text(0, y + 3, val, B, HI if i == 1 else AMB, right=230)
    legend2(s, [("SELECT", "row"), ("ALGO", "change"), ("GLO", "back")])
    s.save(path)


MOCKS = {"p4-layout": mock_layout, "p4-keys-zoom": mock_keys_zoom, "p4-settings": mock_settings, "p3-loop-set-a": mock_loop_set_a, "p3-loop-set-b": mock_loop_set_b, "p3-loop-on": mock_loop_on, "p2-harmony": mock_harmony, "p2-bass": mock_bass, "p2-melody": mock_melody, "p2-drums": mock_drums,
         "p2-keys-blink": mock_keys_blink}

if __name__ == "__main__":
    out = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "docs/step-M3")
    out.mkdir(parents=True, exist_ok=True)
    only = sys.argv[2:] or list(MOCKS)
    for name in only:
        MOCKS[name](out / f"mock-{name}.png")
    print(f"mock_m3: {out}/mock-*.png")
