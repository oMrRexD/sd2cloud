"""Makes the interface sounds the PS2 plays: assets/sounds/<name>.flac -> assets/sounds/<name>.adp (SPU2 ADPCM, the
format audsrv uploads to the sound chip), with the ps2sdk's own encoder, adpenc.

usage: python tools/make_sounds.py   (from the project folder; needs the soundfile package to read FLAC, and adpenc
                                      from $PS2SDK/bin on the PATH)

The FLACs are the sources (lossless; 22050 Hz mono). adpenc reads 16-bit WAV, so each FLAC is decoded to a temporary
WAV first.
  startup  - the app opens
  exit     - leaving: after a backup at IGR, or out of the menu
  move     - the cursor moves
  confirm  - X
  back     - circle
"""
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

import soundfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOUNDS = ROOT / "assets" / "sounds"


def find_adpenc():
    found = shutil.which("adpenc")
    if found:
        return found
    sdk = os.environ.get("PS2SDK")
    if sdk and (pathlib.Path(sdk) / "bin" / "adpenc").exists():
        return str(pathlib.Path(sdk) / "bin" / "adpenc")
    return None


def main():
    adpenc = find_adpenc()
    if not adpenc:
        print("adpenc not found: run this where the ps2sdk is set up (PS2SDK, or adpenc on the PATH)")
        return 1
    with tempfile.TemporaryDirectory() as tmp:
        for flac in sorted(SOUNDS.glob("*.flac")):
            data, rate = soundfile.read(str(flac), dtype="int16")
            if data.ndim > 1:   # mono: the interface sounds don't need two channels
                data = data.mean(axis=1).astype("int16")
            wav = pathlib.Path(tmp) / (flac.stem + ".wav")
            soundfile.write(str(wav), data, rate, subtype="PCM_16")
            out = SOUNDS / (flac.stem + ".adp")
            subprocess.run([adpenc, str(wav), str(out)], check=True, stdout=subprocess.DEVNULL)
            print(f"{flac.name}: {rate} Hz, {len(data) / rate:.2f} s -> {out.name} ({out.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
