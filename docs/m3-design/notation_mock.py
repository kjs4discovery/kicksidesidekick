"""Mock-up: simplified scrolling notation view on the FM-1's 240x240 screen (drawn at 2x)."""
from PIL import Image, ImageDraw, ImageFont
import sys

S = 2
W = H = 240
img = Image.new("RGB", (W * S, H * S), "black")
d = ImageDraw.Draw(img)
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
FONTB = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
FONTC = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"
def f(sz, bold=False, cond=False):
    return ImageFont.truetype(FONTC if cond else (FONTB if bold else FONT), int(sz * S))

AMBER = (255, 176, 0); WHITE = (235, 235, 235); GREY = (110, 110, 110)
DIM = (70, 70, 70); LINE = (95, 95, 95); BOX = (40, 40, 44)

def P(x, y): return (x * S, y * S)
def rect(x0, y0, x1, y1, **kw): d.rectangle([x0 * S, y0 * S, x1 * S, y1 * S], **kw)
def text(x, y, s, font, fill, anchor="la"): d.text(P(x, y), s, font=font, fill=fill, anchor=anchor)

# --- top line
text(4, 3, "Amazing Grace", f(10, cond=True), GREY)
text(4, 15, "MELODY MUTED · YOUR PART", f(8, cond=True), AMBER)
text(236, 2, "5:2", f(20, bold=True), WHITE, "ra")

# --- staff geometry
TOP = 66; SP = 12                   # top line y, line spacing
BOT = TOP + 4 * SP                  # bottom line (E4)
STEP = SP / 2
NOW_X = 80; PPB = 36.0; NOW_BEAT = 16.0   # bar 5, beat 2
CLIP_X = 44                          # notes start right of clef + key signature
LETTERS = "CDEFGAB"
def diat(n):                         # "G4" -> diatonic index
    return int(n[-1]) * 7 + LETTERS.index(n[0])
E4 = diat("E4")
def ypos(n): return BOT - (diat(n) - E4) * STEP
def xpos(beat): return NOW_X + (beat - NOW_BEAT) * PPB

for i in range(5):
    y = TOP + i * SP
    d.line([P(4, y), P(236, y)], fill=LINE, width=S)

# clef + key signature (G major: F#)
import math
gy = ypos("G4"); cx = 15
pts = []
for i in range(0, 200):                       # spiral around the G line, opening outwards
    a = i / 199 * 2.6 * math.pi
    r = 2 + a * 1.9
    pts.append((cx + r * math.cos(a + math.pi), gy + r * math.sin(a + math.pi) * 1.05))
pts += [(cx + 6, TOP + 2), (cx + 7, TOP - 8), (cx + 3, TOP - 14), (cx - 1, TOP - 8), (cx, TOP + 6), (cx + 4, BOT + 10), (cx + 2, BOT + 15), (cx - 3, BOT + 13)]
d.line([P(x, y) for x, y in pts], fill=WHITE, width=2 * S, joint="curve")
d.ellipse([P(cx - 6, BOT + 10)[0], P(cx - 6, BOT + 10)[1], P(cx - 1, BOT + 15)[0], P(cx - 1, BOT + 15)[1]], fill=WHITE)
text(27, ypos("F5") - 10, "♯", f(15), WHITE)

# notes: (name, start beat, length in beats)
notes = [("D4", 14, 1), ("G4", 15, 2), ("B4", 17, .5), ("G4", 17.5, .5),
         ("B4", 18, 2), ("A4", 20, 1), ("D5", 21, 3)]
bars = [15, 18, 21, 24]
chords = [(15, "G"), (18, "Em"), (21, "D")]

# bar lines and chord names
for b in bars:
    x = xpos(b)
    if CLIP_X < x < 238:
        d.line([P(x, TOP), P(x, BOT)], fill=LINE, width=S)
for b, c in chords:
    x = xpos(b)
    if x > CLIP_X - 6:
        col = AMBER if b <= NOW_BEAT < b + 3 else WHITE
        if x < 226:
            text(max(x, CLIP_X + 2), 34, c, f(15, bold=True), col)

def note(nm, beat, length):
    x = xpos(beat) + 11
    if x < CLIP_X + 4 or x > 232:
        return
    y = ypos(nm)
    sounding = beat <= NOW_BEAT < beat + length
    col = AMBER if sounding else (DIM if beat + length <= NOW_BEAT else WHITE)
    # ledger line for D4 is not needed (space below the staff); C4 would need one
    hollow = length >= 2
    rx, ry = 6.2, 4.6
    box = [(x - rx) * S, (y - ry) * S, (x + rx) * S, (y + ry) * S]
    if hollow:
        d.ellipse(box, outline=col, width=2 * S)
    else:
        d.ellipse(box, fill=col)
    if length == 3:                                   # dotted half
        d.ellipse([(x + rx + 2) * S, (y - 1.4) * S, (x + rx + 4.8) * S, (y + 1.4) * S], fill=col)
    up = diat(nm) < diat("B4") or length == .5
    if up:
        sx = x + rx - 1
        d.line([P(sx, y), P(sx, y - 30)], fill=col, width=S + 1)
        if length == .5:
            beams.append((sx, y - 30, col))
    else:
        sx = x - rx + 1
        d.line([P(sx, y), P(sx, y + 30)], fill=col, width=S + 1)
        if length == .5:
            d.line([P(sx, y + 30), P(sx + 7, y + 20)], fill=col, width=S + 1)
    # note name under the staff (learning aid, can be turned off)
    text(x, 132, nm[0], f(9, bold=True), col, "ma")

beams = []
for n in notes:
    note(*n)
for (x0, y0, c0), (x1, y1, c1) in zip(beams[0::2], beams[1::2]):
    top = min(y0, y1)
    d.line([P(x0, y0), P(x0, top)], fill=c0, width=S + 1)
    d.line([P(x1, y1), P(x1, top)], fill=c1, width=S + 1)
    d.polygon([P(x0 - .5, top), P(x1 + .5, top), P(x1 + .5, top + 4), P(x0 - .5, top + 4)], fill=c1)

# "now" line
d.line([P(NOW_X, TOP - 10), P(NOW_X, BOT + 12)], fill=AMBER, width=S)
d.polygon([P(NOW_X - 4, TOP - 14), P(NOW_X + 4, TOP - 14), P(NOW_X, TOP - 8)], fill=AMBER)

# fade the area behind the clef
rect(0, TOP - 2, 2, BOT + 2, fill="black")

# --- layer row
labels = [("DRUMS", "ON", False), ("BASS", "L1", False), ("HARM", "L1", False), ("MELODY", "MUTE", True)]
for i, (a, b, muted) in enumerate(labels):
    x0 = 4 + i * 58
    rect(x0, 158, x0 + 54, 196, fill=BOX, outline=AMBER if muted else None, width=S if muted else 0)
    text(x0 + 27, 163, a, f(9, cond=True), AMBER if muted else WHITE, "ma")
    text(x0 + 27, 177, b, f(11, bold=True), AMBER if muted else WHITE, "ma")

# --- legend (button names coloured, actions grey, dividers)
x = 4
for i, (btn, act) in enumerate([("PLAY", "pause"), ("EDIT", "view"), ("SELECT", "tempo")]):
    if i:
        d.line([P(x + 1, 214), P(x + 1, 228)], fill=GREY, width=S); x += 6
    text(x, 215, btn, f(9, bold=True), AMBER); x += d.textlength(btn, font=f(9, bold=True)) / S + 3
    text(x, 215, act, f(9), GREY); x += d.textlength(act, font=f(9)) / S + 5

img.save(sys.argv[1])
