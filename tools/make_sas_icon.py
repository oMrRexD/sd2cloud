"""SD2Cloud's own icon, for the PS2 browser (what a Save Application System package needs): a memory card in the way
of the sd2psx family (dark smoked shell, the grip's waves low on its sides, a small black and white screen in a raised
frame), with the app's name and a cloud standing out of its face.

Writes package/sas/sd2cloud.icn (the model and its 128x128 texture, in the PS2's icon format) and package/sas/icon.sys
(title, lights, the browser's background): tools/make_release.py puts them in APP_SD2CLOUD.psu. They are kept in the
repository, so this only runs when the icon changes (it needs Pillow).

usage: python tools/make_sas_icon.py [<out folder> [<folder for a preview.png and an APP_SD2CLOUD.psu to look at>]]
"""
import math
import pathlib
import struct
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
TTF = str(ROOT / "third_party/varelaround/VarelaRound-Regular.ttf")
BASE = 0.25                      # how far above the ground it stands (an icon fills a box of 5 units)
LIT = (238, 242, 248)            # what the screen shows: white on black, as the sd2psx's own

# the texture: the card's face on rows 0-111, plain colors on 112-127 (one for each part that has no drawing)
SHELL, EDGE, BLUE, GREY, BLACK = [((16 + 26 * i) / 128, 120 / 128) for i in range(5)]
verts = []


def quad(p, n, uv):
    """four corners (any order around), their normal and texture corners: two triangles, counterclockwise from outside"""
    a, b, c = p[0], p[1], p[2]
    cross = ((b[1] - a[1]) * (c[2] - a[2]) - (b[2] - a[2]) * (c[1] - a[1]), (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2]),
             (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]))
    order = (0, 1, 2, 0, 2, 3) if sum(x * y for x, y in zip(cross, n)) > 0 else (0, 2, 1, 0, 3, 2)
    if not isinstance(uv[0], tuple):
        uv = [uv] * 4
    for i in order:
        verts.append((p[i], n, uv[i]))


def tri(p, n, uv):
    """three corners (any order around), their normal and texture corners: one triangle, counterclockwise from outside"""
    a, b, c = p
    cross = ((b[1] - a[1]) * (c[2] - a[2]) - (b[2] - a[2]) * (c[1] - a[1]), (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2]),
             (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]))
    if not isinstance(uv[0], tuple):
        uv = [uv] * 3
    for i in ((0, 1, 2) if sum(x * y for x, y in zip(cross, n)) > 0 else (0, 2, 1)):
        verts.append((p[i], n, uv[i]))


def slab(shape, z0, z1, color, face=None):
    """a flat shape in x and y standing out of the face from z0 to z1: its front and its rim"""
    cx, cy = sum(p[0] for p in shape) / len(shape), sum(p[1] for p in shape) / len(shape)
    for i, (xa, ya) in enumerate(shape):
        xb, yb = shape[(i + 1) % len(shape)]
        tri([(cx, cy, z1), (xa, ya, z1), (xb, yb, z1)], (0, 0, 1), face or color)
        nx, ny = yb - ya, xa - xb
        l = math.hypot(nx, ny) or 1
        nx, ny = nx / l, ny / l
        if nx * ((xa + xb) / 2 - cx) + ny * ((ya + yb) / 2 - cy) < 0:
            nx, ny = -nx, -ny
        quad([(xa, ya, z0), (xb, yb, z0), (xb, yb, z1), (xa, ya, z1)], (nx, ny, 0), color)


def rect(x0, y0, x1, y1):
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def cloud_shape(cx, cy, s):
    """a cloud's outline, about 2.4 s wide: three bumps over a flat bottom"""
    out = [(cx + 1.2 * s, cy - 0.7 * s)]
    for bx, by, r, a0, a1 in ((0.65, -0.18, 0.55, -80, 100), (-0.15, 0.15, 0.62, 30, 170), (-0.7, -0.3, 0.5, 100, 260)):
        for k in range(6):
            a = math.radians(a0 + (a1 - a0) * k / 5)
            out.append((cx + (bx + r * math.cos(a)) * s, cy + (by + r * math.sin(a)) * s))
    out.append((cx - 0.75 * s, cy - 0.7 * s))
    return out


