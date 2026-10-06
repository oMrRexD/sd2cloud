"""Makes assets/gamenames.txt, the name of each game by its ID, from third_party/gamenames/gamename.csv: the list of
HDL Batch Installer, which the sd2psXtd firmware also takes its names from (so a game is called here what the sd2psx
calls it on its screen).

One "SLES-55110<TAB>Odin Sphere" per line, sorted by ID. The ID is written the way the sd2psx names a game's folder
("SLES_551.10" in the list). The program has the file embedded and reads it as it is (cards.c).

usage: python tools/make_gamenames.py
"""
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "third_party" / "gamenames" / "gamename.csv"
OUT = ROOT / "assets" / "gamenames.txt"


def main():
    names = {}
    for line in SOURCE.read_text(encoding="utf-8", errors="replace").splitlines():
        raw, sep, name = line.partition(";")
        game = raw.strip().upper().replace("_", "-").replace(".", "")
        name = " ".join(name.split())
        if sep and name and re.fullmatch(r"[A-Z]{4}-\d{5}", game):
            names.setdefault(game, name)
    OUT.write_bytes("".join(f"{k}\t{names[k]}\n" for k in sorted(names)).encode("utf-8"))
    print(f"{len(names)} games, {OUT.stat().st_size} bytes")


if __name__ == "__main__":
    main()
