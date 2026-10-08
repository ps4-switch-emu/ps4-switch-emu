#!/usr/bin/env bash
# Builds eden-cli for Linux x86-64 (inside the container, on the build box) - the PC baseline for
# the PS4 benchmark. Same fork, no PS4 toolchain.
#   scripts/remote.sh scripts/build-eden-linux.sh
set -euo pipefail
SRC=/work/eden
BUILD=/build/build/eden-linux
# Eden's prebuilt FFmpeg wants libva >= 2.21 (vaMapBuffer2); Ubuntu 24.04 has 2.20. Only hardware
# video decoding uses it, which the baseline never does.
if [ ! -f /build/build/va_shim.o ]; then
    mkdir -p /build/build
    printf '%s\n' 'int vaMapBuffer(void*, unsigned, void**);' \
        'int vaMapBuffer2(void* d, unsigned b, void** p, unsigned f) { (void)f; return vaMapBuffer(d, b, p); }' \
        > /build/build/va_shim.c
    clang-21 -O2 -c /build/build/va_shim.c -o /build/build/va_shim.o
fi
if [ ! -f "$BUILD/build.ninja" ]; then
    CC=clang-21 CXX=clang++-21 cmake -S "$SRC" -B "$BUILD" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_QT=OFF -DENABLE_QT_TRANSLATION=OFF -DENABLE_CUBEB=OFF -DENABLE_LIBUSB=OFF \
        -DENABLE_UPDATE_CHECKER=OFF -DENABLE_WEB_SERVICE=OFF -DUSE_DISCORD_PRESENCE=OFF \
        -DENABLE_OPENGL=OFF -DENABLE_WERROR=OFF -DYUZU_ROOM=OFF -DYUZU_TESTS=OFF \
        -DCPMUTIL_FORCE_BUNDLED=ON -DYUZU_USE_CPM=ON -DYUZU_USE_EXTERNAL_FFMPEG=OFF -DYUZU_USE_BUNDLED_FFMPEG=ON \
        -DYUZU_DISABLE_LLVM=ON -DSDL_UNIX_CONSOLE_BUILD=ON -DSDL_X11=OFF -DSDL_WAYLAND=OFF \
        "-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld-21 -Wl,--no-as-needed -lva -lva-drm -lva-x11 -ldrm -lX11 /build/build/va_shim.o"
fi
cmake --build "$BUILD" -j"${JOBS:-12}" -t yuzu-cmd
ls -la "$BUILD/bin/eden-cli"
