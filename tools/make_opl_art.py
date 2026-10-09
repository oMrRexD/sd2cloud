"""The art OPL shows for SD2Cloud in its Apps tab: a cover (140x200) and a logo (300x125, transparent), in the look of
the app itself: a nebula like the one of its background, the translucent memory card of its screens (assets/card.png)
with a cloud standing out of it, and the app's name in its own font.

Writes package/art/SD2CLOUD.ELF_COV.png and package/art/SD2CLOUD.ELF_LGO.png: OPL looks for an app's art in the ART
folder of the device, by the name of the ELF the app's title.cfg starts, and tools/make_release.py puts them there in
the zip. That name leaves room for one language only, so the same two files with the line under the name in
Portuguese are written to package/art/pt-BR, and the zip has them aside, to copy over the others.

The same logo, larger, is the one at the top of the README: docs/logo/dark.png and docs/logo/light.png, for the page
it is shown on, and docs/logo/pt-BR for the README in Portuguese.

All of them are kept in the repository, so this only runs when the art changes (it needs Pillow and numpy).

usage: python tools/make_opl_art.py [<a folder to write everything to instead, to look at>]
"""
import pathlib
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
TTF = str(ROOT / "third_party/varelaround/VarelaRound-Regular.ttf")
ELF = "SD2CLOUD.ELF"             # the name OPL looks the art up by: the "boot" of APPS/SD2Cloud/title.cfg
NAME = "SD2Cloud"
LINES = {"": "cloud backup", "pt-BR": "backup na nuvem"}     # the line under the name, by the folder it is written to
E = 6                            # everything is drawn this many times larger, then scaled down
PALE = (178, 206, 255)           # the line under the name
INK = {"dark": ((255, 255, 255), PALE), "light": ((22, 32, 76), (56, 96, 196))}     # the name and its line, by the page


def grid(w, h):
    """the centre of each drawn pixel, in pixels of the finished image"""
    y, x = np.mgrid[0:h * E, 0:w * E].astype(np.float32)
    return (x + 0.5) / E, (y + 0.5) / E


def sd_circle(X, Y, cx, cy, r):
    return np.hypot(X - cx, Y - cy) - r


def sd_box(X, Y, cx, cy, hw, hh, r):
    qx = np.abs(X - cx) - (hw - r)
    qy = np.abs(Y - cy) - (hh - r)
    return np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r


def sd_cloud(X, Y, cx, cy, w):
    """the distance to a cloud w wide: a pill for its base and three circles over it"""
    d = sd_box(X, Y, cx, cy + 0.12 * w, 0.5 * w, 0.16 * w, 0.16 * w)
    for ox, oy, r in ((-0.07, -0.09, 0.25), (0.21, -0.01, 0.175), (-0.31, 0.03, 0.15)):
        d = np.minimum(d, sd_circle(X, Y, cx + ox * w, cy + oy * w, r * w))
    return d


def cover_of(d):
    """how much of each pixel a shape covers, from the distance to its edge"""
    return np.clip(0.5 - d * E, 0, 1)


def layer(a, color):
    """an RGBA image out of how opaque each pixel is (0 to 1) and a color, one for all or one for each pixel"""
    h, w = a.shape
    rgb = np.empty((h, w, 3), np.float32)
    rgb[...] = color
    return Image.fromarray(np.dstack([rgb, a[..., None] * 255]).clip(0, 255).astype(np.uint8))


def blur(im, radius):
    return im.filter(ImageFilter.GaussianBlur(radius * E))


def faded(im, k):
    r, g, b, a = im.split()
    return Image.merge("RGBA", (r, g, b, a.point(lambda v: int(v * k))))


def arrow(d, cx, cy, h, width, color):
    """an arrow pointing up, in round strokes: the sign of sending"""
    wing = h * 0.42
    strokes = [((cx, cy + h / 2), (cx, cy - h / 2)),
               ((cx, cy - h / 2), (cx - wing, cy - h / 2 + wing)),
               ((cx, cy - h / 2), (cx + wing, cy - h / 2 + wing))]
    for (x0, y0), (x1, y1) in strokes:
        d.line([(x0 * E, y0 * E), (x1 * E, y1 * E)], fill=color, width=round(width * E))
        for x, y in ((x0, y0), (x1, y1)):
            r = width / 2
            d.ellipse([(x - r) * E, (y - r) * E, (x + r) * E, (y + r) * E], fill=color)


