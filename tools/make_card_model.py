"""The memory card SD2Cloud shows turning while it works on a card (src/app/work.c): an sd2psx, in the shape of the
PSXMemCard Gen2 (one of the devices its firmware runs on). A dark smoked shell with round edges and the device's name
on it, the grip's waves low on its sides, the arrow of the way in and the two screws at the top, the small black and
white screen in its raised frame showing what the device shows, the two buttons at its end, and at the other end the
opening with its eight gold contacts.

Writes assets/sd2psx.icn: the model and its 128x128 texture, in the PS2's icon format, which the program embeds and
reads as it reads a save's icon (icon_make_sd2psx in src/ui/icon.c). It is kept in the repository, so this only runs
when the model changes (it needs Pillow, and numpy for the preview).

usage: python tools/make_card_model.py [<out file> [<a preview.png to look at>]]
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


out = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "assets" / "sd2psx.icn"
tex = texture()
vertices = model()
data = icn(vertices, tex)
out.write_bytes(data)
print(out.name, len(data), "bytes,", len(vertices) // 3, "triangles")
if len(sys.argv) > 2:
    preview(vertices, tex, sys.argv[2])
    print("preview written")
