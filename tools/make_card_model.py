"""The memory card SD2Cloud shows turning while it works on a card (src/app/work.c): an sd2psx, in the shape of the
PSXMemCard Gen2 (one of the devices its firmware runs on). A dark smoked shell with round edges and the device's name
on it, the grip's waves low on its sides, the arrow of the way in and the two screws at the top, the small black and
white screen in its raised frame showing what the device shows, the two buttons at its end, and at the other end the
opening with its eight gold contacts.

Writes assets/sd2psx.icn: the model and its 128x128 texture, in the PS2's icon format, which the program embeds and
reads as it reads a save's icon (icon_make_sd2psx in src/ui/icon.c). And assets/memcardpro2.icn, a MemCard PRO2 (see
"the MemCard PRO2" below), which the program shows on that device; and assets/memcard.icn, the console's own memory
card (see "the PS2 Memory Card"), for the Memory Card in the other slot. They are kept in the repository, so this only
runs when a model changes (it needs Pillow, and numpy for the previews).

usage: python tools/make_card_model.py [<out folder> [<a folder for a preview of each, to look at>]]
"""
import math
import pathlib
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

from ps2icon import icn

ROOT = pathlib.Path(__file__).resolve().parent.parent
TTF = str(ROOT / "third_party/varelaround/VarelaRound-Regular.ttf")

W, H, T = 3.6, 4.5, 0.6          # the card: width, height, thickness (a save's icon fills a box of 5 units)
BASE = 0.25                      # how far above the ground it stands
ZF = T / 2                       # its face
BEVEL_W = 0.06                   # the round edge between the face and the rim
SEAM = 0.018                     # half the groove where the shell's two halves meet
R_TOP, R_BOTTOM = 0.16, 0.30     # its corners: tighter at the end that goes into the console
WAVES, WAVE_PITCH, WAVE_Y, WAVE_DEPTH = 4, 0.38, 0.5, 0.045   # the grip
PAD = (0.0, 1.78, 2.3, 1.42, 0.17)          # the screen's raised frame: its middle, width, height, corner
WINDOW = (0.0, 1.78, 1.84, 0.92, 0.05)      # and the glass in it (2:1, as the 128x64 screen)
PAD_H, PAD_BEVEL, GLASS = 0.075, 0.03, 0.02  # how far each stands out of the face
SLOT_X, SLOT_Z, SLOT_DEPTH = 1.28, 0.16, 0.26   # the opening at the top: half its width, half its depth, how deep

# The texture: the card's face, on all of it. The part of the face that the screen's frame covers is never seen, so it
# holds what else the model needs: the screen (64x32, half the real one's 128x64) and, beside it, the plain colors,
# one for each part that has no drawing
SCREEN = (27, 62, 91, 94)
FACE = (74, 79, 91)              # the shell's face where it is lightest (see face_bright)
PRINT = (236, 240, 248)          # what is printed on it
PLAIN = [(60, 64, 74), (50, 54, 63), (16, 17, 20), (116, 123, 139), (34, 36, 43), (10, 11, 14), (222, 176, 78),
         (104, 111, 126), (108, 115, 131), (12, 13, 16), (72, 77, 89)]
