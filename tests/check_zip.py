"""Opens the .zip files the tests left (a card copied out as .mcd and as .ps2) with Python's own zip reader: what
SD2Cloud writes has to be a .zip for other programs too, not only for the code that wrote it.

usage: python3 check_zip.py <the tests' build folder>     (exits with an error when one isn't right)
"""
import pathlib
import sys
import zipfile

CARD = 8 * 1024 * 1024
WANT = {"exported-mcd.zip": (".mcd", CARD), "exported-ps2.zip": (".ps2", CARD // 512 * 528)}


def main():
    folder = pathlib.Path(sys.argv[1]).resolve().parent
    bad = 0
    for name, (ext, size) in WANT.items():
        path = folder / name
        try:
            with zipfile.ZipFile(path) as z:
                infos = z.infolist()
                wrong = z.testzip()
                ok = wrong is None and len(infos) == 1 and infos[0].filename.endswith(ext) and infos[0].file_size == size
                said = f"{infos[0].filename}, {infos[0].file_size} bytes" if infos else "nothing inside"
        except (OSError, zipfile.BadZipFile) as e:
            ok, said = False, str(e)
        print(f"{'ok    ' if ok else 'FAILED'}  {name}: {said}")
        bad += not ok
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