CW, CH, CT = 3.3, 4.5, 0.5      # the card: width, height, thickness
FACE = (0, 0, 1, 112 / 128)      # its face in the texture (rows 0-111)


def card_outline(w, h, r, y0):
    """a memory card seen from the front, counterclockwise: round corners, and the grip's scallops low on both sides"""
    pts = []

    def corner(cx, cy, a0):
        for k in range(5):
            a = math.radians(a0 + 90 * k / 4)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    corner(w / 2 - r, y0 + r, -90)
    for i in range(4):
        for k in range(1, 9):
            pts.append((w / 2 - 0.04 * (1 - math.cos(2 * math.pi * k / 8)), y0 + 0.5 + (i + k / 8) * 0.42))
    corner(w / 2 - r, y0 + h - r, 0)
    corner(-w / 2 + r, y0 + h - r, 90)
    for i in range(4):
        for k in range(1, 9):
            pts.append((-w / 2 + 0.04 * (1 - math.cos(2 * math.pi * k / 8)), y0 + 0.5 + (4 - i - k / 8) * 0.42))
    corner(-w / 2 + r, y0 + r, 180)
    return pts


def plate(shape, z0, z1, color, box):
    """a flat shape in x and y, as thick as z0..z1: its face with the texture's box across it, its back and its rim"""
    xs, ys = [p[0] for p in shape], [p[1] for p in shape]

    def uv(p):
        return (box[0] + (p[0] - min(xs)) / (max(xs) - min(xs)) * (box[2] - box[0]),
                box[3] - (p[1] - min(ys)) / (max(ys) - min(ys)) * (box[3] - box[1]))
    c = (0.0, (min(ys) + max(ys)) / 2)
    for i, p in enumerate(shape):
        q = shape[(i + 1) % len(shape)]
        tri([(c[0], c[1], z1), (p[0], p[1], z1), (q[0], q[1], z1)], (0, 0, 1), [uv(c), uv(p), uv(q)])
        tri([(c[0], c[1], z0), (p[0], p[1], z0), (q[0], q[1], z0)], (0, 0, -1), color)
        nx, ny = q[1] - p[1], p[0] - q[0]
        l = math.hypot(nx, ny) or 1
        nx, ny = nx / l, ny / l
        if nx * ((p[0] + q[0]) / 2 - c[0]) + ny * ((p[1] + q[1]) / 2 - c[1]) < 0:
            nx, ny = -nx, -ny
        quad([(p[0], p[1], z0), (q[0], q[1], z0), (q[0], q[1], z1), (p[0], p[1], z1)], (nx, ny, 0), color)


SX, SY0, SY1 = 1.02, 0.95, 2.55   # the screen: half its width, and where it starts and ends above the card's bottom
NAME_Y, CLOUD_Y = 3.22, 3.8       # the name's middle and the cloud's, above the card's bottom


def model():
    y0, z = BASE, CT / 2
    plate(card_outline(CW, CH, 0.3, y0), -z, z, SHELL, FACE)
    for r in (rect(-SX - 0.14, y0 + SY0 - 0.14, SX + 0.14, y0 + SY0), rect(-SX - 0.14, y0 + SY1, SX + 0.14, y0 + SY1 + 0.14),
              rect(-SX - 0.14, y0 + SY0, -SX, y0 + SY1), rect(SX, y0 + SY0, SX + 0.14, y0 + SY1)):
        slab(r, z, z + 0.11, EDGE)   # the screen's frame stands out; the lit panel sits a little inside it
    u0, u1 = 0.5 - SX / CW, 0.5 + SX / CW
    v0, v1 = (1 - SY1 / CH) * FACE[3], (1 - SY0 / CH) * FACE[3]
    quad([(-SX, y0 + SY0, z + 0.03), (SX, y0 + SY0, z + 0.03), (SX, y0 + SY1, z + 0.03), (-SX, y0 + SY1, z + 0.03)], (0, 0, 1),
         [(u0, v1), (u1, v1), (u1, v0), (u0, v0)])
    for x in (-0.9, 0.9):   # the two buttons
        slab(rect(x - 0.28, y0 + 0.3, x + 0.28, y0 + 0.48), z, z + 0.07, GREY)
    slab(cloud_shape(-0.02, y0 + CLOUD_Y, 0.34), z, z + 0.11, BLUE)   # the cloud, over the name
    return verts


