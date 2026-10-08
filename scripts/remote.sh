#!/usr/bin/env bash
# Runs a build step on the Linux build box (see scripts/build-host.sh) instead of this computer.
# Syncs the project there first (excluding out/), runs docker/run.sh with the given command, and
# copies out/ back.
#
#   scripts/remote.sh scripts/build-eden.sh          (from the project folder)
#   JOBS=12 scripts/remote.sh scripts/build-eden.sh build
set -euo pipefail
. "$(dirname "$0")/build-host.sh"
DEST="${EDENPS4_DEST:-eden-ps4}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

rsync -a -e "$RSYNC_SSH" --delete --exclude /out/ --exclude /eden/.cache/ --exclude '/tools/test-nro/test.*' --exclude /tools/test-nro/build/ --exclude '/tools/gfx-test/gfx-test.*' --exclude /tools/gfx-test/build/ --exclude .DS_Store --exclude /build-host.env "$ROOT/" "$HOST:$DEST/"
ssh ${SSH_OPTS[@]+"${SSH_OPTS[@]}"} "$HOST" "cd ~/$DEST && EDENPS4_LOCAL=1 JOBS=${JOBS:-12} EDENPS4_RUN_ARGS=$(printf '%q' "${EDENPS4_RUN_ARGS:-}") docker/run.sh $(printf '%q ' "$@")"
mkdir -p "$ROOT/out"
rsync -a -e "$RSYNC_SSH" "$HOST:$DEST/out/" "$ROOT/out/" 2>/dev/null || true
