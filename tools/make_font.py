"""The fonts of the screen: Varela Round, with the Cyrillic letters it doesn't have; and the Japanese characters, in a
font of their own.

Varela Round (third_party/varelaround) has the Latin letters and no others. The rest comes from two round fonts of
the same kind, matched to it:
- Cyrillic, from Nunito (third_party/nunito, https://github.com/google/fonts/tree/main/ofl/nunito). A Cyrillic letter
  drawn as a Latin one (А, В, Е, К, О, Р, С, Т, а, е, о, р, с, у...) is Varela Round's own Latin letter, so that a
  word reads in one font; the others are Nunito's at the weight whose strokes, once the letter is scaled, are as
  thick as Varela Round's, and scaled so that the capitals are as tall as its capitals and the small letters as tall
  as its small letters.
- Japanese, from M PLUS Rounded 1c (third_party/mplusrounded1c,
  https://github.com/google/fonts/tree/main/ofl/mplusrounded1c): the kana, the punctuation and symbols of the
  Japanese character set (JIS X 0208), its 2,965 common kanji (level 1), and any other character the Japanese texts
  use. Scaled so that its Latin capitals would be as tall as Varela Round's: the kanji keep the size their designers
  gave them beside the Latin letters. Its regular weight, whose strokes are a little thinner than Varela Round's: a
  kanji has many of them, and the program makes every letter bolder still.

Writes assets/font.ttf, the font the program draws with (src/ui/ui.c), which also has the characters of every
language's name (日本語, for the list of languages); and assets/font-ja.ttf.gz, the Japanese characters alone, with
Varela Round's lines, which the program keeps compressed and reads into memory only when one is needed (src/ui/font.c,
font_japanese): 1.5 MB that a big card file opened in Files needs for its saves' icons. tools/check_messages.py
measures with both. They are kept in the repository, so this only runs when one of the fonts, or a character of
lang/ja.txt or of a language's name, changes (it needs fontTools). The fonts are under the SIL Open Font License, and so is what comes out of them; the license keeps the
name "Varela Round" for the font as its authors made it, so this one has a name of its own.

usage: python tools/make_font.py [<out folder>]
"""
import gzip
import io
import pathlib
import sys
import unicodedata

from fontTools import subset
from fontTools.misc.roundTools import otRound
from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont
from fontTools.ttLib.tables._c_m_a_p import cmap_format_4
from fontTools.varLib import instancer

ROOT = pathlib.Path(__file__).resolve().parent.parent
VARELA = ROOT / "third_party/varelaround/VarelaRound-Regular.ttf"
NUNITO = ROOT / "third_party/nunito/Nunito[wght].ttf"
MPLUS = ROOT / "third_party/mplusrounded1c/MPLUSRounded1c-Regular.ttf"
JAPANESE_TEXTS = ROOT / "lang/ja.txt"
NAME = "SD2Cloud Round"

CYRILLIC = range(0x400, 0x530)   # Cyrillic and Cyrillic Supplement: every language written with them
# Nunito's weight: Varela Round's strokes are 91 units thick, and Nunito's at 490 are 87 (small letters) and 92
# (capitals), which the scaling below takes to 91
WEIGHT = 490
# the Cyrillic letters that are drawn as Latin ones
AS_LATIN = {
    "А": "A", "В": "B", "Е": "E", "Ѕ": "S", "І": "I", "Ј": "J", "К": "K", "М": "M", "Н": "H", "О": "O", "Р": "P",
    "С": "C", "Т": "T", "Х": "X", "Ү": "Y", "Ӏ": "I", "Ԛ": "Q", "Ԝ": "W", "Ё": "Ë", "Ї": "Ï",
    "а": "a", "е": "e", "о": "o", "р": "p", "с": "c", "у": "y", "х": "x", "ѕ": "s", "і": "i", "ј": "j", "һ": "h",
    "ӏ": "l", "ԛ": "q", "ԝ": "w", "ё": "ë", "ї": "ï",
}


def top(font, ch):
    """how tall a letter is"""
    glyphs = font.getGlyphSet()
    pen = BoundsPen(glyphs)
    glyphs[font.getBestCmap()[ord(ch)]].draw(pen)
    return pen.bounds[3]


def japanese():
    """the characters of Japanese: JIS X 0208 up to its level 1 kanji (in Shift-JIS, the codes up to 0x9872), the
    whole kana blocks, the half-width kana, and whatever else the Japanese texts have"""
    chars = set()
    for lead in range(0x81, 0x99):
        for trail in range(0x40, 0xFD if lead < 0x98 else 0x73):
            try:
                ch = bytes([lead, trail]).decode("shift_jis")
            except UnicodeDecodeError:
                continue
            if len(ch) == 1:
                chars.add(ch)
    chars |= {chr(c) for c in list(range(0x3000, 0x3100)) + list(range(0xFF61, 0xFFA0))}
    if JAPANESE_TEXTS.is_file():
        chars |= set(JAPANESE_TEXTS.read_text(encoding="utf-8"))
    return {ord(c) for c in chars if ord(c) > 0x7F}


