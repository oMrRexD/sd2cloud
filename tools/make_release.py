"""Builds the release in dist/: the zip (to extract to the root of the microSD), next to the loose ELFs (the in-app
update downloads those), SHA256SUMS.txt and RELEASE-NOTES.md (docs/release-notes.md with the table of hashes).

The zip holds the two ELFs built by make (dist/SD2CLOUD.ELF, dist/SD2CLOUD-IGR.ELF) and what is in package/: the
texts, the settings file (the same one the app creates when there is none) and its example.
usage: make, then python tools/make_release.py
"""
import hashlib
import pathlib
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
PKG = ROOT / "package"
DIST = ROOT / "dist"
WHEN = (2026, 10, 2, 12, 0, 0)   # fixed date inside the zip: the same contents give the same zip


def version():
    for line in (ROOT / "src" / "common.h").read_text(encoding="utf-8").splitlines():
        if line.startswith("#define APP_VERSION"):
            return line.split('"')[1]
    raise SystemExit("APP_VERSION not found in src/common.h")


def crlf(b):
    return b.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")


def main():
    v = version()
    zip_name = f"SD2Cloud-v{v}.zip"
    title = b"title=SD2Cloud\nboot=SD2CLOUD.ELF\n"
    files = [
        ("APPS/SD2Cloud/SD2CLOUD.ELF", (DIST / "SD2CLOUD.ELF").read_bytes()),
        ("APPS/SD2Cloud/SD2CLOUD-IGR.ELF", (DIST / "SD2CLOUD-IGR.ELF").read_bytes()),
        ("APPS/SD2Cloud/title.cfg", title),
        ("SD2Cloud/sd2cloud.ini", crlf((PKG / "sd2cloud.ini").read_bytes())),
        ("SD2Cloud/sd2cloud.example.ini", crlf((PKG / "sd2cloud.example.ini").read_bytes())),
        ("README.txt", crlf((PKG / "README.txt").read_bytes())),
        ("LEIA-ME.txt", crlf((PKG / "LEIA-ME.txt").read_bytes())),
        ("THIRD-PARTY-NOTICES.txt", crlf((PKG / "THIRD-PARTY-NOTICES.txt").read_bytes())),
        ("LICENSE.txt", crlf((ROOT / "LICENSE").read_bytes())),
        ("OFL.txt", crlf((ROOT / "third_party/varelaround/OFL.txt").read_bytes())),
    ]
    tmp = DIST / (zip_name + ".new")
    with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, data in files:
            info = zipfile.ZipInfo(name, WHEN)
            info.compress_type = zipfile.ZIP_DEFLATED
            z.writestr(info, data)
    with zipfile.ZipFile(tmp) as z:
        assert z.testzip() is None
        for name, data in files:
            assert z.read(name) == data, name
    tmp.replace(DIST / zip_name)

    h = lambda b: hashlib.sha256(b).hexdigest()
    hz, ha, hi = h((DIST / zip_name).read_bytes()), h(files[0][1]), h(files[1][1])
    (DIST / "SHA256SUMS.txt").write_text(f"{hz}  {zip_name}\n{ha}  SD2CLOUD.ELF\n{hi}  SD2CLOUD-IGR.ELF\n",
                                        encoding="utf-8", newline="\n")
    notes = DIST / "RELEASE-NOTES.md"
    t = (ROOT / "docs" / "release-notes.md").read_text(encoding="utf-8").rstrip("\n") + "\n\n" + f"""| File | SHA-256 |
|---|---|
| {zip_name} | `{hz}` |
| SD2CLOUD.ELF | `{ha}` |
| SD2CLOUD-IGR.ELF | `{hi}` |

`SD2CLOUD.ELF` and `SD2CLOUD-IGR.ELF` are the same files included in the zip, also attached individually so that SD2Cloud can update itself.
"""
    notes.write_text(t, encoding="utf-8", newline="\n")
    print(f"{zip_name} {(DIST / zip_name).stat().st_size} B  sha {hz}")
    print(f"SD2CLOUD.ELF {ha}\nSD2CLOUD-IGR.ELF {hi}")


if __name__ == "__main__":
    main()
