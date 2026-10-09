"""Draws SD2Cloud's interface art into assets/*.png (the build embeds them; the app decodes them with src/image.c).
Everything here is drawn from scratch: a starless nebula for the background (the stars are
drawn by the app, so they stay sharp and can twinkle), a soft light, the controller's button symbols, a translucent
memory card and a small black memory card.

usage: python tools/make_assets.py   (from the project folder; needs Pillow and numpy)

Images that are only light (white with transparency) or have few colors are saved with a palette, so the PS2 keeps
them as 8-bit textures with a color table and they take a quarter of the video memory.
"""
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets")
rng = np.random.default_rng(20261002)


def save_rgba_palette(img, name, colors=256):
    """an RGBA image as a palette PNG with transparency (8-bit texture + table on the PS2)"""
    q = img.quantize(colors=colors, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.FLOYDSTEINBERG)
    q.save(os.path.join(OUT, name), optimize=True)


def save_light(alpha, name):
    """white with this alpha (0..255): palette of 256 levels of white"""
    a = np.clip(alpha, 0, 255).astype(np.uint8)
    img = Image.fromarray(a).convert("P")
    img.putpalette([255, 255, 255] * 256)
    img.info["transparency"] = bytes(range(256))
    img.save(os.path.join(OUT, name), transparency=bytes(range(256)), optimize=True)


