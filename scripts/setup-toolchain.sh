#!/usr/bin/env bash
# Builds the PS4 sysroot into /build/sysroot (run inside the container: docker/run.sh scripts/setup-toolchain.sh):
#   - libc-overlay headers + libc++/libc++abi 21 for the PS4 target
#   - DolphinPS4's runtime archive (malloc replacement)
# Reuses DolphinPS4's toolchain scripts unchanged; they read PSB/PS4_SYSROOT/OPENORBIS from the
# environment set by the Dockerfile.
set -euo pipefail
DPS4=/work/vendor/DolphinPS4

if [ -f "$PS4_SYSROOT/lib/libc++.a" ] && [ "${1:-}" != force ]; then
    echo "libc++ already built in $PS4_SYSROOT (pass 'force' to rebuild)"
else
    "$DPS4/toolchain/build-libcxx.sh"
fi
"$DPS4/toolchain/build-runtime.sh"
ls -la "$PS4_SYSROOT/lib"
