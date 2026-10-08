#!/usr/bin/env bash
# Builds tools/gpu-test/gpu-test.nro (deko3d examples, run in turn: GPU 3D test) into out/ (see scripts/build-homebrew.sh).
exec "$(dirname "$0")/build-homebrew.sh" gpu-test
