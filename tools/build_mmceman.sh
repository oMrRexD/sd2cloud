#!/bin/sh
# mmceman (the driver of the sd2psx) from https://github.com/ps2-mmce/mmceman at a fixed commit, with the fixes in
# third_party/mmceman/mmce_fs.patch. The commit has "fix: don't block mc1 (2nd port)": before it, a memory card in
# the other slot couldn't be read by mcman once mmceman was there (its pages came wrong, then not at all: -21). The
# mmceman.irx that comes with the SDK is older than the sio2man it runs with (it predates "Changes to mmceman for
# ps2sdk sio2man updates"): short exchanges work, but a long transfer can hang the bus, and reading a whole memory
# card is one. Installs into third_party/mmceman/mmceman.irx in the project,
# without touching the toolchain.
#
# Run it from the project folder, with the ps2dev environment set (PS2DEV, PS2SDK):  sh tools/build_mmceman.sh
# The source goes to third_party/src/mmceman (cloned if missing; git and network needed only for that).
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=$ROOT/third_party/src/mmceman
OUT=$ROOT/third_party/mmceman/mmceman.irx
PIN=de28936fdf45fe445554cfa0b8f058b05fa8212f

mkdir -p "$ROOT/third_party/src"
[ -d "$SRC/.git" ] || git clone --quiet https://github.com/ps2-mmce/mmceman.git "$SRC"
git -C "$SRC" cat-file -e "$PIN^{commit}" 2>/dev/null || git -C "$SRC" fetch --quiet origin   # (a clone from before it)
git -C "$SRC" checkout --quiet --force "$PIN"
git -C "$SRC" clean --quiet -fdx
git -C "$SRC" apply "$ROOT/third_party/mmceman/mmce_fs.patch"

mkdir -p "$SRC/mmceman/obj"
make -C "$SRC/mmceman" > "$ROOT/third_party/src/mmceman-make.log" 2>&1 || {
    tail -20 "$ROOT/third_party/src/mmceman-make.log"
    exit 1
}
cp "$SRC/mmceman/irx/mmceman.irx" "$OUT"

ls -la "$OUT"
sha256sum "$OUT"
