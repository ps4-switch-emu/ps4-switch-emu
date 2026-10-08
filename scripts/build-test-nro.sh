#!/usr/bin/env bash
# Builds tools/test-nro/test.nro (a no-graphics Switch homebrew benchmark) into out/ (see scripts/build-homebrew.sh).
exec "$(dirname "$0")/build-homebrew.sh" test-nro test