def add(base, font, code, scale):
    """a glyph of another font, scaled, as the glyph of code: its name"""
    glyphs = font.getGlyphSet()
    name = f"uni{code:04X}"
    drawing = DecomposingRecordingPen(glyphs)
    glyphs[font.getBestCmap()[code]].draw(drawing)
    pen = TTGlyphPen(None)
    drawing.replay(TransformPen(pen, (scale, 0, 0, scale, 0, 0)))
    glyph = pen.glyph()
    glyph.recalcBounds(base["glyf"])
    base["glyf"][name] = glyph
    base["hmtx"][name] = (otRound(font["hmtx"][font.getBestCmap()[code]][0] * scale),
                          glyph.xMin if glyph.numberOfContours else 0)
    return name


def finish(font, name, description, copyright, added, out):
    """the cmap, the tables a glyph more breaks, the names; saved (gzip'd, when out ends with .gz: the same bytes each
    time)"""
    font.setGlyphOrder(font["glyf"].glyphOrder)
    if not any(table.isUnicode() for table in font["cmap"].tables):   # (the Japanese font, cut down to nothing)
        table = cmap_format_4(4)
        table.platformID, table.platEncID, table.language, table.cmap = 3, 1, 0, {}
        font["cmap"].tables.append(table)
    for table in font["cmap"].tables:
        if table.isUnicode():
            table.cmap.update(added)
    for tag in ("hdmx", "LTSH", "VDMX"):   # tables with a row per glyph, made for the glyphs it had
        if tag in font:
            del font[tag]
    font["post"].formatType = 3.0   # (no glyph names: the program doesn't need them, and there are thousands)
    names = font["name"]
    for record in list(names.names):
        if record.nameID in (16, 17, 21, 22):
            names.removeNames(nameID=record.nameID)
    for nid, text in ((0, copyright), (1, name), (3, f"{name} Regular"), (4, f"{name} Regular"),
                      (6, name.replace(" ", "") + "-Regular"), (10, description)):
        names.setName(text, nid, 3, 1, 0x409)
        names.setName(text, nid, 1, 0, 0)
    out.parent.mkdir(parents=True, exist_ok=True)
    if out.suffix == ".gz":
        data = io.BytesIO()
        font.save(data)
        out.write_bytes(gzip.compress(data.getvalue(), compresslevel=9, mtime=0))
    else:
        font.save(out)


def main():
    folder = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "assets"
    out, outJa = folder / "font.ttf", folder / "font-ja.ttf.gz"
    base = TTFont(VARELA, recalcTimestamp=False)
    # the Japanese font: Varela Round with no glyph but the one of a missing character (its lines, its size), and then
    # the Japanese characters
    ja = TTFont(VARELA, recalcTimestamp=False)
    options = subset.Options()
    options.notdef_outline, options.glyph_names, options.name_IDs, options.layout_features = True, False, ["*"], []
    options.hinting = False
    cut = subset.Subsetter(options)
    cut.populate(unicodes=[])
    cut.subset(ja)
    nunito = instancer.instantiateVariableFont(TTFont(NUNITO), {"wght": WEIGHT})
    mplus = TTFont(MPLUS)
    have = base.getBestCmap()
    capitals = top(base, "H") / top(nunito, "H")
    small = top(base, "x") / top(nunito, "x")
    added = {}
    for code in sorted(c for c in nunito.getBestCmap() if c in CYRILLIC and c not in have):
        ch = chr(code)
        if ch in AS_LATIN:
            added[code] = have[ord(AS_LATIN[ch])]
        else:
            added[code] = add(base, nunito, code, capitals if unicodedata.category(ch) == "Lu" else small)
    cyrillic = len(added)
    kanji = top(base, "H") / top(mplus, "H")
    theirs = mplus.getBestCmap()
    texts = {ord(c) for c in JAPANESE_TEXTS.read_text(encoding="utf-8") if ord(c) > 0x7F} if JAPANESE_TEXTS.is_file() else set()
    lacking = sorted(c for c in texts if c not in theirs and c not in have)   # (no font has them)
    # the main font has the characters of the languages' names too: the list of languages shows them all
    names = {ord(c) for f in (ROOT / "lang").glob("*.txt") for line in f.read_text(encoding="utf-8").splitlines()
             if line.startswith("LANGUAGE_NAME") for c in line.split("=", 1)[1].strip()}
    for code in sorted(c for c in names if c in theirs and c not in have and c not in added):
        added[code] = add(base, mplus, code, kanji)
    inJa = {}
    for code in sorted(c for c in japanese() if c in theirs and c not in have and c not in added):
        inJa[code] = add(ja, mplus, code, kanji)
    copyright = "\n".join(f["name"].getDebugName(0) for f in (base, nunito, mplus))
    finish(base, NAME, "Varela Round with the Cyrillic letters of Nunito, for SD2Cloud", copyright, added, out)
    finish(ja, NAME + " Japanese", "The Japanese characters of M PLUS Rounded 1c, with the lines of Varela Round, for "
           "SD2Cloud", "\n".join(f["name"].getDebugName(0) for f in (base, mplus)), inJa, outJa)
    print(f"{out}: {cyrillic} Cyrillic letters (Nunito at {WEIGHT}, capitals x{capitals:.3f}, small letters "
          f"x{small:.3f}), {len(added) - cyrillic} of the languages' names; {out.stat().st_size // 1024} KB")
    print(f"{outJa}: {len(inJa)} Japanese characters (M PLUS Rounded 1c x{kanji:.3f}); {outJa.stat().st_size // 1024} KB")
    if lacking:
        print(f"lang/ja.txt has {len(lacking)} characters no font has: {''.join(chr(c) for c in lacking)}")


if __name__ == "__main__":
    main()