def text(W, H, s, size, x, y, color, anchor="mm"):
    im = Image.new("RGBA", (W * E, H * E), (0, 0, 0, 0))
    ImageDraw.Draw(im).text((x * E, y * E), s, font=ImageFont.truetype(TTF, round(size * E)), fill=color + (255,),
                            anchor=anchor)
    return im


def nebula(W, H):
    """the cover's background: blue and violet clouds on black, with stars, fading out at the top and at the bottom
    the way the app's screen does behind its two bars"""
    rng = np.random.default_rng(11)
    X, Y = grid(W, H)
    u, v = X / W, Y / H
    img = np.zeros((H * E, W * E, 3), np.float32)
    band = np.exp(-((v - 0.40) / 0.36) ** 2)
    img += np.array([3, 4, 14], np.float32) + (np.array([12, 16, 60], np.float32) - [3, 4, 14]) * band[..., None]

    def patch(cx, cy, sx, sy, color, k):
        g = np.exp(-(((u - cx) / sx) ** 2 + ((v - cy) / sy) ** 2))
        return g[..., None] * np.array(color, np.float32) * k

    clouds = (patch(0.10, 0.55, 0.50, 0.26, (46, 70, 235), 0.95)
              + patch(0.95, 0.22, 0.45, 0.22, (120, 74, 215), 0.80)
              + patch(0.70, 0.74, 0.55, 0.16, (30, 104, 225), 0.55)
              + patch(0.50, 0.38, 0.30, 0.20, (70, 110, 255), 0.50))

    def noise(step):
        a = rng.random((max(2, round(H / step)), max(2, round(W / step))))
        im = Image.fromarray((a * 255).astype(np.uint8)).resize((W * E, H * E), Image.BICUBIC)
        return np.asarray(im, np.float32) / 255

    n = 0.50 * noise(70) + 0.30 * noise(34) + 0.20 * noise(15)
    n = (n - n.min()) / (n.max() - n.min())
    img += clouds * (0.35 + 1.25 * n)[..., None]

    stars = Image.new("L", (W * E, H * E), 0)
    d = ImageDraw.Draw(stars)
    for _ in range(110):
        x, y = rng.random() * W, rng.random() * H
        r = 0.22 + rng.random() ** 3 * 0.55
        if 140 < y < 186 and 10 < x < 130:      # none behind the name
            continue
        d.ellipse([(x - r) * E, (y - r) * E, (x + r) * E, (y + r) * E], fill=int(90 + rng.random() * 165))
    s = np.asarray(stars, np.float32) + 1.6 * np.asarray(blur(stars, 0.9), np.float32)
    img += s[..., None] * np.array([0.86, 0.92, 1.0], np.float32)

    def step(a, b, t):
        t = np.clip((t - a) / (b - a), 0, 1)
        return t * t * (3 - 2 * t)

    img *= (step(0.0, 0.16, v) * step(0.0, 0.20, 1 - v))[..., None]
    img *= (1 - 0.35 * np.clip(((u - 0.5) / 0.62) ** 2, 0, 1))[..., None]
    return Image.fromarray(img.clip(0, 255).astype(np.uint8)).convert("RGBA")


def card(width):
    """the app's own translucent memory card, this wide"""
    im = Image.open(ROOT / "assets" / "card.png").convert("RGBA")
    return im.resize((round(width * E), round(width * E * im.height / im.width)), Image.LANCZOS)


