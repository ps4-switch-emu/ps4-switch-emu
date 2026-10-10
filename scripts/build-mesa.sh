#!/usr/bin/env bash
# Fetches Mesa 26.0.8 + libdrm headers into /build/src, applies DolphinPS4's PS4 patch, and builds
# RADV (Vulkan on the PS4 GPU through GNM) with DolphinPS4's scripts/build-mesa.sh.
# Output: /build/build/mesa/src/amd/vulkan/libvulkan_radeon.a and friends.
#   docker/run.sh scripts/build-mesa.sh [setup]
set -euo pipefail
DPS4=/work/vendor/DolphinPS4
MESA_VER=26.0.8
LIBDRM_VER=2.4.125
SRC=/build/src
mkdir -p "$SRC"

if [ ! -d "$SRC/mesa/.git" ]; then
    rm -rf "$SRC/mesa"
    git clone --depth 1 --branch "mesa-$MESA_VER" https://gitlab.freedesktop.org/mesa/mesa.git "$SRC/mesa"
    git -C "$SRC/mesa" apply "$DPS4/patches/mesa-ps4.patch"
    git -C "$SRC/mesa" -c user.name=edenps4 -c user.email=edenps4@localhost commit -qam "DolphinPS4 mesa-ps4.patch"
fi
# Our own changes on top (patches/): the PS4 Pro experiment's raster setup.
if ! git -C "$SRC/mesa" log --format=%s | grep -q "^PS4 Pro experiment"; then
    git -C "$SRC/mesa" apply /work/patches/mesa-ps4-pro.patch
    git -C "$SRC/mesa" -c user.name=edenps4 -c user.email=edenps4@localhost commit -qam "PS4 Pro experiment (patches/mesa-ps4-pro.patch)"
fi
if [ ! -d "$SRC/libdrm" ]; then
    git clone --depth 1 --branch "libdrm-$LIBDRM_VER" https://gitlab.freedesktop.org/mesa/drm.git "$SRC/libdrm"
fi

# DolphinPS4's script expects its venv and host-tools dirs; ours are already on PATH.
mkdir -p /build/mesa-venv/bin /build/host-tools/bin
# Run it from a copy whose PS4 winsys writes its diagnostics (radv-ps4.log, gpu-fault.txt,
# gpu-hang.txt, flight/) in /data/switch-emu/logs instead of /data/DolphinPS4.
rm -rf /build/dps4-mesa && mkdir -p /build/dps4-mesa
cp -a "$DPS4/scripts" "$DPS4/port" "$DPS4/toolchain" /build/dps4-mesa/
patch -s -p1 -d /build/dps4-mesa < /work/patches/ac_ps4_drm-pro.patch # PS4 Pro levels (gpu-pro.txt)
sed -i 's#/data/DolphinPS4#/data/switch-emu/logs#g' /build/dps4-mesa/port/mesa/ac_ps4_drm.c
/build/dps4-mesa/scripts/build-mesa.sh "${1:-}"
ls -la /build/build/mesa/src/amd/vulkan/libvulkan_radeon.a
