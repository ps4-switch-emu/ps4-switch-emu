#!/usr/bin/env bash
# Builds and runs dynarmic's tests (inside the container): its own unit tests plus the fuzz tests
# that run random ARM64 instructions in the JIT and in Unicorn (a separate reference emulator)
# and compare every register. Run before shipping any change to the CPU JIT.
#   scripts/remote.sh scripts/test-dynarmic.sh ["catch2 filter"]
# EDEN_HOST_FEATURES=jaguar (default here) generates the code the PS4's Jaguar cores get.
set -euo pipefail
SRC=/work/eden
BUILD=/build/build/dynarmic-tests
FILTER="${1:-}"
if ! [ -f /usr/include/unicorn/unicorn.h ]; then # the container is thrown away afterwards
    apt-get update -qq >/dev/null && apt-get install -y -qq libunicorn-dev >/dev/null
fi
if [ ! -f "$BUILD/build.ninja" ]; then
    CC=clang-21 CXX=clang++-21 cmake -S "$SRC" -B "$BUILD" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_QT=OFF -DENABLE_QT_TRANSLATION=OFF -DENABLE_CUBEB=OFF -DENABLE_LIBUSB=OFF \
        -DENABLE_UPDATE_CHECKER=OFF -DENABLE_WEB_SERVICE=OFF -DUSE_DISCORD_PRESENCE=OFF \
        -DENABLE_OPENGL=OFF -DENABLE_WERROR=OFF -DYUZU_ROOM=OFF -DYUZU_TESTS=OFF \
        -DCPMUTIL_FORCE_BUNDLED=ON -DYUZU_USE_CPM=ON -DYUZU_USE_EXTERNAL_FFMPEG=OFF -DYUZU_USE_BUNDLED_FFMPEG=ON \
        -DYUZU_DISABLE_LLVM=ON -DSDL_UNIX_CONSOLE_BUILD=ON -DSDL_X11=OFF -DSDL_WAYLAND=OFF \
        -DDYNARMIC_TESTS=ON -DDYNARMIC_TESTS_USE_UNICORN=ON \
        -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld-21
fi
# Asserts ignored like the emulator's own builds (some random instructions trip debug asserts).
cmake -S "$SRC" -B "$BUILD" -DDYNARMIC_IGNORE_ASSERTS=ON >/dev/null
cmake --build "$BUILD" -j"${JOBS:-12}" -t dynarmic_tests
export EDEN_HOST_FEATURES="${EDEN_HOST_FEATURES-jaguar}"
TESTS="$(find "$BUILD" -name dynarmic_tests -type f -perm -u+x | head -1)"
echo "== host features: ${EDEN_HOST_FEATURES:-native}, seed ${SEED:=12345}"
# One process per test case (a failed assert aborts the whole process) with a fixed seed, so two
# runs - before and after a JIT change - can be compared line by line. Some cases fail before
# any change (the fuzzers find known differences from Unicorn): compare, don't expect all PASS.
"$TESTS" --list-tests --verbosity quiet ${FILTER:+"$FILTER"} | grep -v '^$' | while IFS= read -r name; do
    if out="$(timeout 600 "$TESTS" --rng-seed "$SEED" "$name" 2>&1)"; then
        echo "PASS  $name"
    else
        echo "FAIL  $name: $(grep -m1 -E 'FAILED|REQUIRE|SIG' <<< "$out" | tr -s ' ')"
        [ -n "${VERBOSE:-}" ] && tail -25 <<< "$out"
    fi
done