PLAIN_BOX = [(92 + 5 * (i % 2), 62 + 5 * (i // 2), 97 + 5 * (i % 2), 67 + 5 * (i // 2)) for i in range(len(PLAIN))]
SHELL, RIM, GROOVE, BEVEL, FRAME, FRAME_WALL, GOLD, BUTTON, ARROW, SLOT, FRAME_EDGE = [
    ((b[0] + b[2]) / 2 / 128, (b[1] + b[3]) / 2 / 128) for b in PLAIN_BOX]
LIT = (238, 242, 248)            # what the screen shows: white on black, as the sd2psx's own
GLOW = 0xFF                      # the screen's own light: as bright from any side (see transform in icon.c)

verts = []


def unit(v):
    l = math.sqrt(sum(x * x for x in v)) or 1
    return tuple(x / l for x in v)


def tri(p, n, uv, bright=0x80):
    """three corners (any order around), one normal or one for each, one place in the texture or one for each, and
    how bright (0x80 = as the texture is; one for each corner when it is a list)"""
    ns = n if isinstance(n[0], tuple) else [n] * 3
    uvs = uv if isinstance(uv[0], tuple) else [uv] * 3
    brights = bright if isinstance(bright, list) else [bright] * 3
    a, b, c = p
    cross = ((b[1] - a[1]) * (c[2] - a[2]) - (b[2] - a[2]) * (c[1] - a[1]), (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2]),
             (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]))
    out = [sum(m[k] for m in ns) for k in range(3)]
    for i in ((0, 1, 2) if sum(x * y for x, y in zip(cross, out)) > 0 else (0, 2, 1)):   # counterclockwise from outside
        verts.append((p[i], unit(ns[i]), uvs[i], brights[i]))


def quad(p, n, uv, bright=0x80):
    """four corners around: two triangles"""
    ns = n if isinstance(n[0], tuple) else [n] * 4
    uvs = uv if isinstance(uv[0], tuple) else [uv] * 4
    for i in ((0, 1, 2), (0, 2, 3)):
        tri([p[k] for k in i], [ns[k] for k in i], [uvs[k] for k in i], bright)


def outward(pts):
    """for each corner of a counterclockwise outline, the way out of it"""
    out = []
    for i, b in enumerate(pts):
        a, c = pts[i - 1], pts[(i + 1) % len(pts)]
        e1, e2 = unit((b[1] - a[1], a[0] - b[0])), unit((c[1] - b[1], b[0] - c[0]))
        out.append(unit((e1[0] + e2[0], e1[1] + e2[1])))
    return out


def card_outline():
    """the card from the front, counterclockwise from its bottom right corner, and which of its sides is the top"""
    pts = []

    def corner(cx, cy, r, a0):
        for k in range(7):
            a = math.radians(a0 + 15 * k)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    grip = [(W / 2 - WAVE_DEPTH * (1 - math.cos(2 * math.pi * j / 8)), WAVE_Y + WAVE_PITCH * j / 8) for j in range(WAVES * 8 + 1)]
    corner(W / 2 - R_BOTTOM, R_BOTTOM, R_BOTTOM, -90)
    pts += grip
    corner(W / 2 - R_TOP, H - R_TOP, R_TOP, 0)
    top = len(pts) - 1
    corner(-W / 2 + R_TOP, H - R_TOP, R_TOP, 90)
    pts += [(-x, y) for x, y in reversed(grip)]
    corner(-W / 2 + R_BOTTOM, R_BOTTOM, R_BOTTOM, 180)
    return pts, top


def round_rect(cx, cy, w, h, r):
    """counterclockwise from its bottom right corner; two of them have their corners in the same order"""
    pts = []
    for sx, sy, a0 in ((1, -1, -90), (1, 1, 0), (-1, 1, 90), (-1, -1, 180)):
        for k in range(5):
            a = math.radians(a0 + 22.5 * k)
            pts.append((cx + sx * (w / 2 - r) + r * math.cos(a), cy + sy * (h / 2 - r) + r * math.sin(a)))
    return pts


def face_uv(x, y):
    return ((0.5 + (x + W / 2) / W * 127) / 128, (0.5 + (1 - y / H) * 127) / 128)


def under_frame(col, row):
    """is this corner of the texture on the part of the face that the frame covers, well inside it?"""
    cx, cy, w, h, r = PAD
    x, y = abs((col - 0.5) / 127 * W - W / 2 - cx), abs((1 - (row - 0.5) / 127) * H - cy)
    w, h, edge = w / 2 - 0.07, h / 2 - 0.07, max(r - 0.07, 0)
    return x <= w and y <= h and (x <= w - edge or y <= h - edge or math.hypot(x - w + edge, y - h + edge) <= edge)


def face_bright(y):
    """the smoked shell is a little lighter toward the top. That is in its corners' brightness, not in the texture:
    16-bit color would show it in bands"""
    return int(round(0x80 * (0.62 + 0.38 * y / H)))


def bands():
    """the rim from front to back: the shell's two halves and the groove between them"""
    z = ZF - BEVEL_W
    return ((z, SEAM, RIM), (SEAM, -SEAM, GROOVE), (-SEAM, -z, RIM))


def shell():
    pts, top = card_outline()
    out = outward(pts)
    inner = [(p[0] - n[0] * BEVEL_W, p[1] - n[1] * BEVEL_W) for p, n in zip(pts, out)]
    middle, zr, k = (0.0, H / 2), ZF - BEVEL_W, 0.4
    for i, a in enumerate(pts):
        j = (i + 1) % len(pts)
        b, ia, ib, na, nb = pts[j], inner[i], inner[j], out[i], out[j]
        tri([middle + (ZF,), ia + (ZF,), ib + (ZF,)], (0, 0, 1), [face_uv(*middle), face_uv(*ia), face_uv(*ib)],
            [face_bright(middle[1]), face_bright(ia[1]), face_bright(ib[1])])
        tri([middle + (-ZF,), ia + (-ZF,), ib + (-ZF,)], (0, 0, -1), SHELL)
        for s in (1, -1):   # the round edge, front and back: its light goes from the face's to the rim's
            quad([ia + (s * ZF,), ib + (s * ZF,), b + (s * zr,), a + (s * zr,)],
                 [(na[0] * k, na[1] * k, s), (nb[0] * k, nb[1] * k, s), (nb[0], nb[1], s * k), (na[0], na[1], s * k)], BEVEL)
        if i == top:   # (that side has the opening in it)
            continue
        for z0, z1, color in bands():
            quad([a + (z0,), b + (z0,), b + (z1,), a + (z1,)], [na + (0,), nb + (0,), nb + (0,), na + (0,)], color)
    opening(pts[top][0])


def opening(x1):
    """the top side, from -x1 to x1: the rim around the opening, the opening itself and the contacts in it"""
    up, y, zr, sx, sz, floor = (0, 1, 0), H, ZF - BEVEL_W, SLOT_X, SLOT_Z, H - SLOT_DEPTH
    for xa, xb in ((-x1, -sx), (sx, x1)):
        for z0, z1, color in bands():
            quad([(xa, y, z0), (xb, y, z0), (xb, y, z1), (xa, y, z1)], up, color)
    for za, zb in ((zr, sz), (-sz, -zr)):
        quad([(-sx, y, za), (sx, y, za), (sx, y, zb), (-sx, y, zb)], up, RIM)
    for z, n in ((sz, (0, 0, -1)), (-sz, (0, 0, 1))):
        quad([(-sx, y, z), (sx, y, z), (sx, floor, z), (-sx, floor, z)], n, SLOT)
    for x, n in ((-sx, (1, 0, 0)), (sx, (-1, 0, 0))):
        quad([(x, y, -sz), (x, y, sz), (x, floor, sz), (x, floor, -sz)], n, SLOT)
    quad([(-sx, floor, -sz), (sx, floor, -sz), (sx, floor, sz), (-sx, floor, sz)], up, SLOT)
    # eight contacts in groups of 2, 3 and 3, as a PS2 memory card has them, with the shell's two dividers between
    gap, divider, edge = 0.05, 0.12, 0.06
    cw = (2 * sx - 2 * edge - 2 * divider - 5 * gap) / 8
    x = -sx + edge
    for group in (2, 3, 3):
        for i in range(group):
            box((x, floor, -0.075), (x + cw, y - 0.035, 0.075), GOLD)
            x += cw + (gap if i < group - 1 else 0)
        if x < sx - edge - 0.001:
            box((x, floor, -sz), (x + divider, y, sz), RIM)
            x += divider


def box(lo, hi, color):
    """a block from lo to hi, without its bottom"""
    (x0, y0, z0), (x1, y1, z1) = lo, hi
    quad([(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)], (0, 1, 0), color)
    quad([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1), color)
    quad([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)], (0, 0, -1), color)
    quad([(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], (-1, 0, 0), color)
    quad([(x1, y0, z0), (x1, y0, z1), (x1, y1, z1), (x1, y1, z0)], (1, 0, 0), color)


def screen():
    """the raised frame, the window in it and the lit glass at its bottom"""
    cx, cy, w, h, r = PAD
    outer, top = round_rect(cx, cy, w, h, r), round_rect(cx, cy, w - 2 * PAD_BEVEL, h - 2 * PAD_BEVEL, r - PAD_BEVEL)
    window = round_rect(*WINDOW)
    no, nw = outward(outer), outward(window)
    z0, z1, z2, zg, k = ZF, ZF + PAD_H - PAD_BEVEL, ZF + PAD_H, ZF + GLASS, 0.4
    x0, x1 = WINDOW[0] - WINDOW[2] / 2, WINDOW[0] + WINDOW[2] / 2
    y0, y1 = WINDOW[1] - WINDOW[3] / 2, WINDOW[1] + WINDOW[3] / 2

    def glass_uv(p):   # (half a texel inside the screen's part of the texture: nothing of its neighbors comes in)
        return ((SCREEN[0] + 0.5 + (p[0] - x0) / (x1 - x0) * (SCREEN[2] - SCREEN[0] - 1)) / 128,
                (SCREEN[1] + 0.5 + (y1 - p[1]) / (y1 - y0) * (SCREEN[3] - SCREEN[1] - 1)) / 128)
    for i, a in enumerate(outer):
        j = (i + 1) % len(outer)
        b, ta, tb, wa, wb = outer[j], top[i], top[j], window[i], window[j]
        na, nb, ma, mb = no[i] + (0,), no[j] + (0,), nw[i], nw[j]
        quad([a + (z0,), b + (z0,), b + (z1,), a + (z1,)], [na, nb, nb, na], FRAME)
        quad([a + (z1,), b + (z1,), tb + (z2,), ta + (z2,)],
             [na, nb, (nb[0] * k, nb[1] * k, 1), (na[0] * k, na[1] * k, 1)], FRAME_EDGE)
        quad([ta + (z2,), tb + (z2,), wb + (z2,), wa + (z2,)], (0, 0, 1), FRAME)
        quad([wa + (z2,), wb + (z2,), wb + (zg,), wa + (zg,)],
             [(-ma[0], -ma[1], 0), (-mb[0], -mb[1], 0), (-mb[0], -mb[1], 0), (-ma[0], -ma[1], 0)], FRAME_WALL)
        tri([(WINDOW[0], WINDOW[1], zg), wa + (zg,), wb + (zg,)], (0, 0, 1),
            [glass_uv((WINDOW[0], WINDOW[1])), glass_uv(wa), glass_uv(wb)], GLOW)


def slab(shape, z0, z1, color):
    """a flat shape in x and y standing out of the face from z0 to z1: its front and its rim"""
    cx, cy = sum(p[0] for p in shape) / len(shape), sum(p[1] for p in shape) / len(shape)
    for i, a in enumerate(shape):
        b = shape[(i + 1) % len(shape)]
        tri([(cx, cy, z1), a + (z1,), b + (z1,)], (0, 0, 1), color)
        n = unit((b[1] - a[1], a[0] - b[0]))
        if n[0] * ((a[0] + b[0]) / 2 - cx) + n[1] * ((a[1] + b[1]) / 2 - cy) < 0:
            n = (-n[0], -n[1])
        quad([a + (z0,), b + (z0,), b + (z1,), a + (z1,)], n + (0,), color)


BUTTON_X, LABEL_Y = 1.02, 0.4    # the two buttons, and their names over them
NAME, NAME_Y = "SD2PSX", 3.1     # what the card is, printed between the arrow and the screen
ARROW_Y, SCREW = H - 0.62, (W / 2 - 0.3, H - 0.3)


def model():
    shell()
    screen()
    slab([(0.0, ARROW_Y + 0.2), (-0.36, ARROW_Y - 0.18), (0.36, ARROW_Y - 0.18)], ZF, ZF + 0.022, ARROW)
    for x in (-BUTTON_X, BUTTON_X):   # the buttons are at the card's end: their caps show a little over its edge
        box((x - 0.27, -0.035, ZF - 0.22), (x + 0.27, 0.15, ZF + 0.03), BUTTON)
    return [((x, y + BASE, z), *rest) for (x, y, z), *rest in verts]


def screen_art():
    """what the sd2psx shows on its 128x64 screen with a card selected: the mode in a lit bar, the card and its
    channel, and what the buttons do. Drawn large and brought down to half of it"""
    k = 8
    im = Image.new("RGB", (128 * k, 64 * k), (2, 3, 6))
    d = ImageDraw.Draw(im)
    font = ImageFont.truetype(TTF, 11 * k)

    def text(x, y, s, fill, anchor="lt"):
        d.text((x * k, y * k), s, font=font, anchor=anchor, fill=fill, stroke_width=k // 3, stroke_fill=fill)
    d.rectangle((0, 0, 128 * k, 12 * k), fill=LIT)
    text(64, 6, "PS2 Memory Card", (2, 3, 6), "mm")
    text(2, 18, "Card", LIT)
    text(126, 18, "1", LIT, "rt")
    text(2, 33, "Channel", LIT)
    text(126, 33, "1", LIT, "rt")
    for x0, x1, s in ((0, 12, "<"), (44, 84, "Menu"), (116, 128, ">")):
        d.rectangle((x0 * k, 51 * k, x1 * k, 64 * k), fill=LIT)
        text((x0 + x1) / 2, 57, s, (2, 3, 6), "mm")
    sheen = Image.new("RGB", im.size, (0, 0, 0))   # the glass over it: a faint light across one corner
    ImageDraw.Draw(sheen).polygon(((70 * k, 0), (104 * k, 0), (84 * k, 64 * k), (50 * k, 64 * k)), fill=(10, 13, 22))
    im = ImageChops.add(im, sheen)
    return im.resize((SCREEN[2] - SCREEN[0], SCREEN[3] - SCREEN[1]), Image.LANCZOS)


def texture():
    """the card's face, drawn large in its own proportions and brought down to the texture; where the frame covers
    it, the screen and the plain colors"""
    k = 8
    w = 128 * k
    px = w / W   # pixels to a unit of the model
    h = int(round(H * px))

    def fx(x):
        return w / 2 + x * px

    def fy(y):   # a height above the card's bottom, as a row of the drawing
        return h - y * px
    face = Image.new("RGB", (w, h), FACE)
    # the shadow of the screen's frame on the face: all around it, and more under it (the light comes from above)
    cx, cy, pw, ph, r = PAD
    shadow = Image.new("L", (w, h), 0)
    ImageDraw.Draw(shadow).rounded_rectangle((fx(cx - pw / 2) - 0.035 * px, fy(cy + ph / 2) - 0.02 * px, fx(cx + pw / 2) + 0.035 * px,
                                              fy(cy - ph / 2) + 0.1 * px), radius=r * px, fill=170)
    face = Image.composite(Image.new("RGB", (w, h), (16, 17, 21)), face, shadow.filter(ImageFilter.GaussianBlur(0.045 * px)))
    d = ImageDraw.Draw(face)
    for s in (-1, 1):    # the two screws, each deep in its hole
        x, y, r = fx(s * SCREW[0]), fy(SCREW[1]), 0.115 * px
        d.ellipse((x - r, y - r, x + r, y + r), fill=(18, 19, 23))
        r *= 0.62
        d.ellipse((x - r, y - r, x + r, y + r), fill=(92, 98, 112))
        d.line((x - r * 0.7, y, x + r * 0.7, y), fill=(34, 36, 43), width=k)
        d.line((x, y - r * 0.7, x, y + r * 0.7), fill=(34, 36, 43), width=k)
    font = ImageFont.truetype(TTF, int(0.23 * px))
    for x, s in ((-BUTTON_X, "BT1"), (BUTTON_X, "BT2")):
        d.text((fx(x), fy(LABEL_Y)), s, font=font, anchor="mm", fill=PRINT, stroke_width=k // 4, stroke_fill=PRINT)
    d.text((fx(0), fy(NAME_Y)), NAME, font=ImageFont.truetype(TTF, int(0.58 * px)), anchor="mm", fill=PRINT,
           stroke_width=k // 2, stroke_fill=PRINT)
    im = face.resize((128, 128), Image.LANCZOS)
    for box in [SCREEN] + PLAIN_BOX:
        assert all(under_frame(col, row) for col in box[::2] for row in box[1::2]), "the frame doesn't cover %r" % (box,)
    im.paste(screen_art(), SCREEN[:2])
    d = ImageDraw.Draw(im)
    for box, c in zip(PLAIN_BOX, PLAIN):
        d.rectangle((box[0], box[1], box[2] - 1, box[3] - 1), fill=c)
    return im


# ------------------------------------------------------------ the MemCard PRO2

# The MemCard PRO2 (8BitMods), for SD2Cloud on that device: a PS2 memory card in a dark smoked see-through shell, as
# big as the sd2psx's model, so the program draws either the same way. Its upper part is thin, and the dark board
# inside shows through it with the device's name printed on the board; its lower part is a thicker block standing out
# of the face, with the screen (white on black) sunk into it in a black bezel, the two buttons at its corners, "8BitMods"
# moulded between them, and the opening of the microSD at its end
P_Z, P_ZB, P_YB = 0.2, 0.36, 2.95          # the thin part's half thickness; the block's face; where the block ends
P_RTOP, P_RBOTTOM = 0.16, 0.3              # the corners: the block's are rounder
P_BEZEL, P_SINK = (0.0, 1.8, 1.96, 1.12), 0.035   # the screen's bezel (middle, width, height) and how deep it is sunk
P_OLED = (0.0, 1.8, 1.62, 0.81)            # its lit part, 2:1 as the device's 128x64
P_BUTTON_X, P_BUTTON = (-1.22, 1.22), (0.6, 0.46, 0.07)   # the buttons: their x; their y, side and height
P_SD = (0.6, 0.05, 0.1)                    # the microSD's opening in the block's end: half width, half height, depth
P_SLOT_Z = 0.11                            # half the depth of the opening at the top, where the contacts are
P_SMOKE = (40, 42, 47)                     # the smoked shell, over what is inside it
P_BOARD = (22, 23, 27)                     # the board inside, black
P_TRACE = (44, 46, 52)                     # its traces
P_PRINT = (240, 244, 240)                  # what is printed on the board
P_LIT = (205, 226, 255)                    # what the screen shows
P_MOULD = (98, 102, 111)                   # what is moulded in the block
# the parts with no drawing, each a plain color of the texture: under the buttons' caps, which hide it
P_COLORS = {"back": (36, 38, 43), "rim": (60, 63, 70), "side": (52, 55, 62), "ledge": (76, 80, 88),
            "sink": (16, 17, 20), "button": (86, 90, 98), "slot": (10, 11, 14), "gold": (222, 176, 78),
            "inside": (30, 32, 37)}


def pro2_patches():
    """where each plain color is: a 3x3 texel square, nine of them under each button's cap"""
    boxes = []
    for bx in P_BUTTON_X:
        u, v = face_uv(bx, P_BUTTON[0])
        cx, cy = int(u * 128), int(v * 128)
        boxes += [(cx - 4 + 3 * (i % 3), cy - 4 + 3 * (i // 3), cx - 1 + 3 * (i % 3), cy - 1 + 3 * (i // 3)) for i in range(9)]
    return dict(zip(P_COLORS, boxes))


def pro2_uv(name):
    b = pro2_patches()[name]
    return ((b[0] + b[2]) / 2 / 128, (b[1] + b[3]) / 2 / 128)


def contacts(sx):
    """the opening at the top from -sx to sx: eight contacts in groups of 2, 3 and 3, as a PS2 memory card has them,
    with the shell's two dividers between them. Each piece: its x from and to, and whether it is a contact"""
    gap, divider, edge = 0.05, 0.12, 0.06
    cw = (2 * sx - 2 * edge - 2 * divider - 5 * gap) / 8
    x, out = -sx + edge, []
    for group in (2, 3, 3):
        for i in range(group):
            out.append((x, x + cw, 1))
            x += cw + (gap if i < group - 1 else 0)
        if x < sx - edge - 0.001:
            out.append((x, x + divider, 0))
            x += divider
    return out


def pro2_outline(y0, y1, r0, r1, top_corners=True):
    """a rectangle the card's width from y0 to y1, counterclockwise from its bottom right, its bottom corners rounded
    by r0 and its top ones by r1 (or square)"""
    pts = []

    def corner(cx, cy, r, a0):
        for k in range(7):
            a = math.radians(a0 + 15 * k)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    if r0:
        corner(W / 2 - r0, y0 + r0, r0, -90)
    else:
        pts.append((W / 2, y0))
    if top_corners:
        corner(W / 2 - r1, y1 - r1, r1, 0)
        corner(-W / 2 + r1, y1 - r1, r1, 90)
    else:
        pts += [(W / 2, y1), (-W / 2, y1)]
    if r0:
        corner(-W / 2 + r0, y0 + r0, r0, 180)
    else:
        pts.append((-W / 2, y0))
    return pts


def fan(pts, z, normal, uv, bright=0x80):
    """a flat convex shape at depth z, from its middle: uv = a function of (x, y), or one place in the texture"""
    c = (sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts))
    for i, a in enumerate(pts):
        b = pts[(i + 1) % len(pts)]
        ps = [c, a, b]
        tri([p + (z,) for p in ps], normal, [uv(*p) for p in ps] if callable(uv) else uv,
            [bright(p[1]) for p in ps] if callable(bright) else bright)


def walls(pts, z0, z1, color, skip=()):
    """the sides of an outline from depth z0 to z1, facing out; skip = the edges (by their first corner) left out"""
    out = outward(pts)
    for i, a in enumerate(pts):
        if i in skip:
            continue
        j = (i + 1) % len(pts)
        b, na, nb = pts[j], out[i], out[j]
        quad([a + (z0,), b + (z0,), b + (z1,), a + (z1,)], [na + (0,), nb + (0,), nb + (0,), na + (0,)], color)


def face_with_hole(pts, hole, z):
    """the block's face, a convex outline with a rectangular hole in it (the screen's bezel): the ring between them
    in quads, each from a stretch of the outline to the stretch of the hole's edge it faces"""
    hx, hy, hw, hh = hole

    def on_hole(p):
        dx, dy = p[0] - hx, p[1] - hy
        t = min(hw / 2 / abs(dx) if dx else 1e9, hh / 2 / abs(dy) if dy else 1e9)
        return (hx + dx * t, hy + dy * t)

    def on_outline(a):   # where the ray from the hole's middle at angle a leaves the outline
        dx, dy = math.cos(a), math.sin(a)
        best = None
        for i, p in enumerate(pts):
            q = pts[(i + 1) % len(pts)]
            ex, ey = q[0] - p[0], q[1] - p[1]
            den = dx * ey - dy * ex
            if abs(den) < 1e-12:
                continue
            t = ((p[0] - hx) * ey - (p[1] - hy) * ex) / den
            s = ((p[0] - hx) * dy - (p[1] - hy) * dx) / den
            if t > 0 and -1e-9 <= s <= 1 + 1e-9 and (best is None or t < best):
                best = t
        return (hx + dx * best, hy + dy * best)
    angles = sorted({math.atan2(p[1] - hy, p[0] - hx) for p in pts} |
                    {math.atan2(sy * hh / 2, sx * hw / 2) for sx in (-1, 1) for sy in (-1, 1)})
    ring = [on_outline(a) for a in angles]
    for i, a in enumerate(ring):
        b = ring[(i + 1) % len(ring)]
        ps = [a, b, on_hole(b), on_hole(a)]
        quad([p + (z,) for p in ps], (0, 0, 1), [face_uv(*p) for p in ps])


def pro2_model():
    color = pro2_uv
    plate = pro2_outline(P_YB, H, 0, P_RTOP)
    block = pro2_outline(0, P_YB, P_RBOTTOM, 0, top_corners=False)
    whole = pro2_outline(0, H, P_RBOTTOM, P_RTOP)
    # the thin part: its face (lighter toward the top, as the light falls on it), its sides, and its top with the
    # opening; the block's top edge on it
    fan(plate, P_Z, (0, 0, 1), face_uv, lambda y: int(round(0x80 * (0.8 + 0.2 * y / H))))
    top = len(plate) // 2 - 1   # (the edge between the two top corners)
    walls(plate, -P_Z, P_Z, color("rim"), skip=(len(plate) - 1, top))
    x1, sx, sz, floor, y = W / 2 - P_RTOP, SLOT_X, P_SLOT_Z, H - SLOT_DEPTH, H
    for xa, xb in ((-x1, -sx), (sx, x1)):
        quad([(xa, y, -P_Z), (xb, y, -P_Z), (xb, y, P_Z), (xa, y, P_Z)], (0, 1, 0), color("rim"))
    for za, zb in ((P_Z, sz), (-sz, -P_Z)):
        quad([(-sx, y, za), (sx, y, za), (sx, y, zb), (-sx, y, zb)], (0, 1, 0), color("rim"))
    for z, n in ((sz, (0, 0, -1)), (-sz, (0, 0, 1))):
        quad([(-sx, y, z), (sx, y, z), (sx, floor, z), (-sx, floor, z)], n, color("inside"))
    for x, n in ((-sx, (1, 0, 0)), (sx, (-1, 0, 0))):
        quad([(x, y, -sz), (x, y, sz), (x, floor, sz), (x, floor, -sz)], n, color("inside"))
    quad([(-sx, floor, -sz), (sx, floor, -sz), (sx, floor, sz), (-sx, floor, sz)], (0, 1, 0), color("inside"))
    for xa, xb, gold in contacts(sx):
        if gold:
            box((xa, floor, -0.055), (xb, y - 0.03, 0.055), color("gold"))
        else:
            box((xa, floor, -sz), (xb, y, sz), color("rim"))
    # the block: its face with the bezel sunk into it and the lit glass at the bottom, its sides and end, and its
    # top edge
    face_with_hole(block, P_BEZEL[:4], P_ZB)
    bx, by, bw, bh = P_BEZEL
    z0, zg = P_ZB, P_ZB - P_SINK
    hole = [(bx + bw / 2, by - bh / 2), (bx + bw / 2, by + bh / 2), (bx - bw / 2, by + bh / 2), (bx - bw / 2, by - bh / 2)]
    for i, a in enumerate(hole):
        b = hole[(i + 1) % 4]
        n = unit((a[1] - b[1], b[0] - a[0]))   # (into the hole: the walls are seen from inside it)
        quad([a + (z0,), b + (z0,), b + (zg,), a + (zg,)], n + (0,), color("sink"))
    quad([p + (zg,) for p in hole], (0, 0, 1), [face_uv(*p) for p in hole], GLOW)
    walls(block, -P_Z, P_ZB, color("side"), skip=(7, len(block) - 1))   # (not its top, on the thin part, nor its end)
    quad([(-W / 2, P_YB, P_Z), (W / 2, P_YB, P_Z), (W / 2, P_YB, P_ZB), (-W / 2, P_YB, P_ZB)], (0, 1, 0), color("ledge"))
    # its end, with the microSD's opening in the middle of its depth
    xe, (hw, hz, depth), zm = W / 2 - P_RBOTTOM, P_SD, (P_ZB - P_Z) / 2
    down = (0, -1, 0)
    for za, zb in ((-P_Z, zm - hz), (zm + hz, P_ZB)):
        quad([(-xe, 0, za), (xe, 0, za), (xe, 0, zb), (-xe, 0, zb)], down, color("side"))
    for xa, xb in ((-xe, -hw), (hw, xe)):
        quad([(xa, 0, zm - hz), (xb, 0, zm - hz), (xb, 0, zm + hz), (xa, 0, zm + hz)], down, color("side"))
    for z, n in ((zm - hz, (0, 0, 1)), (zm + hz, (0, 0, -1))):
        quad([(-hw, 0, z), (hw, 0, z), (hw, depth, z), (-hw, depth, z)], n, color("slot"))
    for x, n in ((-hw, (1, 0, 0)), (hw, (-1, 0, 0))):
        quad([(x, 0, zm - hz), (x, 0, zm + hz), (x, depth, zm + hz), (x, depth, zm - hz)], n, color("slot"))
    quad([(-hw, depth, zm - hz), (hw, depth, zm - hz), (hw, depth, zm + hz), (-hw, depth, zm + hz)], down, color("slot"))
    # the buttons, standing on the block's face at its corners
    y, side, height = P_BUTTON
    for x in P_BUTTON_X:
        box((x - side / 2, y - side / 2, P_ZB), (x + side / 2, y + side / 2, P_ZB + height), color("button"))
    # the back, all of it
    fan(whole, -P_Z, (0, 0, -1), color("back"))
    return [((x, y + BASE, z), *rest) for (x, y, z), *rest in verts]


def pro2_screen(w, h, k):
    """what the device shows, as in the photo of one: the mode, the microSD and the signal in the first line, the card
    and its channel under it"""
    im = Image.new("RGB", (w, h), (3, 4, 6))
    d = ImageDraw.Draw(im)
    big, small = ImageFont.truetype(TTF, int(h * 0.34)), ImageFont.truetype(TTF, int(h * 0.2))
    d.text((w * 0.03, h * 0.05), "PS2", font=big, fill=P_LIT, anchor="lt", stroke_width=k // 3, stroke_fill=P_LIT)
    d.text((w * 0.97, h * 0.05), "8M", font=big, fill=P_LIT, anchor="rt", stroke_width=k // 3, stroke_fill=P_LIT)
    d.rectangle((w * 0.33, h * 0.08, w * 0.4, h * 0.36), outline=P_LIT, width=max(k // 2, 1))   # the microSD
    for i in range(4):   # the signal's bars
        x = w * (0.46 + 0.045 * i)
        d.rectangle((x, h * (0.3 - 0.06 * i), x + w * 0.03, h * 0.36), fill=P_LIT)
    d.text((w * 0.5, h * 0.6), "PS2 Memory Card 1", font=small, fill=P_LIT, anchor="mm")
    d.text((w * 0.03, h * 0.93), "MemoryCard1", font=small, fill=P_LIT, anchor="ls")
    d.text((w * 0.97, h * 0.93), "Ch:1", font=small, fill=P_LIT, anchor="rs")
    return im


def pro2_texture():
    """the face: the board through the shell above, the block below with the bezel and the screen in it and
    "8BitMods" moulded; then the plain colors under the buttons"""
    k = 8
    w = 128 * k
    px = w / W
    h = int(round(H * px))

    def fx(x):
        return w / 2 + x * px

    def fy(y):
        return h - y * px
    # what is inside the shell, in its dark: the board all down the card, its traces and a chip, the switches
    # under the buttons and the microSD's socket at its end
    inside = Image.new("RGB", (w, h), (16, 17, 20))
    d = ImageDraw.Draw(inside)
    m = 0.2   # the shell all around the board
    d.rounded_rectangle((fx(-W / 2 + m), fy(H - m), fx(W / 2 - m), fy(m)), radius=0.12 * px, fill=P_BOARD)
    for i in range(6):   # the traces, from the contacts down to the screen
        x = -1.1 + 0.44 * i
        d.line((fx(x), fy(H - m), fx(x), fy(H - 0.62), fx(x + 0.18), fy(H - 0.8), fx(x + 0.18), fy(P_BEZEL[1])),
               fill=P_TRACE, width=int(0.035 * px))
    d.rectangle((fx(0.95), fy(3.25), fx(1.48), fy(2.98)), fill=(20, 22, 26))   # a chip
    for x in P_BUTTON_X:   # the buttons' switches
        y, side = P_BUTTON[0], P_BUTTON[1]
        d.rectangle((fx(x - side / 2 - 0.04), fy(y + side / 2 + 0.04), fx(x + side / 2 + 0.04), fy(y - side / 2 - 0.04)),
                    fill=(150, 154, 160))
    d.rectangle((fx(-0.62), fy(0.3), fx(0.62), fy(0.05)), fill=(150, 154, 160))   # the microSD's socket
    # the smoked shell over all of it, dark: thin above, thicker in the block, which shows less of what is inside
    smoke = Image.new("L", (w, h), int(0.55 * 255))
    ImageDraw.Draw(smoke).rectangle((0, fy(P_YB), w, h), fill=int(0.74 * 255))
    face = Image.composite(Image.new("RGB", (w, h), P_SMOKE), inside, smoke)
    d = ImageDraw.Draw(face)
    # the name printed on the board, white enough to be read through the shell: "MemCard", and "PRO" over a "2"
    big, small = ImageFont.truetype(TTF, int(0.5 * px)), ImageFont.truetype(TTF, int(0.22 * px))
    wb, ws = d.textlength("MemCard", font=big), d.textlength("PRO", font=small)
    left, base = fx(0) - (wb + 0.05 * px + ws) / 2, fy(3.3)
    d.text((left, base), "MemCard", font=big, fill=P_PRINT, anchor="ls", stroke_width=k // 3, stroke_fill=P_PRINT)
    xp = left + wb + 0.05 * px
    d.text((xp, base - 0.25 * px), "PRO", font=small, fill=P_PRINT, anchor="ls", stroke_width=k // 4, stroke_fill=P_PRINT)
    d.text((xp + ws / 2, base), "2", font=small, fill=P_PRINT, anchor="ms", stroke_width=k // 4, stroke_fill=P_PRINT)
    tiny = ImageFont.truetype(TTF, int(0.11 * px))
    for i, s in enumerate(("Copyright © 2023 AppCake LTD.", "MemCardPRO2")):
        d.text((fx(0), fy(3.08 - 0.14 * i)), s, font=tiny, fill=(110, 128, 120), anchor="mm")
    for xa, xb, gold in contacts(SLOT_X):   # the shell's dividers at the opening
        if not gold:
            d.rectangle((fx(xa + 0.02), fy(H), fx(xb - 0.02), fy(H - 0.7)), fill=(128, 134, 142))
    for s in (-1, 1):   # the shell's two posts, at the top corners
        x, y, r = fx(s * (W / 2 - 0.3)), fy(H - 0.3), 0.12 * px
        d.ellipse((x - r, y - r, x + r, y + r), fill=(30, 32, 36))
        r *= 0.6
        d.ellipse((x - r, y - r, x + r, y + r), fill=(96, 100, 108))
    # on the block: the bezel and the screen in it, and the name moulded between the buttons
    bx, by, bw, bh = P_BEZEL
    d.rectangle((fx(bx - bw / 2), fy(by + bh / 2), fx(bx + bw / 2), fy(by - bh / 2)), fill=(6, 7, 9))
    ox, oy, ow, oh = P_OLED
    face.paste(pro2_screen(int(ow * px), int(oh * px), k), (int(fx(ox - ow / 2)), int(fy(oy + oh / 2))))
    d.text((fx(0), fy(P_BUTTON[0])), "8BitMods", font=ImageFont.truetype(TTF, int(0.3 * px)), fill=P_MOULD, anchor="mm",
           stroke_width=k // 3, stroke_fill=P_MOULD)
    # the light on the clear shell: a soft band across it, which is what tells it is glass-like over what is inside
    sheen = Image.new("L", (w, h), 0)
    ImageDraw.Draw(sheen).polygon(((fx(-W / 2), fy(3.6)), (fx(-W / 2), fy(4.3)), (fx(W / 2), fy(2.2)), (fx(W / 2), fy(1.5))),
                                  fill=34)
    sheen = sheen.filter(ImageFilter.GaussianBlur(0.12 * px))
    ImageDraw.Draw(sheen).rectangle((fx(bx - bw / 2), fy(by + bh / 2), fx(bx + bw / 2), fy(by - bh / 2)), fill=0)
    face = Image.composite(Image.new("RGB", (w, h), (200, 206, 214)), face, sheen)
    im = face.resize((128, 128), Image.LANCZOS)
    d = ImageDraw.Draw(im)
    for name, b in pro2_patches().items():
        d.rectangle((b[0], b[1], b[2] - 1, b[3] - 1), fill=P_COLORS[name])
    return im


# ------------------------------------------------------------ the PS2 Memory Card

# The console's own memory card, the SCPH-10020 (8 MB, black), for the Memory Card in the other slot when it is sent to
# Drive whole. Measured on photos of one, from the front: narrower than the other models (as tall, 0.72 of its height
# wide) and thinner, with small round corners. At the top, the two small holes and the arrow of the way in, raised;
# under them the PlayStation symbol and "PlayStation 2" in blue, then "8MB", "MEMORY CARD" and "MAGICGATE" in grey;
# the label's wide recess down to the grip's waves on both sides, and "SONY" moulded at the bottom. The opening with
# its eight gold contacts is at the top. The face takes the texture's first 120 rows; the last ones hold the plain
# colors of the parts with no drawing
M_W, M_T = 3.2, 0.5                        # its width and thickness (its height is H, as the others')
M_ZF = M_T / 2                             # its face
M_R_TOP, M_R_BOTTOM = 0.12, 0.2            # its corners
M_WAVES, M_WAVE_Y, M_WAVE_PITCH, M_WAVE_DEPTH = 4, 0.92, 0.355, 0.05   # the grip's waves, on both sides
M_SLOT_X, M_SLOT_Z = 1.12, 0.12            # the opening at the top: half its width, half its depth
M_ROWS = 120                               # the texture's rows the face takes
M_BODY = (50, 52, 58)                      # the black plastic, where the light falls on it most (lighter than black: on
                                           # the screen's dark blue a black card would be a hole)
M_GREY = (146, 150, 158)                   # what is printed in grey
M_BLUE = (44, 124, 232)                    # and in blue
M_COLORS = {"back": (36, 37, 42), "rim": (44, 46, 52), "groove": (14, 15, 17), "bevel": (62, 65, 72),
            "slot": (8, 9, 11), "gold": (222, 176, 78), "arrow": (84, 88, 98)}

# where things are on the face, as fractions of its width (from the left) and height (from the top), measured on a
# photo of one from the front
M_HOLES = ((0.088, 0.055), (0.912, 0.055))
M_ARROW = (0.5, 0.02, 0.272, 0.041)        # its middle's x, its apex's y, its width, its height
M_LABEL = (0.032, 0.463, 0.968, 0.793)     # the recess: left, top, right, bottom
M_PS = (0.087, 0.086, 0.261, 0.179)        # the PlayStation symbol: its box's left, top, right, bottom
M_WORDMARK = (0.384, 0.925, 0.146)         # "PlayStation 2": from, to, and its middle's height
M_8MB = (0.377, 0.638, 0.307, 0.106)       # "8MB": from, to, its line, how tall the 8 is
M_MEMORY = (0.236, 0.756, 0.35)            # "MEMORY CARD": from, to, its middle's height
M_MAGICGATE = (0.302, 0.686, 0.421)        # "MAGICGATE"
M_SONY = (0.31, 0.69, 0.867)               # "SONY", moulded

# the PlayStation symbol, 160x123, one bit a pixel (zlib, base64): the P standing and the S lying in front of it,
# traced from the symbol itself, so that the card shows it as it is printed on one
PS_SYMBOL = (160, 123, (
    "eNrt1jFuxSAMAFAQUlmqn/UPVTlKLlY1HC1H4QiMGRBusHFIwEOkfqlLM/3/JMBgwCiF30ON3xQEy4JBECwLBqtgm2DgBYuCZcH6UdCiYFmwbmSy"
    "INgmWBYMJPOChd6mLur9fzbdwNVSZ0l3wRRTsoFk/qatNy38wuKLbfu3P7NX5/LuHrq7T/3N83HrbC3DWU1qvkzjgWYvoXxP3XlWyuTRnGAwmhHM"
    "sem2CMDWrifL5tp9N7O1e0xDtXqPRWpK5sgSNb3Y3lhDb4Fuw3S2iE0hsm1YNKgLthJ3MjRUtT3gBbI9ei22FgNHo5NlnDzMFCVZpFkttJnI/HFR"
    "Q82ly3VWdXXQatNMTd/QatNEC/ZZ7ENhJLBRd5wjnEFcilk2HDWgzWwLzmAfe9WcN0MFiBatWk2cPRtQhTQns1y7Tnbk3DUDLq4tb7aVvWt+aZfr"
    "Sy49b0ngfFwODVsUzI821Dw31tBifrT+iBTr3jBLseFxkVz/WtG7Dc8fl9zw+jHpOT6dvoRn1/vx6wct3pdY"
))


def mc_patch(i):
    """the i-th plain color's place: a 4x6 texel block in the rows under the face's"""
    return (2 + 6 * i, M_ROWS + 2, 6 + 6 * i, M_ROWS + 8)


def mc_color(name):
    b = mc_patch(list(M_COLORS).index(name))
    return ((b[0] + b[2]) / 2 / 128, (b[1] + b[3]) / 2 / 128)


def mc_face_uv(x, y):
    return ((0.5 + (x + M_W / 2) / M_W * 127) / 128, (0.5 + (1 - y / H) * (M_ROWS - 1)) / 128)


def mc_bright(y):
    """the shell a little lighter toward the top (in the corners' brightness: 16-bit color would band it)"""
    return int(round(0x80 * (0.72 + 0.28 * y / H)))


def mc_outline():
    """the card from the front, counterclockwise from its bottom right corner, the grip's waves on both sides; and
    which of its edges is the top"""
    pts = []

    def corner(cx, cy, r, a0):
        for k in range(7):
            a = math.radians(a0 + 15 * k)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    grip = [(M_W / 2 - M_WAVE_DEPTH * (1 - math.cos(2 * math.pi * j / 8)), M_WAVE_Y + M_WAVE_PITCH * j / 8)
            for j in range(M_WAVES * 8 + 1)]
    corner(M_W / 2 - M_R_BOTTOM, M_R_BOTTOM, M_R_BOTTOM, -90)
    pts += grip
    corner(M_W / 2 - M_R_TOP, H - M_R_TOP, M_R_TOP, 0)
    top = len(pts) - 1
    corner(-M_W / 2 + M_R_TOP, H - M_R_TOP, M_R_TOP, 90)
    pts += [(-x, y) for x, y in reversed(grip)]
    corner(-M_W / 2 + M_R_BOTTOM, M_R_BOTTOM, M_R_BOTTOM, 180)
    return pts, top


def mc_model():
    col = mc_color
    pts, top = mc_outline()
    out = outward(pts)
    bw = 0.05   # the round edge between the face and the rim
    inner = [(p[0] - n[0] * bw, p[1] - n[1] * bw) for p, n in zip(pts, out)]
    middle, zr, k = (0.0, H / 2), M_ZF - bw, 0.4
    rim = ((zr, SEAM, col("rim")), (SEAM, -SEAM, col("groove")), (-SEAM, -zr, col("rim")))
    for i, a in enumerate(pts):
        j = (i + 1) % len(pts)
        b, ia, ib, na, nb = pts[j], inner[i], inner[j], out[i], out[j]
        tri([middle + (M_ZF,), ia + (M_ZF,), ib + (M_ZF,)], (0, 0, 1), [mc_face_uv(*middle), mc_face_uv(*ia), mc_face_uv(*ib)],
            [mc_bright(middle[1]), mc_bright(ia[1]), mc_bright(ib[1])])
        tri([middle + (-M_ZF,), ia + (-M_ZF,), ib + (-M_ZF,)], (0, 0, -1), col("back"))
        for s in (1, -1):   # the round edge, front and back
            quad([ia + (s * M_ZF,), ib + (s * M_ZF,), b + (s * zr,), a + (s * zr,)],
                 [(na[0] * k, na[1] * k, s), (nb[0] * k, nb[1] * k, s), (nb[0], nb[1], s * k), (na[0], na[1], s * k)], col("bevel"))
        if i == top:   # (that edge has the opening in it)
            continue
        for z0, z1, color in rim:
            quad([a + (z0,), b + (z0,), b + (z1,), a + (z1,)], [na + (0,), nb + (0,), nb + (0,), na + (0,)], color)
    # the opening at the top, with its contacts
    x1, sx, sz, floor, y, up = pts[top][0], M_SLOT_X, M_SLOT_Z, H - SLOT_DEPTH, H, (0, 1, 0)
    for xa, xb in ((-x1, -sx), (sx, x1)):
        for z0, z1, color in rim:
            quad([(xa, y, z0), (xb, y, z0), (xb, y, z1), (xa, y, z1)], up, color)
    for za, zb in ((zr, sz), (-sz, -zr)):
        quad([(-sx, y, za), (sx, y, za), (sx, y, zb), (-sx, y, zb)], up, col("rim"))
    for z, n in ((sz, (0, 0, -1)), (-sz, (0, 0, 1))):
        quad([(-sx, y, z), (sx, y, z), (sx, floor, z), (-sx, floor, z)], n, col("slot"))
    for x, n in ((-sx, (1, 0, 0)), (sx, (-1, 0, 0))):
        quad([(x, y, -sz), (x, y, sz), (x, floor, sz), (x, floor, -sz)], n, col("slot"))
    quad([(-sx, floor, -sz), (sx, floor, -sz), (sx, floor, sz), (-sx, floor, sz)], up, col("slot"))
    for xa, xb, gold in contacts(sx):
        if gold:
            box((xa, floor, -0.06), (xb, y - 0.035, 0.06), col("gold"))
        else:
            box((xa, floor, -sz), (xb, y, sz), col("rim"))
    # the arrow of the way in: a low ridge, its two slopes catching the light differently
    ax, ay, aw, ah = M_ARROW
    apex, left, right = (0.0, H - ay * H), (-aw * M_W / 2, H - (ay + ah) * H), (aw * M_W / 2, H - (ay + ah) * H)
    ridge = (0.0, (apex[1] + left[1]) / 2)
    hz = 0.03
    tri([left + (M_ZF,), ridge + (M_ZF + hz,), apex + (M_ZF,)], unit((-0.4, 0.5, 1)), col("arrow"))
    tri([right + (M_ZF,), apex + (M_ZF,), ridge + (M_ZF + hz,)], unit((0.4, 0.5, 1)), col("arrow"))
    tri([left + (M_ZF,), right + (M_ZF,), ridge + (M_ZF + hz,)], unit((0, -0.6, 1)), col("arrow"), 0x60)
    return [((x, y + BASE, z), *rest) for (x, y, z), *rest in verts]


def ps_symbol():
    """the PlayStation symbol's bitmap, as an image (white where it is)"""
    import base64
    import zlib
    w, h, data = PS_SYMBOL
    return Image.frombytes("1", (w, h), zlib.decompress(base64.b64decode("".join(data)))).convert("L")


def mc_texture():
    """the face as on the photos, then the plain colors"""
    k = 8
    w = 128 * k
    px = w / M_W
    h = int(round(H * px))

    def fx(f):   # a fraction of the width, from the left, as a column of the drawing
        return f * w

    def fy(f):   # a fraction of the height, from the top
        return f * h
    face = Image.new("RGB", (w, h), M_BODY)
    d = ImageDraw.Draw(face)

    def font(size, weight="Bold"):
        f = ImageFont.truetype(str(ROOT / "third_party/nunito/Nunito[wght].ttf"), int(size * h))
        f.set_variation_by_name(weight)
        return f

    # the two small holes at the top corners
    for hx, hy in M_HOLES:
        r = 0.016 * h
        d.ellipse((fx(hx) - r, fy(hy) - r, fx(hx) + r, fy(hy) + r), fill=(14, 15, 17), outline=(70, 73, 80), width=k // 2)
    # the PlayStation symbol and "PlayStation 2", in blue
    x0, y0, x1, y1 = M_PS
    mask = ps_symbol().resize((int(fx(x1) - fx(x0)), int(fy(y1) - fy(y0))), Image.LANCZOS)
    face.paste(Image.new("RGB", mask.size, M_BLUE), (int(fx(x0)), int(fy(y0))), mask)

    def fitted(s, x0, x1, y, fill, weight, anchor="lm", spacing=0.0):
        """s across from x0 to x1 (fractions of the width), its middle at y"""
        size = 0.1
        f = font(size, weight)
        width = sum(d.textlength(c, font=f) for c in s) + spacing * h * (len(s) - 1)
        f = font(size * (x1 - x0) * w / width, weight)
        sp = spacing * (x1 - x0) * w / width
        left = fx(x0)
        for c in s:
            d.text((left, fy(y)), c, font=f, anchor=anchor, fill=fill)
            left += d.textlength(c, font=f) + sp * h
    fitted("PlayStation 2", *M_WORDMARK, M_BLUE, "SemiBold")
    # "8MB" (a big 8, a smaller MB on its line), "MEMORY CARD" and "MAGICGATE", in grey
    x0, x1, base, tall = M_8MB
    big = font(tall / 0.72)
    d.text((fx(x0), fy(base)), "8", font=big, anchor="ls", fill=M_GREY)
    left = fx(x0) + d.textlength("8", font=big) + 0.012 * w
    small = font(0.058)
    small = font(0.058 * (fx(x1) - left) / d.textlength("MB", font=small))
    d.text((left, fy(base)), "MB", font=small, anchor="ls", fill=M_GREY)
    fitted("MEMORY CARD", *M_MEMORY, M_GREY, "SemiBold", spacing=0.004)
    fitted("MAGICGATE", *M_MAGICGATE, M_GREY, "Bold", spacing=0.004)
    # the label's recess: a shadow along its top and left edges, a light line along its bottom one
    x0, y0, x1, y1 = M_LABEL
    d.rounded_rectangle((fx(x0), fy(y0), fx(x1), fy(y1)), radius=0.012 * h, fill=(42, 44, 49))
    d.line((fx(x0) + 0.01 * w, fy(y0), fx(x1) - 0.01 * w, fy(y0)), fill=(12, 12, 14), width=int(0.008 * h))
    d.line((fx(x0), fy(y0) + 0.01 * h, fx(x0), fy(y1) - 0.01 * h), fill=(16, 16, 19), width=int(0.006 * h))
    d.line((fx(x0) + 0.01 * w, fy(y1), fx(x1) - 0.01 * w, fy(y1)), fill=(80, 84, 92), width=int(0.005 * h))
    # "SONY", moulded: the same black, a light edge under the letters and a dark one over them
    x0, x1, y = M_SONY
    for dy, c in ((0.005, (84, 88, 97)), (-0.004, (20, 21, 24)), (0.0, (46, 48, 54))):
        fitted("SONY", x0, x1, y + dy, c, "Black", spacing=0.02)
    tex = Image.new("RGB", (128, 128), M_BODY)
    tex.paste(face.resize((128, M_ROWS), Image.LANCZOS), (0, 0))
    tex.paste(tex.crop((0, M_ROWS - 1, 128, M_ROWS)), (0, M_ROWS))   # (a row as the face's last, before the colors)
    d = ImageDraw.Draw(tex)
    for i, c in enumerate(M_COLORS.values()):
        b = mc_patch(i)
        d.rectangle((b[0], b[1], b[2] - 1, b[3] - 1), fill=c)
    return tex


# ------------------------------------------------------------ the preview

def preview(vertices, tex, path):
    """the card as SD2Cloud draws it (camera_look, transform and icon_draw in src/ui/icon.c): swung to three sides,
    large, and under them as many pixels as a PS2 gives it"""
    import numpy as np
    P = np.array([v[0] for v in vertices], float)
    N = np.array([v[1] for v in vertices], float)
    UV = np.array([v[2] for v in vertices], float) * 128
    B = np.array([v[3] if len(v) > 3 else 0x80 for v in vertices], float)
    center = (P.min(0) + P.max(0)) / 2
    radius = np.sqrt(((P - center) ** 2).sum(1)).max()
    texels = np.asarray(tex.convert("RGB"), float) // 8 * 8   # (16-bit color on the PS2)
    lights = (((-0.45, 0.6, 0.66), (0.6, 0.6, 0.6)), ((0.3, -0.8, 0.5), (0.22, 0.22, 0.3)))
    elev, dist = 0.506, 30.0

    def render(spin, scale, size=210):
        wpx, hpx = int(250 * scale), int(250 * scale)
        ca, sa, s, co = math.cos(spin), math.sin(spin), math.sin(elev), math.cos(elev)
        p = P - center
        wp = np.stack([p[:, 0] * ca + p[:, 2] * sa, p[:, 1], -p[:, 0] * sa + p[:, 2] * ca], 1)
        n = np.stack([N[:, 0] * ca + N[:, 2] * sa, N[:, 1], -N[:, 0] * sa + N[:, 2] * ca], 1)
        light = np.full((len(P), 3), 0.5)
        for direction, color in lights:
            light += np.clip(n @ np.array(direction), 0, None)[:, None] * np.array(color)
        light = np.minimum(light * B[:, None], 128) * 0.92 / 128
        rel = wp - np.array([0, dist * s, dist * co])
        depth = rel @ np.array([0, -s, -co])
        f = size * scale / (2.2 * radius) * dist
        sx = wpx / 2 + f * rel[:, 0] / depth
        sy = hpx / 2 - f * (rel @ np.array([0, co, -s])) / depth
        img = np.zeros((hpx, wpx, 3))
        yy, xx = np.mgrid[0:hpx, 0:wpx]
        g = np.clip(1 - np.hypot(xx - wpx / 2, (yy - hpx / 2) * 1.1) / (wpx * 0.55), 0, 1)[..., None]
        img[:] = np.array([10, 12, 34]) + g ** 1.6 * np.array([34, 44, 120])   # the screen's dark blue and its halo
        zbuf = np.full((hpx, wpx), 1e9)
        for t in range(0, len(P), 3):
            x, y = sx[t:t + 3], sy[t:t + 3]
            area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0])
            if abs(area) < 1e-9:
                continue
            x0, x1 = max(int(math.floor(x.min())), 0), min(int(math.ceil(x.max())) + 1, wpx)
            y0, y1 = max(int(math.floor(y.min())), 0), min(int(math.ceil(y.max())) + 1, hpx)
            if x0 >= x1 or y0 >= y1:
                continue
            gy, gx = np.mgrid[y0:y1, x0:x1] + 0.5
            w0 = ((x[1] - gx) * (y[2] - gy) - (x[2] - gx) * (y[1] - gy)) / area
            w1 = ((x[2] - gx) * (y[0] - gy) - (x[0] - gx) * (y[2] - gy)) / area
            w2 = 1 - w0 - w1
            z = w0 * depth[t] + w1 * depth[t + 1] + w2 * depth[t + 2]
            hit = (w0 >= 0) & (w1 >= 0) & (w2 >= 0) & (z < zbuf[y0:y1, x0:x1])
            if not hit.any():
                continue
            u = np.clip(w0 * UV[t, 0] + w1 * UV[t + 1, 0] + w2 * UV[t + 2, 0] - 0.5, 0, 126.999)
            v = np.clip(w0 * UV[t, 1] + w1 * UV[t + 1, 1] + w2 * UV[t + 2, 1] - 0.5, 0, 126.999)
            iu, iv = u.astype(int), v.astype(int)
            fu, fv = (u - iu)[..., None], (v - iv)[..., None]
            c = (texels[iv, iu] * (1 - fu) + texels[iv, iu + 1] * fu) * (1 - fv) + (texels[iv + 1, iu] * (1 - fu) + texels[iv + 1, iu + 1] * fu) * fv
            c *= w0[..., None] * light[t] + w1[..., None] * light[t + 1] + w2[..., None] * light[t + 2]
            img[y0:y1, x0:x1][hit] = c[hit]
            zbuf[y0:y1, x0:x1][hit] = z[hit]
        return Image.fromarray(np.clip(img, 0, 255).astype("uint8"))
    out = Image.new("RGB", (1500, 1500), (0, 0, 0))
    out.paste(render(0.0, 8).resize((1000, 1000), Image.LANCZOS), (0, 0))
    for i, spin in enumerate((0.55, -0.55)):
        out.paste(render(spin, 4).resize((500, 500), Image.LANCZOS), (1000, 500 * i))
    for i, spin in enumerate((0.3, -0.3)):
        out.paste(render(spin, 1).resize((500, 500), Image.NEAREST), (500 * i, 1000))
    out.paste(tex.resize((384, 384), Image.NEAREST), (1058, 1058))
    out.save(path)


out = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "assets"
for file, build, paint in (("sd2psx.icn", model, texture), ("memcardpro2.icn", pro2_model, pro2_texture),
                           ("memcard.icn", mc_model, mc_texture)):
    verts.clear()
    vertices = build()
    tex = paint()
    data = icn(vertices, tex)
    (out / file).write_bytes(data)
    print(file, len(data), "bytes,", len(vertices) // 3, "triangles")
    if len(sys.argv) > 2:
        preview(vertices, tex, str(pathlib.Path(sys.argv[2]) / (file[:-4] + ".png")))
        print("preview written")
