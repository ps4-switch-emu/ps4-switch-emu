#!/usr/bin/env python3
"""Turns the PS4 profiler's samples.log into per-thread function totals.

    tools/summarize-samples.py samples.log <eden-cli ELF> [llvm-symbolizer]

samples.log (externals/ps4sup/sampler.cpp) lists, per thread and 10 s window, the share of
samples per 16-byte bucket: "elf" (Eden, ELF addresses), "jit" (recompiled guest code) and
"sys" (PS4 system libraries). This adds the windows up, symbolizes the elf buckets and prints
each thread's busiest functions. Run in the build container (docker/run.sh), which has
llvm-symbolizer-21 and keeps each packaged version's ELF as /build/oelf/eden-cli-v<version>.elf.
"""
import collections
import re
import subprocess
import sys

log, elf = sys.argv[1], sys.argv[2]
symbolizer = sys.argv[3] if len(sys.argv) > 3 else "llvm-symbolizer-21"

threads = collections.defaultdict(lambda: collections.Counter())  # name -> key -> samples
kinds = collections.defaultdict(lambda: collections.Counter())    # name -> kind -> samples
totals = collections.Counter()
busy = collections.defaultdict(list)                              # name -> CPU % per window
current = None
window_samples = 0
for line in open(log, errors="replace"):
    m = re.match(r"== ([\d.]+) s, (.+?): (\d+) samples(?:, CPU (\d+)% busy)?", line)
    if m:
        current, window_samples = m.group(2), int(m.group(3))
        totals[current] += window_samples
        if m.group(4) is not None:
            busy[current].append(int(m.group(4)))
        continue
    m = re.match(r"\s+([\d.]+)% (elf|jit|sys) (0x[0-9a-f]+)", line)
    if m and current:
        count = float(m.group(1)) / 100.0 * window_samples
        threads[current][(m.group(2), int(m.group(3), 16))] += count
        kinds[current][m.group(2)] += count

addresses = sorted({addr for t in threads.values() for (kind, addr) in t if kind == "elf"})
names = {}
if addresses:
    out = subprocess.run([symbolizer, "--obj=" + elf, "--demangle"],
                         input="\n".join(hex(a) for a in addresses), capture_output=True, text=True).stdout
    blocks = [b.strip().split("\n") for b in out.strip().split("\n\n")]
    for addr, block in zip(addresses, blocks):
        names[addr] = block[0] if block and block[0] != "??" else hex(addr)

for name, total in totals.most_common():
    if total < 100:
        continue
    k = kinds[name]
    listed = sum(k.values()) or 1
    cpu = f"  CPU busy {sum(busy[name]) / len(busy[name]):.0f}% (max {max(busy[name])}%)" if busy[name] else ""
    print(f"\n=== {name}: {total} samples{cpu}  "
          f"jit {100 * k['jit'] / listed:.0f}%  eden {100 * k['elf'] / listed:.0f}%  sys {100 * k['sys'] / listed:.0f}%")
    functions = collections.Counter()
    for (kind, addr), count in threads[name].items():
        if kind == "elf":
            functions[names.get(addr, hex(addr))] += count
        elif kind == "jit":
            functions["[jit] recompiled guest code"] += count
        else:
            functions[f"[sys] 0x{addr & ~0xFFFFF:x} (system module)"] += count
    for fn, count in functions.most_common(25):
        print(f"  {100 * count / total:5.1f}%  {fn}")
