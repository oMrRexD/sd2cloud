"""The PS2's icon file (the one icon.sys names), as the tools of this folder write it: a model of one shape, its
128x128 texture, and no animation. The format is documented by mymc/mymc+ (Ross Ridge and contributors); src/ui/icon.c
reads it."""
import struct


def a1b5g5r5(im):
    """a Pillow RGB image as the texture's 16-bit words"""
    return [0x8000 | ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3) for r, g, b in im.getdata()]


def rle(words):
    """the texture packed as the PS2 does: a count and the word to repeat that many times, or 65536 - n and the n
    words that follow, as they are"""
    out, i, n = [], 0, len(words)
    while i < n:
        j = i
        while j < n and words[j] == words[i] and j - i < 0x7FFF:
            j += 1
        if j - i < 3:   # nothing worth a count here: as they are, up to where three of a kind begin
            j = i + 1
            while j < n and j - i < 0x80 and not (j + 2 < n and words[j] == words[j + 1] == words[j + 2]):
                j += 1
            out += [0x10000 - (j - i)] + words[i:j]
        else:
            out += [j - i, words[i]]
        i = j
    return struct.pack("<I", 2 * len(out)) + struct.pack("<%dH" % len(out), *out)


def icn(vertices, tex):
    """vertices: three for each triangle, each one its place (x, y, z: y up, z toward the viewer), its normal, its
    place in the texture (0..1) and, when it isn't the usual 0x80, how bright it is; tex: a 128x128 Pillow image"""
    def s(f):
        return max(-32768, min(32767, int(round(f * 4096))))
    out = struct.pack("<IIIfI", 0x00010000, 1, 0x0E, 1.0, len(vertices))
    for v in vertices:   # the file's y points down and its z away from the viewer
        (x, y, z), (nx, ny, nz), (u, t) = v[:3]
        c = v[3] if len(v) > 3 else 0x80
        out += struct.pack("<hhhH", s(x), s(-y), s(-z), 0x0400)
        out += struct.pack("<hhhH", s(nx), s(-ny), s(-nz), 0)
        out += struct.pack("<hhBBBB", s(u), s(t), c, c, c, 0x80)
    out += struct.pack("<IIfII", 1, 1, 1.0, 0, 1)          # the animation: one frame of the only shape
    out += struct.pack("<IIff", 0, 1, 0.0, 1.0)
    return out + rle(a1b5g5r5(tex))
