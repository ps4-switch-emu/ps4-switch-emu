#!/usr/bin/env bash
# Configures and builds eden-cli for the PS4 (inside the container):
#   docker/run.sh scripts/build-eden.sh [configure|build|all] [extra cmake args...]
# Build tree: /build/build/eden. Downloaded dependencies are cached in /build/cpm-cache.
set -euo pipefail
SRC=/work/eden
BUILD=/build/build/eden
export CPM_SOURCE_CACHE=/build/cpm-cache
step="${1:-all}"
shift || true

configure() {
    # Options follow eden/.ci/ps4/build.sh; SDL3 replaced SDL2 on the branch since that script was written.
    cmake -S "$SRC" -B "$BUILD" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE=/work/cmake/ps4-eden.cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_QT=OFF -DENABLE_QT_TRANSLATION=OFF \
        -DENABLE_CUBEB=OFF -DENABLE_LIBUSB=OFF \
        -DENABLE_UPDATE_CHECKER=OFF -DENABLE_WEB_SERVICE=OFF -DUSE_DISCORD_PRESENCE=OFF \
        -DENABLE_OPENGL=ON -DENABLE_WERROR=OFF \
        -DYUZU_ROOM=OFF -DYUZU_TESTS=OFF -DDYNARMIC_TESTS=OFF \
        -DCPMUTIL_FORCE_BUNDLED=ON -DYUZU_USE_CPM=ON -DYUZU_USE_EXTERNAL_FFMPEG=ON \
        -DYUZU_DISABLE_LLVM=ON \
        -DDYNARMIC_ENABLE_NO_EXECUTE_SUPPORT=OFF \
        -DSDL_UNIX_CONSOLE_BUILD=ON \
        "$@"
}

case "$step" in
    configure) configure "$@" ;;
    build) cmake --build "$BUILD" -j"${JOBS:-8}" -- -k 0 ;;
    all) [ -f "$BUILD/build.ninja" ] || configure "$@"; cmake --build "$BUILD" -j"${JOBS:-8}" -- -k 0 ;;
esac
