"""Runs SD2Cloud's debug build on PCSX2 through the scenarios of scenarios.py and compares what each one leaves with
what it left when it was recorded (expected/<scenario>.txt): the program's own log, without what changes from one run
to the next, and the SHA-256 of every file on the "microSD" (the cards, a .psu, the card inside a .zip).

A scenario is a folder that stands for the microSD (PCSX2's host: device), made from the cards of fixtures/, and a
script.txt, which the debug build follows instead of a controller (the letters are in src/system.c). The program's
screens and flows are what this tries: the parts with no screen have tests of their own (tests/README.md).

usage: python tests/pcsx2/run.py --pcsx2 <pcsx2-qt.exe> [--elf <SD2CLOUD-DEBUG.ELF>] [--only a,b] [--list]
                                 [--record] [--show] [--keep]

  --record   writes what each scenario left as the expected result (after looking at it: --show)
  --show     prints what each scenario left
  --keep     leaves the scenarios' folders (tests/pcsx2/work) as they ended

The PCSX2 has to be a portable one (a portable.ini or portable.txt beside it), set up (a BIOS, the first-run wizard
done) and with "Enable Host Filesystem" on: this refuses to start one that isn't, so as never to open, or change,
the PCSX2 someone plays on. It exits with an error when a scenario doesn't end or leaves something else.
"""
import argparse
import difflib
import gzip
import hashlib
import pathlib
import re
import shutil
import subprocess
import sys
import time
import zipfile

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))
from scenarios import SCENARIOS  # noqa: E402

ELF = "SD2CLOUD-DEBUG.ELF"
SETTINGS = "[general]\r\nlanguage = en\r\nask_connect = no\r\n"
# lines of the log that tell of the PC or of the emulator's own memory card, not of what SD2Cloud did
DROP = ("frames:", "helper:", "SAS package", "root signature:", "sound:", "semaphores:", "drawing thread:", "memory ",
        "usb:", "network:", "sd2psx:", "device:")
# files of a scenario's folder that aren't its result
NOT_RESULT = {ELF, "log.txt", "screen.tga", "SD2Cloud/script.txt", "SD2Cloud/debug-log.txt"}
# and, in a folder's listing, the two files that are of the run itself (the program, which every build changes, and
# the log being written)
OWN_FILES = re.compile(r"^\s+\d+: (log\.txt|SD2CLOUD-DEBUG\.ELF) \(")


def check_emulator(exe):
    """the reasons not to start that PCSX2 (none = it is fit for this)"""
    folder = exe.parent
    why = []
    if not exe.is_file():
        return [f"{exe} isn't there"]
    if not (folder / "portable.ini").exists() and not (folder / "portable.txt").exists():
        why.append("it isn't a portable PCSX2 (no portable.ini beside it): it would use, and could change, the settings "
                   "of the PCSX2 installed for playing")
    ini = folder / "inis" / "PCSX2.ini"
    text = ini.read_text(encoding="utf-8", errors="replace") if ini.is_file() else ""
    if not text:
        why.append(f"it was never set up ({ini} isn't there)")
    else:
        if not re.search(r"(?m)^SetupWizardIncomplete\s*=\s*false", text):
            why.append("its first-run wizard wasn't finished")
        if not re.search(r"(?m)^HostFs\s*=\s*true", text):
            why.append('"Enable Host Filesystem" is off (Settings > Emulation)')
    return why


def build(scenario, work, elf):
    if work.exists():
        shutil.rmtree(work)
    (work / "SD2Cloud").mkdir(parents=True)
    for card, fixture in scenario.get("cards", {}).items():
        path = work / "MemoryCards" / "PS2" / (card + ".mcd")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(gzip.decompress((HERE / "fixtures" / (fixture + ".mcd.gz")).read_bytes()))
    for name, source in scenario.get("files", {}).items():
        path = work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(source, bytes):
            path.write_bytes(source)
        elif source.endswith(".mcd"):
            path.write_bytes(gzip.decompress((HERE / "fixtures" / (source + ".gz")).read_bytes()))
        else:
            shutil.copyfile(HERE / "fixtures" / source, path)
    (work / "SD2Cloud" / "sd2cloud.ini").write_bytes((SETTINGS + scenario.get("settings", "")).encode())
    (work / "SD2Cloud" / "script.txt").write_text(scenario["script"], encoding="ascii")
    shutil.copyfile(elf, work / ELF)


