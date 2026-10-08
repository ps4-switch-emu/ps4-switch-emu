#!/usr/bin/env bash
# Builds tools/audio-test/audio-test.nro (tone test) into out/ (see scripts/build-homebrew.sh).
exec "$(dirname "$0")/build-homebrew.sh" audio-test
