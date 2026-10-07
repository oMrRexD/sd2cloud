"""Builds the release in dist/: the zip (to extract to the root of the microSD), next to the loose ELFs (the in-app
update downloads those), APP_SD2CLOUD.psu, SHA256SUMS.txt and RELEASE-NOTES.md (docs/release-notes.md with the table
of hashes).

APP_SD2CLOUD.psu is the app as a Save Application System (SAS) package, for those who keep their apps that way: one
APP_SD2CLOUD folder, as a save is, with what the zip has in APPS/SD2Cloud and its texts, the title.cfg the SAS asks
for and the app's own 3D icon (package/sas; the icon is made by tools/make_sas_icon.py).

The zip holds the two ELFs built by make (dist/SD2CLOUD.ELF, dist/SD2CLOUD-IGR.ELF) and what is in package/: the
texts, the settings file (the same one the app creates when there is none) and its example.
usage: make, then python tools/make_release.py
"""
import datetime
import hashlib
import pathlib
import struct
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
PKG = ROOT / "package"
DIST = ROOT / "dist"
WHEN = (2026, 10, 2, 12, 0, 0)   # fixed date inside the zip: the same contents give the same zip
SAS = "APP_SD2CLOUD"             # the folder of the Save Application System package


def version():
    for line in (ROOT / "src" / "common.h").read_text(encoding="utf-8").splitlines():
        if line.startswith("#define APP_VERSION"):
            return line.split('"')[1]
    raise SystemExit("APP_VERSION not found in src/common.h")


def crlf(b):
    return b.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")


def sas_date(folder):
    """the date the Save Application System gives an APP_ folder by its name (as its SAS-TIMESTAMPS.py does): counted
    back from the end of 2098, a second for each step of the alphabet. The PS2 browser shows the newest first, so the
    apps stay together at the top, in alphabetical order"""
    chars = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_-."
    name = folder[len("APP_"):].upper().replace("-", "")[:48]
    rank = 0
    for i in range(48):   # the name as a number in base 40, a missing letter being less than any letter
        rank = rank * len(chars) + (0 if i >= len(name) else (chars.index(name[i]) if name[i] in chars else len(chars) - 1) + 1)
    t = datetime.datetime(2099, 1, 1, 7, 59, 59) - datetime.timedelta(seconds=min(rank * 86400 // len(chars) ** 48, 86399))
    return (t.year, t.month, t.day, t.hour, t.minute, t.second)


def psu(folder, files, date):
    """a save folder as a .psu: the folder's entry, "." and "..", then each file's entry and its contents in 1 KB blocks"""
    when = struct.pack("<BBBBBBH", 0, date[5], date[4], date[3], date[2], date[1], date[0])

    def entry(mode, length, name):
        head = struct.pack("<HHI", mode, 0, length) + when + struct.pack("<II", 0, 0) + when + struct.pack("<I", 0)
        return head.ljust(64, b"\0") + name.encode("ascii").ljust(448, b"\0")
    out = entry(0x8427, len(files) + 2, folder) + entry(0x8427, 0, ".") + entry(0x8427, 0, "..")
    for name, data in files:
        out += entry(0x8497, len(data), name) + data.ljust((len(data) + 1023) // 1024 * 1024, b"\0")
    return out


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

    sas = PKG / "sas"
    cfg = (sas / "title.cfg").read_text(encoding="utf-8").replace("@VERSION@", v)
    inside = [("icon.sys", (sas / "icon.sys").read_bytes()), ("sd2cloud.icn", (sas / "sd2cloud.icn").read_bytes()),
              ("title.cfg", crlf(cfg.encode("utf-8")))]
    inside += [(name.split("/")[-1], data) for name, data in files if name.startswith("APPS/SD2Cloud/SD2CLOUD") or "/" not in name]
    package = psu(SAS, inside, sas_date(SAS))
    (DIST / (SAS + ".psu")).write_bytes(package)

    h = lambda b: hashlib.sha256(b).hexdigest()
    hz, ha, hi, hp = h((DIST / zip_name).read_bytes()), h(files[0][1]), h(files[1][1]), h(package)
    (DIST / "SHA256SUMS.txt").write_text(f"{hz}  {zip_name}\n{ha}  SD2CLOUD.ELF\n{hi}  SD2CLOUD-IGR.ELF\n{hp}  {SAS}.psu\n",
                                        encoding="utf-8", newline="\n")
    notes = DIST / "RELEASE-NOTES.md"
    t = (ROOT / "docs" / "release-notes.md").read_text(encoding="utf-8").rstrip("\n") + "\n\n" + f"""| File | SHA-256 |
|---|---|
| {zip_name} | `{hz}` |
| SD2CLOUD.ELF | `{ha}` |
| SD2CLOUD-IGR.ELF | `{hi}` |
| {SAS}.psu | `{hp}` |

`SD2CLOUD.ELF` and `SD2CLOUD-IGR.ELF` are the same files included in the zip, also attached individually so that SD2Cloud can update itself. `{SAS}.psu` is the same program as a Save Application System (SAS) package: the `{SAS}` folder, with its own 3D icon.
"""
    notes.write_text(t, encoding="utf-8", newline="\n")
    print(f"{zip_name} {(DIST / zip_name).stat().st_size} B  sha {hz}")
    print(f"SD2CLOUD.ELF {ha}\nSD2CLOUD-IGR.ELF {hi}\n{SAS}.psu {len(package)} B  sha {hp}")


if __name__ == "__main__":
    main()
