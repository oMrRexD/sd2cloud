"""Makes assets/gamenames.bin, the name of each game by its ID, from third_party/gamenames/gamename.csv: the list of
HDL Batch Installer, which the sd2psXtd firmware also takes its names from (so a game is called here what the sd2psx
calls it on its screen).

One "SLES-55110<TAB>Odin Sphere" per line, sorted by ID. The ID is written the way the sd2psx names a game's folder
("SLES_551.10" in the list). The lines go in blocks of about 16 KB, each compressed by itself (zlib), which the
program unpacks one at a time when it needs a name (cards.c): the list is 619 KB as text, and a big card file opened
in Files needs that memory for its saves' icons.

The file: "GNB1", the number of blocks (4 bytes, low byte first), then for each block its first ID (10 bytes), where
its data starts, how long it is and how long it is unpacked (4 bytes each); then the blocks. A block as long as it is
unpacked isn't compressed.

usage: python tools/make_gamenames.py
"""
import pathlib
import re
import struct
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "third_party" / "gamenames" / "gamename.csv"
OUT = ROOT / "assets" / "gamenames.bin"
BLOCK = 16000   # (cards.c unpacks a block into 20000 bytes)


def main():
    names = {}
    for line in SOURCE.read_text(encoding="utf-8", errors="replace").splitlines():
        raw, sep, name = line.partition(";")
        game = raw.strip().upper().replace("_", "-").replace(".", "")
        name = " ".join(name.split())
        if sep and name and re.fullmatch(r"[A-Z]{4}-\d{5}", game):
            names.setdefault(game, name)
    blocks, text, first = [], b"", None
    for k in sorted(names):
        line = f"{k}\t{names[k]}\n".encode("utf-8")
        if text and len(text) + len(line) > BLOCK:
            blocks.append((first, text))
            text = b""
        if not text:
            first = k
        text += line
    blocks.append((first, text))
    data = [zlib.compress(t, 9) for _, t in blocks]
    head = 8 + 22 * len(blocks)
    out = b"GNB1" + struct.pack("<I", len(blocks))
    at = head
    for (first, text), d in zip(blocks, data):
        out += first.encode("ascii") + struct.pack("<III", at, len(d), len(text))
        at += len(d)
    OUT.write_bytes(out + b"".join(data))
    print(f"{len(names)} games, {len(blocks)} blocks, {OUT.stat().st_size} bytes "
          f"({sum(len(t) for _, t in blocks)} as text)")


if __name__ == "__main__":
    main()
