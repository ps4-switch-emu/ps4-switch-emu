#!/usr/bin/env bash
# Builds tools/pad-test/pad-test.nro (controller test) into out/ (see scripts/build-homebrew.sh).
exec "$(dirname "$0")/build-homebrew.sh" pad-test
