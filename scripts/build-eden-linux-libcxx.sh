#!/usr/bin/env bash
# The Linux eden-cli built against LLVM's libc++ (as the PS4 build is) instead of libstdc++: for
# telling apart bugs that come from the C++ library's behaviour (hash-table order, sort ties...).
#   docker/run.sh scripts/build-eden-linux-libcxx.sh   -> /build/build/eden-linux-libcxx/bin/eden-cli
set -euo pipefail
if ! [ -f /usr/lib/llvm-21/lib/libc++.so ] && ! ls /usr/lib/x86_64-linux-gnu/libc++.so* >/dev/null 2>&1; then
    apt-get update -qq >/dev/null && apt-get install -y -qq libc++-21-dev libc++abi-21-dev >/dev/null
fi
SRC=/work/eden
BUILD=/build/build/eden-linux-libcxx
[ -f /build/build/va_shim.o ] || { echo "run scripts/build-eden-linux.sh once first (va_shim.o)"; exit 1; }
if [ ! -f "$BUILD/build.ninja" ]; then
    CC=clang-21 CXX=clang++-21 CXXFLAGS="-stdlib=libc++" LDFLAGS="-stdlib=libc++" cmake -S "$SRC" -B "$BUILD" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_QT=OFF -DENABLE_QT_TRANSLATION=OFF -DENABLE_CUBEB=OFF -DENABLE_LIBUSB=OFF \
        -DENABLE_UPDATE_CHECKER=OFF -DENABLE_WEB_SERVICE=OFF -DUSE_DISCORD_PRESENCE=OFF \
        -DENABLE_OPENGL=OFF -DENABLE_WERROR=OFF -DYUZU_ROOM=OFF -DYUZU_TESTS=OFF \
        -DCPMUTIL_FORCE_BUNDLED=ON -DYUZU_USE_CPM=ON -DYUZU_USE_EXTERNAL_FFMPEG=OFF -DYUZU_USE_BUNDLED_FFMPEG=ON \
        -DYUZU_DISABLE_LLVM=ON -DSDL_UNIX_CONSOLE_BUILD=ON -DSDL_X11=OFF -DSDL_WAYLAND=OFF \
        "-DCMAKE_EXE_LINKER_FLAGS=-stdlib=libc++ -fuse-ld=lld-21 -Wl,--no-as-needed -lva -lva-drm -lva-x11 -ldrm -lX11 /build/build/va_shim.o"
fi
cmake --build "$BUILD" -j"${JOBS:-12}" -t yuzu-cmd
ls -la "$BUILD/bin/eden-cli"
