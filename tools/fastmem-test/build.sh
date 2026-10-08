#!/usr/bin/env bash
# Builds and runs a host-side test of the PS4 fastmem arena (eden/src/common/host_memory.cpp,
# class HostMemory::PS4Arena) against a simulated PS4 address space.
#   scripts/remote.sh tools/fastmem-test/build.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC=/work/eden/src/common/host_memory.cpp
OUT=/build/build/fastmem-test
mkdir -p "$OUT"
# The class body: from its comment block to the #endif closing the PS4 section.
awk '/^class HostMemory::PS4Arena \{/{p=1} p{print} p&&/^};/{exit}' "$SRC" \
    | sed 's/class HostMemory::PS4Arena/class PS4Arena/' > "$OUT/arena_body.inc"
clang++-21 -std=c++20 -O1 -g -fsanitize=undefined -fsanitize-trap=all -I"$OUT" "$HERE/test.cpp" -o "$OUT/fastmem-test"
"$OUT/fastmem-test"