def run(exe, work, seconds):
    """1 = the program got to the end of its script"""
    log = work / "log.txt"
    info = None
    if sys.platform == "win32":   # minimized, and without taking the keyboard from whoever is at the PC
        info = subprocess.STARTUPINFO()
        info.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        info.wShowWindow = 7
    p = subprocess.Popen([str(exe), "-batch", "-elf", str(work / ELF)], startupinfo=info, stdout=subprocess.DEVNULL,
                         stderr=subprocess.DEVNULL)
    ended = False
    try:
        end = time.time() + seconds
        while time.time() < end and p.poll() is None:
            time.sleep(1)
            if log.is_file() and "(end" in log.read_text(encoding="utf-8", errors="replace"):
                ended = True
                time.sleep(1.5)   # (what it was writing gets to the folder)
                break
    finally:
        p.kill()
        p.wait()
    return ended


def tidy(line):
    """a line of the log as it is on every run, or None for one that isn't the program's doing"""
    if not line.strip() or line.startswith(DROP) or OWN_FILES.match(line):
        return None
    line = re.sub(r"\b\d+ ms\b", "N ms", line)
    line = re.sub(r"\b\d+ s (reading|writing)", r"N s \1", line)
    line = re.sub(r"^SD2Cloud [0-9.]+ -- ", "SD2Cloud <version> -- ", line)
    return line.rstrip()


def sha(data):
    return hashlib.sha256(data).hexdigest()


def result(work):
    log = work / "log.txt"
    lines = log.read_text(encoding="utf-8", errors="replace").splitlines() if log.is_file() else []
    out = ["# log"] + [t for t in map(tidy, lines) if t is not None] + ["", "# files"]
    for path in sorted(p for p in work.rglob("*") if p.is_file()):
        name = path.relative_to(work).as_posix()
        if name in NOT_RESULT:
            continue
        if name.endswith(".zip"):   # the .zip has the date it was made: what counts is what is inside
            try:
                with zipfile.ZipFile(path) as z:
                    for member in z.namelist():
                        out.append(f"{sha(z.read(member))}  {name} > {member}")
            except zipfile.BadZipFile:
                out.append(f"NOT A ZIP  {name}")
        else:
            out.append(f"{sha(path.read_bytes())}  {name}")
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser(description="SD2Cloud's scenarios on PCSX2")
    ap.add_argument("--pcsx2", type=pathlib.Path)
    ap.add_argument("--elf", type=pathlib.Path, default=ROOT / "dist" / ELF)
    ap.add_argument("--only")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--record", action="store_true")
    ap.add_argument("--show", action="store_true")
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()
    names = [s["name"] for s in SCENARIOS]
    if args.list:
        for s in SCENARIOS:
            print(f"{s['name']:<24} {s['what']}")
        return 0
    only = args.only.split(",") if args.only else names
    unknown = [n for n in only if n not in names]
    if unknown or not args.pcsx2:
        ap.error(f"no scenario called {', '.join(unknown)}" if unknown else "--pcsx2 <pcsx2-qt.exe> is needed")
    why = check_emulator(args.pcsx2.resolve())
    if why:
        print("This PCSX2 isn't started:\n  - " + "\n  - ".join(why))
        return 2
    if not args.elf.is_file():
        print(f"{args.elf} isn't there: make DEBUG=1 builds it")
        return 2
    failed = 0
    for s in SCENARIOS:
        if s["name"] not in only:
            continue
        work = HERE / "work" / s["name"]
        expected = HERE / "expected" / (s["name"] + ".txt")
        build(s, work, args.elf)
        ended = run(args.pcsx2.resolve(), work, s.get("seconds", 90))
        got = result(work)
        if args.show:
            print(got)
        if not ended:
            say = "DID NOT END (its script is still waiting, or the program stopped)"
        elif args.record:
            expected.parent.mkdir(exist_ok=True)
            expected.write_text(got, encoding="utf-8", newline="\n")
            say = "recorded"
        elif not expected.is_file():
            say = "NOT RECORDED YET (--record)"
        elif expected.read_text(encoding="utf-8") != got:
            say = "DIFFERENT"
            sys.stdout.writelines(difflib.unified_diff(expected.read_text(encoding="utf-8").splitlines(True),
                                                       got.splitlines(True), "expected", "now", n=1))
        else:
            say = "ok"
        failed += say not in ("ok", "recorded")
        print(f"{say:<12} {s['name']}")
        if not args.keep and say in ("ok", "recorded"):
            shutil.rmtree(work, ignore_errors=True)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
