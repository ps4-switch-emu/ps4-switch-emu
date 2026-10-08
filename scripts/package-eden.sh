#!/usr/bin/env bash
# Packages the built eden-cli as a PS4 fake-PKG (inside the container):
#   docker/run.sh scripts/package-eden.sh [version XX.YY]
# Output: /work/out/<content id>.pkg (the out/ folder in the project, so it's reachable outside the container).
#
# Identity follows DolphinPS4's Vulkan build: plain homebrew PAID 0x3800000000000035 and plain
# SFO values ("Vulkan (RADV on GNM) runs under the plain homebrew identity (4.5 GiB of direct
# memory)", vendor/DolphinPS4/scripts/package-dolphin.sh).
set -euo pipefail
DPS4=/work/vendor/DolphinPS4
BUILD=/build/build/eden
ELF="$BUILD/bin/eden-cli"
VERSION="${1:-00.01}"
TITLE_ID=SWEM00001
STAGE=/build/build/eden-stage
WORK=/build/build/eden-fself
PAID=0x3800000000000035
AUTH_INFO=000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000

[ -f "$ELF" ] || { echo "missing $ELF: build first (scripts/build-eden.sh)" >&2; exit 1; }
rm -rf "$STAGE" "$WORK"
mkdir -p "$STAGE/sce_sys" "$WORK" /work/out

OO_PS4_TOOLCHAIN="$OPENORBIS" "$OPENORBIS/bin/create-fself-git" -in="$ELF" -out="$WORK/eden-cli.oelf" \
    --eboot "$WORK/eboot.bin" --paid "$PAID" --authinfo "$AUTH_INFO" > "$WORK/create-fself.log"
# PacBrew's create-fself sometimes duplicates the data segment; the console then refuses the app.
cmake -DOELF="$WORK/eden-cli.oelf" -P "$DPS4/toolchain/check-oelf.cmake"
cp "$WORK/eboot.bin" "$STAGE/eboot.bin"

# Icon: the project's logo (assets/icon.svg: square, the PS4 rounds the corners itself), 512x512 PNG.
rsvg-convert -w 512 -h 512 -o "$STAGE/sce_sys/icon0.png" /work/assets/icon.svg

PKG="$(SFO_STYLE=plain NO_RIGHT_SPRX=1 "$DPS4/scripts/make-pkg.sh" "$STAGE" "$TITLE_ID" "PS4 Switch Emu" "$VERSION" SWITCHEMU /work/out | tail -1)"
mkdir -p /build/oelf && cp "$WORK/eden-cli.oelf" "/build/oelf/eden-cli-v$VERSION.oelf" && cp "$ELF" "/build/oelf/eden-cli-v$VERSION.elf" # symbolizing samples.log
ls -la "$PKG"