def cloud(d, cx, cy, s, fill):
    d.polygon([(x, 2 * cy - y) for x, y in cloud_shape(cx, cy, s)], fill=fill)


def texture():
    """the card's face, drawn large in its own proportions and brought down to the texture's rows 0-111; plain colors
    under it"""
    k = 8
    w, h = 128 * k, int(128 * k * CH / CW)
    px = w / CW   # pixels to a unit of the model

    def fy(y):   # a height above the card's bottom, as a row of the drawing
        return h - y * px
    face = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(face)
    for y in range(h):   # the smoked shell: dark, a little lighter toward the top
        d.line((0, y, w, y), fill=(58 - 26 * y // h, 62 - 27 * y // h, 71 - 30 * y // h))
    for x in (0.22, 0.5, 0.78):   # what shows through the shell at the top: ribs, the screws, the arrow of the way in
        d.rectangle((w * x - 3 * k, 0, w * x + 3 * k, fy(CH - 0.55)), fill=(44, 47, 55))
    for x in (0.09, 0.91):
        d.ellipse((w * x - 5 * k, fy(CH - 0.3) - 5 * k, w * x + 5 * k, fy(CH - 0.3) + 5 * k), fill=(30, 32, 38), outline=(92, 98, 110), width=k)
    d.polygon(((w * 0.5, fy(CH - 0.14)), (w * 0.4, fy(CH - 0.36)), (w * 0.6, fy(CH - 0.36))), fill=(98, 104, 118))
    font = ImageFont.truetype(TTF, int(0.56 * px))
    name = "SD2Cloud"
    d.text((w / 2, fy(NAME_Y)), name, font=font, anchor="mm", fill=(244, 247, 252), stroke_width=k // 2, stroke_fill=(244, 247, 252))
    # where the screen is, what it shows: a title bar and a cloud taking a card in
    x0, x1, y0, y1 = w / 2 - SX * px, w / 2 + SX * px, fy(SY1), fy(SY0)
    d.rectangle((x0 - 2 * k, y0 - 2 * k, x1 + 2 * k, y1 + 2 * k), fill=(3, 4, 8))
    d.rectangle((x0 + 3 * k, y0 + 3 * k, x1 - 3 * k, y0 + 12 * k), fill=LIT)
    cx, cy = w / 2, y0 + (y1 - y0) * 0.6
    cloud(d, cx, cy, 0.4 * px, LIT)
    cy += 9 * k   # (the arrow sits low in the cloud, small enough to leave its outline whole)
    d.rectangle((cx - 1.5 * k, cy - 2 * k, cx + 1.5 * k, cy + 5 * k), fill=(3, 4, 8))
    d.polygon(((cx, cy - 8 * k), (cx - 5 * k, cy - 2 * k), (cx + 5 * k, cy - 2 * k)), fill=(3, 4, 8))
    im = Image.new("RGB", (128, 128), (0, 0, 0))
    im.paste(face.resize((128, 112), Image.LANCZOS), (0, 0))
    d = ImageDraw.Draw(im)
    for i, c in enumerate(((44, 47, 54), (78, 84, 96), (112, 190, 255), (96, 103, 118), (3, 4, 8))):
        d.rectangle((3 + 26 * i, 112, 28 + 26 * i, 127), fill=c)
    return im


def a1b5g5r5(im):
    out = bytearray()
    for r, g, b in im.getdata():
        out += struct.pack("<H", 0x8000 | ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3))
    return bytes(out)


def icn(vertices, tex):
    def s(f):
        return max(-32768, min(32767, int(round(f * 4096))))
    out = struct.pack("<IIIfI", 0x00010000, 1, 0x07, 1.0, len(vertices))
    for (x, y, z), (nx, ny, nz), (u, t) in vertices:   # the file's y points down and its z away from the viewer
        out += struct.pack("<hhhH", s(x), s(-y), s(-z), 0x0400)
        out += struct.pack("<hhhH", s(nx), s(-ny), s(-nz), 0)
        out += struct.pack("<hhBBBB", s(u), s(t), 0x80, 0x80, 0x80, 0x80)
    out += struct.pack("<IIfII", 1, 1, 1.0, 0, 1)          # the animation: one frame of the only shape
    out += struct.pack("<IIff", 0, 1, 0.0, 1.0)
    return out + a1b5g5r5(tex)


def icon_sys(title, icon_file):
    t = title.encode("shift_jis")
    out = b"PS2D" + struct.pack("<HHI", 0, len(t), 0) + struct.pack("<I", 0x40)
    for c in ((22, 40, 104), (34, 60, 150), (8, 14, 44), (16, 28, 84)):   # the browser's background, corner by corner
        out += struct.pack("<4i", c[0], c[1], c[2], 0)
    for dvec in ((0.45, 0.6, 0.66), (-0.3, -0.8, 0.5), (0.0, 0.3, -0.9)):
        out += struct.pack("<4f", dvec[0], dvec[1], dvec[2], 0.0)
    for c in ((0.6, 0.6, 0.6), (0.22, 0.22, 0.3), (0.1, 0.1, 0.12)):
        out += struct.pack("<4f", c[0], c[1], c[2], 0.0)
    out += struct.pack("<4f", 0.5, 0.5, 0.5, 0.0)
    out += t.ljust(68, b"\0")
    for _ in range(3):   # the same icon in the list, while copying and while deleting
        out += icon_file.encode("ascii").ljust(64, b"\0")
    return out.ljust(964, b"\0")


def psu(folder, files):
    when = struct.pack("<BBBBBBH", 0, 0, 0, 12, 7, 10, 2026)

    def entry(mode, length, name):
        return (struct.pack("<HHI", mode, 0, length) + when + struct.pack("<II", 0, 0) + when + struct.pack("<I", 0)).ljust(64, b"\0") + \
            name.encode("ascii").ljust(448, b"\0")
    out = entry(0x8427, len(files) + 2, folder) + entry(0x8427, 0, ".") + entry(0x8427, 0, "..")
    for name, data in files:
        out += entry(0x8497, len(data), name) + data.ljust((len(data) + 1023) // 1024 * 1024, b"\0")
    return out


out = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "package" / "sas"
out.mkdir(parents=True, exist_ok=True)
tex = texture()
data = icn(model(), tex)
sysd = icon_sys("ＳＤ２Ｃｌｏｕｄ", "sd2cloud.icn")
(out / "sd2cloud.icn").write_bytes(data)
(out / "icon.sys").write_bytes(sysd)
print("sd2cloud.icn", len(data), "bytes,", len(verts) // 3, "triangles; icon.sys", len(sysd), "bytes")
if len(sys.argv) > 2:
    tex.resize((512, 512), Image.NEAREST).save(pathlib.Path(sys.argv[2], "preview.png"))
    title = b"title=SD2Cloud\nboot=SD2CLOUD.ELF\n"
    pathlib.Path(sys.argv[2], "APP_SD2CLOUD.psu").write_bytes(psu("APP_SD2CLOUD", [("icon.sys", sysd), ("sd2cloud.icn", data), ("title.cfg", title)]))
    print("APP_SD2CLOUD.psu written")