def smooth_noise(w, h, cell):
    """value noise: random values on a grid of cells, blurred"""
    g = rng.random((h // cell + 3, w // cell + 3)).astype(np.float32)
    big = Image.fromarray((g * 255).astype(np.uint8)).resize(((w // cell + 3) * cell, (h // cell + 3) * cell), Image.BICUBIC)
    a = np.asarray(big, np.float32)[cell:cell + h, cell:cell + w] / 255.0
    return a


def fractal(w, h):
    n = np.zeros((h, w), np.float32)
    amp, total = 1.0, 0.0
    for cell in (160, 80, 40, 20, 10):
        n += amp * smooth_noise(w, h, cell)
        total += amp
        amp *= 0.55
    return n / total


def blob(w, h, cx, cy, rx, ry):
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    return np.exp(-(((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2))


def space():
    """the nebula behind every screen: deep navy, clouds of periwinkle blue and violet, darker above the header line
    and below the footer line"""
    W, H = 640, 448
    base = np.zeros((H, W, 3), np.float32)
    base[:] = (3, 4, 12)
    n = fractal(W, H)
    clouds = [  # (x, y, rx, ry, color, strength): wide soft glows, a little texture from the noise
        (30, 260, 140, 120, (78, 100, 225), 1.00),
        (130, 120, 150, 60, (88, 70, 175), 0.55),
        (200, 330, 160, 70, (50, 70, 190), 0.45),
        (470, 280, 170, 140, (62, 88, 210), 0.70),
        (600, 120, 120, 60, (70, 60, 150), 0.40),
        (330, 190, 130, 90, (45, 55, 140), 0.30),
    ]
    for cx, cy, rx, ry, col, s in clouds:
        shape = blob(W, H, cx, cy, rx, ry) * s * (0.55 + 0.8 * n)
        for k in range(3):
            base[:, :, k] += shape * col[k]
    # wisps: thin brighter streaks of the noise
    n2 = fractal(W, H)
    wisp = np.clip((n2 - 0.6) * 5, 0, 1) ** 2
    for k, c in enumerate((25, 35, 80)):
        base[:, :, k] += wisp * c * blob(W, H, 320, 250, 300, 150)
    # header and footer bands darker, like a frame
    y = np.arange(H, dtype=np.float32)[:, None]
    frame = np.clip((y - 60) / 40, 0, 1) * np.clip((392 - y) / 40, 0, 1)
    base *= (0.25 + 0.75 * frame)[:, :, None]
    img = Image.fromarray(np.clip(base, 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2))
    img = img.resize((320, 224), Image.LANCZOS)
    img.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG).save(
        os.path.join(OUT, "space.png"), optimize=True)


def glow():
    """a round soft light: tinted and stretched by the app (behind the selected item, halos, drifting lights)"""
    s = 64
    y, x = np.mgrid[0:s, 0:s].astype(np.float32)
    d = np.sqrt((x - (s - 1) / 2) ** 2 + (y - (s - 1) / 2) ** 2) / (s / 2)
    a = np.exp(-(d ** 2) * 3.2) * np.clip(1 - d, 0, 1) ** 0.5
    save_light(a / a.max() * 255, "glow.png")


def buttons():
    """the controller's symbols, 32x32 each. On the first row cross, circle, triangle, square (the colors of the PS2
    controller, in thin strokes like the console's menus); on the second START and SELECT, drawn as the DualShock 2
    has them: a small button that points to the right and a small bar"""
    S, K = 32, 8   # drawn 8x bigger and reduced: smooth edges
    big = Image.new("RGBA", (4 * S * K, 2 * S * K), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    w = int(2.6 * K)
    cols = [(140, 150, 245, 255), (238, 86, 86, 255), (70, 205, 140, 255), (236, 130, 210, 255)]
    m = 7 * K
    # cross
    x0 = 0
    d.line([(x0 + m, m), (x0 + S * K - m, S * K - m)], fill=cols[0], width=w)
    d.line([(x0 + m, S * K - m), (x0 + S * K - m, m)], fill=cols[0], width=w)
    # circle
    x0 = S * K
    d.ellipse([x0 + m - K, m - K, x0 + S * K - m + K, S * K - m + K], outline=cols[1], width=w)
    # triangle
    x0 = 2 * S * K
    pts = [(x0 + S * K / 2, m - K * 1.5), (x0 + S * K - m + K, S * K - m), (x0 + m - K, S * K - m)]
    d.line(pts + [pts[0]], fill=cols[2], width=w, joint="curve")
    # square
    x0 = 3 * S * K
    d.rectangle([x0 + m, m, x0 + S * K - m, S * K - m], outline=cols[3], width=w)
    # START and SELECT: filled, in the dark grey rubber the DualShock 2 has them in, with a lighter edge (the light
    # on a button's rim) that keeps them in sight on the legend's dark line
    edge, grey, y0 = (150, 154, 164, 255), (84, 87, 95, 255), S * K
    pts = [(9.5 * K, y0 + 8.5 * K), (9.5 * K, y0 + 23.5 * K), (23.5 * K, y0 + 16 * K)]
    d.polygon(pts, fill=edge)
    d.line(pts + [pts[0], pts[1]], fill=edge, width=3 * K, joint="curve")   # (its corners are round)
    d.polygon(pts, fill=grey)
    d.line(pts + [pts[0], pts[1]], fill=grey, width=int(1.2 * K), joint="curve")
    d.rounded_rectangle([S * K + 5 * K, y0 + 10 * K, S * K + 27 * K, y0 + 22 * K], radius=4 * K, fill=edge)
    d.rounded_rectangle([S * K + 6 * K, y0 + 11 * K, S * K + 26 * K, y0 + 21 * K], radius=3 * K, fill=grey)
    small = big.resize((4 * S, 2 * S), Image.LANCZOS)
    # a faint glow of each color under the strokes
    halo = small.filter(ImageFilter.GaussianBlur(2.2))
    ha = np.asarray(halo, np.float32)
    ha[:, :, 3] *= 0.55
    out = Image.alpha_composite(Image.fromarray(ha.astype(np.uint8)), small)
    save_rgba_palette(out, "buttons.png")


def rounded_mask(w, h, r, k=4):
    m = Image.new("L", (w * k, h * k), 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, w * k - 1, h * k - 1], radius=r * k, fill=255)
    return np.asarray(m.resize((w, h), Image.LANCZOS), np.float32) / 255


def card():
    """the big translucent memory card on the right of the card list: teal glass, a dark label, the small triangle at
    the top; a soft halo around it. The app writes the card's number (or name) on it"""
    cw, ch, pad = 132, 168, 24
    W, H = cw + 2 * pad, ch + 2 * pad
    out = np.zeros((H, W, 4), np.float32)
    # halo
    halo = np.zeros((H, W), np.float32)
    halo[pad - 6:pad + ch + 6, pad - 6:pad + cw + 6] = rounded_mask(cw + 12, ch + 12, 16)
    halo = np.asarray(Image.fromarray((halo * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(7)), np.float32) / 255
    out[:, :, :3] = (60, 150, 170)
    out[:, :, 3] = halo * 0.28
    body = np.zeros((H, W), np.float32)
    body[pad:pad + ch, pad:pad + cw] = rounded_mask(cw, ch, 9)
    # glass: lighter at the top, a little darker at the bottom
    y = np.arange(H, dtype=np.float32)[:, None] - pad
    tone = np.clip(1.0 - y / ch * 0.35, 0.6, 1.0)
    glass = np.zeros((H, W, 4), np.float32)
    glass[:, :, 0] = 46 * tone
    glass[:, :, 1] = 112 * tone
    glass[:, :, 2] = 140 * tone
    glass[:, :, 3] = body * 0.5
    # label: a dark band in the lower half
    lab = np.zeros((H, W), np.float32)
    lx, ly, lw, lh = pad + 8, pad + int(ch * 0.47), cw - 16, int(ch * 0.36)
    lab[ly:ly + lh, lx:lx + lw] = rounded_mask(lw, lh, 5)
    lab = np.asarray(Image.fromarray((lab * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.5)), np.float32) / 255
    glass[:, :, :3] = glass[:, :, :3] * (1 - lab[:, :, None] * 0.7) + np.array([8, 22, 34], np.float32) * lab[:, :, None] * 0.7
    glass[:, :, 3] = np.maximum(glass[:, :, 3], lab * 0.7 * body)
    # rim light along the edge
    edge = body - np.asarray(Image.fromarray((body * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(5)), np.float32) / 255
    glass[:, :, :3] += edge[:, :, None] * np.array([40, 70, 70], np.float32)
    glass[:, :, 3] = np.maximum(glass[:, :, 3], edge * 0.55)
    # triangle at the top
    tri = Image.new("L", (W * 4, H * 4), 0)
    cx, ty = W * 2, (pad + 12) * 4
    ImageDraw.Draw(tri).polygon([(cx, ty), (cx + 32, ty + 22), (cx - 32, ty + 22)], fill=255)
    tri = np.asarray(tri.resize((W, H), Image.LANCZOS), np.float32) / 255
    glass[:, :, :3] = glass[:, :, :3] * (1 - tri[:, :, None] * 0.6) + np.array([10, 30, 40], np.float32) * tri[:, :, None] * 0.6
    # over the halo
    a1, a2 = out[:, :, 3:4], glass[:, :, 3:4]
    a = a2 + a1 * (1 - a2)
    rgb = (glass[:, :, :3] * a2 + out[:, :, :3] * a1 * (1 - a2)) / np.maximum(a, 1e-6)
    img = Image.fromarray(np.dstack([np.clip(rgb, 0, 255), np.clip(a * 255, 0, 255)]).astype(np.uint8))
    save_rgba_palette(img, "card.png")


def minicard():
    """a small black PS2 memory card, for the top of the saves screen"""
    W, H, K = 24, 28, 8
    big = Image.new("RGBA", (W * K, H * K), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    d.rounded_rectangle([0, 0, W * K - 1, H * K - 1], radius=2 * K, fill=(24, 24, 28, 255), outline=(70, 70, 78, 255), width=K)
    d.rectangle([3 * K, 15 * K, (W - 3) * K, (H - 4) * K], fill=(44, 44, 52, 255))
    d.polygon([(W * K / 2, 3 * K), (W * K / 2 + 4 * K, 6 * K), (W * K / 2 - 4 * K, 6 * K)], fill=(60, 60, 70, 255))
    save_rgba_palette(big.resize((W, H), Image.LANCZOS), "minicard.png", 32)


def main():
    os.makedirs(OUT, exist_ok=True)
    space()
    glow()
    buttons()
    card()
    minicard()
    for f in sorted(f for f in os.listdir(OUT) if f.endswith(".png")):   # assets/ also holds the sounds
        im = Image.open(os.path.join(OUT, f))
        print(f, im.size, im.mode, os.path.getsize(os.path.join(OUT, f)), "bytes")


if __name__ == "__main__":
    main()
