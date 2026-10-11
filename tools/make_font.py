"""The font of the screen: Varela Round, with the Cyrillic letters it doesn't have.

Varela Round (third_party/varelaround) has the Latin letters and no Cyrillic ones. Nunito
(third_party/nunito, from https://github.com/google/fonts/tree/main/ofl/nunito) is a round font of the same kind that
has them, so they come from it, matched to Varela Round:
- a Cyrillic letter drawn as a Latin one (А, В, Е, К, О, Р, С, Т, а, е, о, р, с, у...) is Varela Round's own Latin
  letter, so that a word reads in one font;
- the others are Nunito's at the weight whose strokes, once the letter is scaled, are as thick as Varela Round's,
  and scaled so that the capitals are as tall as its capitals and the small letters as tall as its small letters.

Writes assets/font.ttf, which the program embeds (src/ui/ui.c) and tools/check_messages.py measures with. It is kept
in the repository, so this only runs when one of the two fonts changes (it needs fontTools). Both fonts are under the
SIL Open Font License, and so is what comes out of them; the license keeps the name "Varela Round" for the font as
its authors made it, so this one has a name of its own.

usage: python tools/make_font.py [<out file>]
"""
import pathlib
import sys
import unicodedata

from fontTools.misc.roundTools import otRound
from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

ROOT = pathlib.Path(__file__).resolve().parent.parent
VARELA = ROOT / "third_party/varelaround/VarelaRound-Regular.ttf"
NUNITO = ROOT / "third_party/nunito/Nunito[wght].ttf"
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


def main():
    out = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "assets/font.ttf"
    base = TTFont(VARELA, recalcTimestamp=False)
    nunito = instancer.instantiateVariableFont(TTFont(NUNITO), {"wght": WEIGHT})
    have, theirs, glyphs = base.getBestCmap(), nunito.getBestCmap(), nunito.getGlyphSet()
    capitals = top(base, "H") / top(nunito, "H")
    small = top(base, "x") / top(nunito, "x")
    added = {}
    for code in sorted(c for c in theirs if c in CYRILLIC and c not in have):
        ch = chr(code)
        if ch in AS_LATIN:
            added[code] = have[ord(AS_LATIN[ch])]
            continue
        scale = capitals if unicodedata.category(ch) == "Lu" else small
        name = f"uni{code:04X}"
        drawing = DecomposingRecordingPen(glyphs)
        glyphs[theirs[code]].draw(drawing)
        pen = TTGlyphPen(None)
        drawing.replay(TransformPen(pen, (scale, 0, 0, scale, 0, 0)))
        glyph = pen.glyph()
        glyph.recalcBounds(base["glyf"])
        base["glyf"][name] = glyph
        base["hmtx"][name] = (otRound(nunito["hmtx"][theirs[code]][0] * scale), glyph.xMin if glyph.numberOfContours else 0)
        added[code] = name
    base.setGlyphOrder(base["glyf"].glyphOrder)
    for table in base["cmap"].tables:
        if table.isUnicode():
            table.cmap.update(added)
    for tag in ("hdmx", "LTSH", "VDMX"):   # tables with a row per glyph, made for the glyphs it had
        if tag in base:
            del base[tag]
    names = base["name"]
    copyright = (f"{names.getDebugName(0)}\n{nunito['name'].getDebugName(0)}")
    for record in list(names.names):
        if record.nameID in (16, 17, 21, 22):
            names.removeNames(nameID=record.nameID)
    for nid, text in ((0, copyright), (1, NAME), (3, f"{NAME} Regular"), (4, f"{NAME} Regular"),
                      (6, NAME.replace(" ", "") + "-Regular"),
                      (10, "Varela Round with the Cyrillic letters of Nunito, for SD2Cloud")):
        names.setName(text, nid, 3, 1, 0x409)
        names.setName(text, nid, 1, 0, 0)
    out.parent.mkdir(parents=True, exist_ok=True)
    base.save(out)
    print(f"{out}: {len(added)} Cyrillic letters ({sum(1 for n in added.values() if n.startswith('uni'))} from "
          f"Nunito at {WEIGHT}, capitals x{capitals:.3f}, small letters x{small:.3f})")


if __name__ == "__main__":
    main()
