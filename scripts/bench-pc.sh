#!/usr/bin/env bash
# Runs test.nro in the Linux eden-cli (PC baseline) inside the container on the build box and
# times each test from outside (the guest's own clock follows emulated cycles in single-core mode).
#   scripts/remote.sh scripts/bench-pc.sh [single|multi|unsafe|nofastmem]
set -euo pipefail
MODE="${1:-single}"
EDEN=/build/build/eden-linux/bin/eden-cli
NRO=/work/tools/test-nro/test.nro
export HOME=/build/pc-bench-home XDG_DATA_HOME=/build/pc-bench-home/data XDG_CONFIG_HOME=/build/pc-bench-home/config
export EDEN_NULL_RENDERER=1
CFG="$XDG_CONFIG_HOME/eden/sdl2-config.ini"
RESULT="$XDG_DATA_HOME/eden/sdmc/cpu-benchmark.txt"
mkdir -p "$HOME" "$XDG_CONFIG_HOME/eden"

# First run writes the default config; then pin the core mode (single = like the PS4 build).
[ -f "$CFG" ] || timeout 20 "$EDEN" "$NRO" >/dev/null 2>&1 || true
multi=false
[ "$MODE" = multi ] && multi=true
accuracy=0
[ "$MODE" = unsafe ] && accuracy=2   # CpuAccuracy: Auto, Accurate, Unsafe, ...
# nofastmem: Unsafe without the host MMU - every guest access through the page table, like the
# PS4's slow path.
host_mmu=true
if [ "$MODE" = nofastmem ]; then accuracy=2; host_mmu=false; multi=true; fi
sed -i -E "s/^cpuopt_unsafe_host_mmu\\\\default=.*/cpuopt_unsafe_host_mmu\\\\default=false/; s/^cpuopt_unsafe_host_mmu=.*/cpuopt_unsafe_host_mmu=$host_mmu/" "$CFG"
sed -i -E "s/^use_multi_core\\\\default=.*/use_multi_core\\\\default=false/; s/^use_multi_core=.*/use_multi_core=$multi/" "$CFG"
sed -i -E "s/^cpu_accuracy\\\\default=.*/cpu_accuracy\\\\default=false/; s/^cpu_accuracy=.*/cpu_accuracy=$accuracy/" "$CFG"
echo "mode: $MODE ($(grep -E '^(use_multi_core|cpu_accuracy|cpuopt_unsafe_host_mmu)=' "$CFG" | tr '\n' ' '))"

rm -f "$RESULT"
"$EDEN" "$NRO" > /build/pc-bench.log 2>&1 &
pid=$!
# Timestamp each new output line; stop eden-cli once the guest wrote "done".
python3 - "$RESULT" <<'PY'
import os, sys, time
path, seen, begin, start = sys.argv[1], 0, None, time.time()
while time.time() - start < 600:
    lines = open(path).read().split("\n")[:-1] if os.path.exists(path) else []
    for line in lines[seen:]:
        now = time.time() - start
        if line.startswith("begin "):
            begin = (line[6:], now)
            print(f"[{now:7.2f}] {line}")
        elif line.startswith("end ") and begin and line[4:].startswith(begin[0]):
            print(f"[{now:7.2f}] {line}  -> {now - begin[1]:.2f} s real")
            begin = None
        else:
            print(f"[{now:7.2f}] {line}")
    seen = len(lines)
    if any(l.startswith("done") for l in lines):
        break
    time.sleep(0.05)
PY
kill "$pid" 2>/dev/null || true
wait "$pid" 2>/dev/null || true