def cloud(W, H, cx, cy, w):
    """a white cloud w wide with the arrow in it, its shadow under it and a light around it"""
    X, Y = grid(W, H)
    a = cover_of(sd_cloud(X, Y, cx, cy, w))
    t = np.clip((Y - (cy - 0.34 * w)) / (0.62 * w), 0, 1)[..., None]
    body = layer(a, np.array([255, 255, 255], np.float32) * (1 - t) + np.array([186, 214, 255], np.float32) * t)
    out = Image.new("RGBA", body.size, (0, 0, 0, 0))
    shadow = layer(cover_of(sd_cloud(X, Y, cx, cy + 0.07 * w, w)), (2, 6, 30))
    out.alpha_composite(faded(blur(shadow, 0.045 * w), 0.70))
    out.alpha_composite(faded(blur(layer(a, (120, 170, 255)), 0.10 * w), 0.85))
    out.alpha_composite(body)
    sign = Image.new("RGBA", body.size, (0, 0, 0, 0))
    arrow(ImageDraw.Draw(sign), cx - 0.02 * w, cy + 0.06 * w, 0.30 * w, 0.075 * w, (46, 92, 210, 255))
    out.alpha_composite(sign)
    return out


def cover(line):
    W, H = 140, 200
    c = nebula(W, H)
    X, Y = grid(W, H)
    light = np.exp(-(((X - 70) / 46) ** 2 + ((Y - 80) / 50) ** 2))      # behind the card
    c.alpha_composite(layer(light * 0.55, (70, 110, 255)))

    ct = card(98)
    at = ((W * E - ct.width) // 2, round(80 * E - ct.height / 2))
    shadow = Image.new("RGBA", c.size, (0, 0, 0, 0))
    shadow.paste((1, 3, 20, 255), (at[0], at[1] + 3 * E), ct.getchannel("A").point(lambda v: 255 if v > 90 else 0))
    c.alpha_composite(faded(blur(shadow, 4), 0.55))
    c.alpha_composite(ct, at)
    c.alpha_composite(cloud(W, H, 70, 62, 72))

    name = text(W, H, NAME, 23, 70, 157, (255, 255, 255))
    c.alpha_composite(faded(blur(name, 2.2), 0.55))
    c.alpha_composite(name)
    c.alpha_composite(text(W, H, line, 10, 70, 177, PALE))
    return c.convert("RGB").resize((W, H), Image.LANCZOS)


def logo(line, page="dark", scale=1):
    """the logo, scale times 300x125. On a light page the name is dark, and the card, which is translucent, stands on
    a dark shape of its own"""
    W, H = 300, 125
    ink, pale = INK[page]
    c = Image.new("RGBA", (W * E, H * E), (0, 0, 0, 0))
    ct = card(78)
    at = (round(52 * E - ct.width / 2), round(64 * E - ct.height / 2))
    if page == "light":
        c.paste((10, 16, 44, 255), at, ct.getchannel("A").point(lambda v: 255 if v > 90 else 0))
    c.alpha_composite(ct, at)
    c.alpha_composite(cloud(W, H, 52, 50, 56))
    x, size = 108, 38
    while ImageFont.truetype(TTF, round(size * E)).getlength(NAME) / E > W - x - 6:      # the name fits to the right
        size -= 0.5
    name = text(W, H, NAME, size, x, 51, ink, "lm")
    if page == "dark":
        c.alpha_composite(faded(blur(name, 2.5), 0.6))
    c.alpha_composite(name)
    c.alpha_composite(text(W, H, line, 17, x + 2, 83, pale, "lm"))
    return c.resize((W * scale, H * scale), Image.LANCZOS)


def page_logos(line):
    """the logo for the top of the README, three times larger and cut to what is drawn: for a dark page and a light one"""
    both = {page: logo(line, page, 3) for page in INK}
    box = both["dark"].getchannel("A").point(lambda v: 255 if v > 8 else 0).getbbox()
    return {page: im.crop(box) for page, im in both.items()}


def main():
    out = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "package" / "art"
    top = out / "logo" if len(sys.argv) > 1 else ROOT / "docs" / "logo"
    for folder, line in LINES.items():
        (out / folder).mkdir(parents=True, exist_ok=True)
        (top / folder).mkdir(parents=True, exist_ok=True)
        made = [(out / folder / f"{ELF}_COV.png", cover(line)), (out / folder / f"{ELF}_LGO.png", logo(line))]
        made += [(top / folder / f"{page}.png", im) for page, im in page_logos(line).items()]
        for path, im in made:
            im.save(path, optimize=True)
            print(f"{path}: {im.width}x{im.height}")


if __name__ == "__main__":
    main()
