# Sourced by the build scripts. The build box's ssh name comes from EDENPS4_HOST or from
# build-host.env in the project folder (not in git), for example:
#   EDENPS4_HOST=buildbox
#   EDENPS4_FALLBACK_IP=100.64.0.10   # optional: tried when the ssh name doesn't answer (e.g. a VPN address)
_build_host_env="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/build-host.env"
if [ -f "$_build_host_env" ]; then
    . "$_build_host_env"
fi
HOST="${EDENPS4_HOST:?set EDENPS4_HOST or create build-host.env (see scripts/build-host.sh)}"
# When the ssh name doesn't answer, reach the same host at EDENPS4_FALLBACK_IP (same user and key).
# ssh/scp take "${SSH_OPTS[@]}"; rsync takes -e "$RSYNC_SSH".
SSH_OPTS=()
if [ -n "${EDENPS4_FALLBACK_IP:-}" ] && ! ssh -o ConnectTimeout=4 -o BatchMode=yes "$HOST" true 2>/dev/null; then
    SSH_OPTS=(-o "HostName=$EDENPS4_FALLBACK_IP")
    echo "build host: $HOST unreachable, using $EDENPS4_FALLBACK_IP" >&2
fi
RSYNC_SSH="ssh ${SSH_OPTS[*]+${SSH_OPTS[*]}}"
