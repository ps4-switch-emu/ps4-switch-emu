#!/usr/bin/env bash
# Builds and runs the launcher menu test on this computer (see test.cpp).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
clang++ -std=c++20 -O0 -g -w -DOUT_DIR="\"$HERE/../../out\"" "$HERE/test.cpp" -o /tmp/eden-menu-test && /tmp/eden-menu-test
