#!/usr/bin/env bash
# Packages the built eden-cli as a PS4 fake-PKG (inside the container):
#   docker/run.sh scripts/package-eden.sh 00.66     a test build, shown as v00.66
#   docker/run.sh scripts/package-eden.sh 0.1.0     a release, shown as v0.1.0
# The PKG itself needs an XX.YY version: a release M.m.p becomes (M+1).(m)(p) - 0.1.0 is 01.10 -
# so releases sort above every 00.xx test build and later releases above earlier ones.
# Output: /work/out/<content id>.pkg (the out/ folder in the project, so it's reachable outside the container).
#
# Identity follows DolphinPS4's Vulkan build: plain homebrew PAID 0x3800000000000035 and plain
# SFO values ("Vulkan (RADV on GNM) runs under the plain homebrew identity (4.5 GiB of direct
# memory)", vendor/DolphinPS4/scripts/package-dolphin.sh).
set -euo pipefail
DPS4=/work/vendor/DolphinPS4
BUILD=/build/build/eden
# ELF=/build/oelf/eden-cli-vXX.YY.elf repackages an earlier build (every packaged version is kept
# there); PKG_OUT=/work/out/<folder> puts the package somewhere other than out/.
ELF="${ELF:-$BUILD/bin/eden-cli}"
VERSION="${1:-00.01}" # what Settings > About shows (without the v)
if [[ "$VERSION" =~ ^([0-9]+)\.([0-9])\.([0-9])$ ]]; then
    APP_VER="$(printf '%02d.%d%d' $((BASH_REMATCH[1] + 1)) "${BASH_REMATCH[2]}" "${BASH_REMATCH[3]}")"
elif [[ "$VERSION" =~ ^[0-9]{2}\.[0-9]{2}$ ]]; then
    APP_VER="$VERSION"
else
    echo "version must be XX.YY (test build) or M.m.p with single-digit m and p (release): $VERSION" >&2
    exit 1
fi
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
echo "$VERSION" > "$STAGE/version.txt" # shown in Settings > About (read from /app0)

# Icon: the project's logo (assets/icon.svg: square, the PS4 rounds the corners itself), 512x512 PNG.
rsvg-convert -w 512 -h 512 -o "$STAGE/sce_sys/icon0.png" /work/assets/icon.svg

# PRO=1: an experimental package that declares PS4 Pro support (param.sfo ATTRIBUTE 0x00800000,
# "supports PS4 Pro"), into out/experimental. Without it a PS4 Pro runs the app at the base PS4's
# clocks (CPU 1.6 instead of 2.13 GHz) unless Boost Mode is on in the system settings. Untested:
# DolphinPS4's Vulkan build kept the plain identity (memory budget), and Pro mode changes the GPU.
MAKE_PKG="$DPS4/scripts/make-pkg.sh"
PKG_OUT="${PKG_OUT:-/work/out}"
mkdir -p "$PKG_OUT"
if [ "${PRO:-0}" = 1 ]; then
    sed 's/^\(    set_entry ATTRIBUTE Integer 4\) 0$/\1 0x00800000/' "$DPS4/scripts/make-pkg.sh" > "$WORK/make-pkg-pro.sh"
    grep -q 'ATTRIBUTE Integer 4 0x00800000' "$WORK/make-pkg-pro.sh" || { echo "PRO: could not set ATTRIBUTE" >&2; exit 1; }
    chmod +x "$WORK/make-pkg-pro.sh"
    export DOLPHIN_PS4_MODULES_DIR="${DOLPHIN_PS4_MODULES_DIR:-$DPS4/modules}"
    MAKE_PKG="$WORK/make-pkg-pro.sh"
    PKG_OUT="$WORK/pro-out"
    mkdir -p "$PKG_OUT" /work/out/experimental
fi
PKG="$(SFO_STYLE=plain NO_RIGHT_SPRX=1 "$MAKE_PKG" "$STAGE" "$TITLE_ID" "PS4 Switch Emu" "$APP_VER" SWITCHEMU "$PKG_OUT" | tail -1)"
if [ "${PRO:-0}" = 1 ]; then
    mv "$PKG" "/work/out/experimental/$(basename "$PKG" .pkg)-PRO.pkg"
    PKG="/work/out/experimental/$(basename "$PKG" .pkg)-PRO.pkg"
fi
mkdir -p /build/oelf && cp "$WORK/eden-cli.oelf" "/build/oelf/eden-cli-v$VERSION.oelf" && { [ "$ELF" -ef "/build/oelf/eden-cli-v$VERSION.elf" ] || cp "$ELF" "/build/oelf/eden-cli-v$VERSION.elf"; } # symbolizing samples.log
ls -la "$PKG"
