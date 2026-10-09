#!/usr/bin/env bash
# Profiles a game in the Linux eden-cli (scripts/build-eden-linux.sh) with the PS4's settings -
# multi-core, fastmem, the PS4's page-table layout (EDEN_FORCE_PT_SIGN_EXTENSION), no graphics -
# to find slow paths in the emulator's own code without the console. Inside the container, which
# needs perf_event_open:
#   EDENPS4_RUN_ARGS="--security-opt seccomp=unconfined" docker/run.sh scripts/profile-pc.sh \
#       /build/pc-data/Game.nsp [boot seconds] [profile seconds] [button script] [gfx]
# A button script (tools/autopress/*.txt, see StartAutopress in yuzu.cpp) plays the game without a
# controller, so the profile can cover gameplay rather than the title screen. "gfx" renders with
# Mesa's software Vulkan (llvmpipe, off-screen) instead of no graphics - some games wait for the
# GPU (e.g. cutscenes) - and saves a screenshot every 5 s in /build/pc-shots; llvmpipe's own
# threads show up separately in the profile.
# Keys and firmware go in /build/pc-bench-home/data/eden (keys/, nand/system/Contents/registered/).
# Prints the busiest threads and functions; the raw profile stays in /build/pc-profile.data.
set -euo pipefail
GAME="${1:?usage: profile-pc.sh <game> [boot seconds] [profile seconds]}"
BOOT="${2:-25}"
SECONDS_PROFILED="${3:-30}"
if [ -n "${4:-}" ]; then
    export EDEN_AUTOPRESS="$4"
fi
GFX="${5:-}"
# JITDUMP=1: save the compiled blocks too and show the hottest ones disassembled (tools/jit-hot-blocks.py).
if [ "${JITDUMP:-}" = 1 ]; then
    export PERF_BUILDID_DIR=/tmp EDEN_JIT_DUMP=/build/pc-jit.dump
fi
EDEN=/build/build/eden-linux/bin/eden-cli
export HOME=/build/pc-bench-home XDG_DATA_HOME=/build/pc-bench-home/data XDG_CONFIG_HOME=/build/pc-bench-home/config
export EDEN_FORCE_PT_SIGN_EXTENSION=1
# The JIT emits code for the PS4's Jaguar cores (no AVX2/FMA/BMI2) unless EDEN_HOST_FEATURES= is set empty.
export EDEN_HOST_FEATURES="${EDEN_HOST_FEATURES-jaguar}"
export SDL_AUDIO_DRIVER=dummy
if [ "$GFX" = gfx ]; then
    export SDL_VIDEO_DRIVER=offscreen EDEN_SCREENSHOTS=/build/pc-shots
    rm -rf /build/pc-shots && mkdir -p /build/pc-shots
else
    export EDEN_NULL_RENDERER=1
fi
CFG="$XDG_CONFIG_HOME/eden/sdl2-config.ini"

PERF="$(ls /usr/lib/linux-tools-*/perf 2>/dev/null | head -1 || true)"
if [ -z "$PERF" ]; then # the container is thrown away afterwards: install perf each time
    apt-get update -qq >/dev/null && apt-get install -y -qq linux-tools-generic >/dev/null
    PERF="$(ls /usr/lib/linux-tools-*/perf | head -1)"
fi
if [ "$GFX" = gfx ] && [ ! -f /usr/share/vulkan/icd.d/lvp_icd.json ]; then
    apt-get install -y -qq mesa-vulkan-drivers >/dev/null
fi

# The PS4 build's settings; warnings only (logging every service call would skew the profile).
mkdir -p "$XDG_CONFIG_HOME/eden"
[ -f "$CFG" ] || { timeout -s KILL 20 "$EDEN" >/dev/null 2>&1 || true; }
set_cfg() { sed -i -E "s/^$1\\\\default=.*/$1\\\\default=false/; s/^$1=.*/$1=$2/" "$CFG"; }
set_cfg use_multi_core true
set_cfg cpu_accuracy "${CPU_ACCURACY:-0}" # 0 Auto (the default), 1 Accurate, 2 Unsafe
set_cfg cpuopt_unsafe_host_mmu true
set_cfg log_filter '"*:Warning"'
if [ "$GFX" = gfx ]; then
    set_cfg backend 1       # Vulkan
    set_cfg vulkan_device 0 # llvmpipe: the only Vulkan device in the container
else
    set_cfg backend 2       # no graphics
fi
set_cfg output_engine 2 # SDL with its dummy driver (no audio device in the container): consumes audio in real time like a real device

# perf starts eden-cli itself (attaching to a running process needs privileges the build box's
# kernel settings don't give), skips the boot, and stops it after the profiled stretch.
echo "booting for ${BOOT} s, then profiling ${SECONDS_PROFILED} s"
"$PERF" record -q -F 999 -e cpu-clock:u -D $((BOOT * 1000)) -o /build/pc-profile.data -- \
    timeout -s KILL $((BOOT + SECONDS_PROFILED)) "$EDEN" "$GAME" > /build/pc-profile.log 2>&1 || true

echo "== timeline: CPU use per emulator thread, 5 s steps (100 = one core busy; llvmpipe left out)"
"$PERF" script -i /build/pc-profile.data -F comm,time 2>/dev/null | python3 -c '
import sys, collections
counts = collections.defaultdict(collections.Counter)
first = None
for line in sys.stdin:
    parts = line.split()
    if len(parts) < 2: continue
    t = float(parts[-1].rstrip(":"))
    first = t if first is None else first
    name = " ".join(parts[:-1])
    if name.startswith("llvmpipe"): continue  # the software renderer, not the emulator
    counts[int((t - first) // 5)][name] += 1
threads = collections.Counter()
for c in counts.values(): threads.update(c)
top = [t for t, _ in threads.most_common(6)]
print("   s  " + "".join(f"{t[:13]:>14}" for t in top))
for step in sorted(counts):
    print(f"{step * 5:4d}  " + "".join(f"{counts[step][t] / 5 / 999 * 100:14.0f}" for t in top))
'
echo "== threads (share of samples)"
"$PERF" report -i /build/pc-profile.data --stdio --sort comm 2>/dev/null | grep -E '^ +[0-9]' | head -15
echo "== where each busy thread spends its time (JIT code shows as [unknown] / anon)"
"$PERF" report -i /build/pc-profile.data --stdio --sort comm,dso,sym --percent-limit 0.4 2>/dev/null \
    | grep -E '^ +[0-9]' | head -60
if [ "${JITDUMP:-}" = 1 ]; then
    echo "== hottest JIT blocks"
    "$PERF" script -i /build/pc-profile.data -F comm,ip 2>/dev/null \
        | python3 "$(dirname "$0")/../tools/jit-hot-blocks.py" /build/pc-jit.dump "${JITBLOCKS:-6}"
fi
