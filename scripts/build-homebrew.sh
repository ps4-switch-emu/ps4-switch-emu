#!/usr/bin/env bash
# Builds a Switch homebrew test program in tools/<dir> with devkitPro's devkita64 image and copies
# the .nro to out/. Runs here with Docker or Podman, or on the build box when one is configured
# (scripts/build-host.sh; EDENPS4_LOCAL=1 builds here anyway).
#
#   scripts/build-homebrew.sh gfx-test            -> out/gfx-test.nro
#   scripts/build-homebrew.sh test-nro test       -> out/test.nro (the .nro's name, when it differs)
set -euo pipefail
DIR="$1"
NRO="${2:-$1}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE=docker.io/devkitpro/devkita64:latest
mkdir -p "$ROOT/out"

if [ "${EDENPS4_LOCAL:-0}" != 1 ] && { [ -n "${EDENPS4_HOST:-}" ] || [ -f "$ROOT/build-host.env" ]; }; then
    . "$ROOT/scripts/build-host.sh"
    DEST="${EDENPS4_DEST:-eden-ps4}"
    rsync -a -e "$RSYNC_SSH" --delete --exclude "$NRO.*" --exclude build/ --exclude romfs/shaders/ "$ROOT/tools/$DIR/" "$HOST:$DEST/tools/$DIR/"
    ssh ${SSH_OPTS[@]+"${SSH_OPTS[@]}"} "$HOST" "cd ~/$DEST/tools/$DIR && \$(command -v podman || command -v docker) run --rm -v \"\$PWD:/src:z\" -w /src $IMAGE make"
    scp -q ${SSH_OPTS[@]+"${SSH_OPTS[@]}"} "$HOST:$DEST/tools/$DIR/$NRO.nro" "$ROOT/out/$NRO.nro"
else
    if command -v docker >/dev/null 2>&1; then engine=docker; else engine=podman; fi
    "$engine" run --rm --platform linux/amd64 -v "$ROOT/tools/$DIR:/src:z" -w /src "$IMAGE" make
    cp "$ROOT/tools/$DIR/$NRO.nro" "$ROOT/out/$NRO.nro"
fi
ls -la "$ROOT/out/$NRO.nro"
