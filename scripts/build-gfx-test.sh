#!/usr/bin/env bash
# Builds tools/gfx-test/gfx-test.nro (CPU-drawn framebuffer test) into out/ (see scripts/build-homebrew.sh).
exec "$(dirname "$0")/build-homebrew.sh" gfx-test
