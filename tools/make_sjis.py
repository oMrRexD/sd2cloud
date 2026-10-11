"""The table from Shift-JIS to Unicode that SD2Cloud reads a save's title with (icon.sys keeps it in Shift-JIS: kana
and kanji in the Japanese games' saves).

Writes assets/sjis.bin, which the program embeds (src/ui/icon.c): for each two-byte code of JIS X 0208 with
Microsoft's additions (lead byte 0x81-0x9F or 0xE0-0xEF, second byte 0x40-0xFC), its Unicode code as two bytes, low
byte first (0 = no character). 47 x 189 codes. Kept in the repository, so this only runs again if the table has to
change.

usage: python tools/make_sjis.py [<out file>]
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
LEADS = list(range(0x81, 0xA0)) + list(range(0xE0, 0xF0))
TRAILS = range(0x40, 0xFD)


def main():
    out = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "assets/sjis.bin"
    table = bytearray()
    known = 0
    for lead in LEADS:
        for trail in TRAILS:
            try:
                ch = bytes([lead, trail]).decode("cp932")
            except UnicodeDecodeError:
                ch = ""
            code = ord(ch) if len(ch) == 1 and ord(ch) <= 0xFFFF else 0
            known += code != 0
            table += code.to_bytes(2, "little")
    out.write_bytes(bytes(table))
    print(f"{out}: {known} characters, {len(table)} bytes")


if __name__ == "__main__":
    main()
