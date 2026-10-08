#!/usr/bin/env bash
# Runs a command in the build container. The project folder is mounted at /work; build trees,
# the PS4 sysroot and downloaded sources live in the "edenps4-build" volume at /build (a volume is
# much faster than a Docker Desktop bind mount for large builds).
#
# Works with Docker or rootless Podman. With a build box configured (scripts/build-host.sh), it runs
# there instead: scripts/remote.sh syncs the project over and runs this script on the box.
#
#   docker/run.sh                       interactive shell
#   docker/run.sh scripts/setup.sh      run a script
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$HERE")"
IMAGE=edenps4-build

# EDENPS4_LOCAL=1 builds here even with a build box configured (remote.sh sets it on the box).
if [ "${EDENPS4_LOCAL:-0}" != 1 ] && { [ -n "${EDENPS4_HOST:-}" ] || [ -f "$ROOT/build-host.env" ]; }; then
    exec "$ROOT/scripts/remote.sh" "$@"
fi

if command -v docker >/dev/null 2>&1; then
    engine=docker
    label=""
else
    engine=podman
    label=":z"   # SELinux (Fedora and its spins): let the container read the bind mount
fi

if ! "$engine" image inspect "$IMAGE" >/dev/null 2>&1; then
    "$engine" build --platform linux/amd64 -t "$IMAGE" "$HERE"
fi

tty=()
[ -t 0 ] && tty=(-it)
# EDENPS4_RUN_ARGS: extra container options (scripts/profile-pc.sh needs perf_event_open).
read -r -a extra <<< "${EDENPS4_RUN_ARGS:-}"
exec "$engine" run --rm ${tty[@]+"${tty[@]}"} ${extra[@]+"${extra[@]}"} --platform linux/amd64 \
    -v "$ROOT:/work$label" -v edenps4-build:/build \
    -e JOBS="${JOBS:-8}" \
    "$IMAGE" "${@:-bash}"
