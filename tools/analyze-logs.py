#!/usr/bin/env python3
"""One-page summary of a logs/ folder copied from the PS4 (/data/switch-emu/logs).

    tools/analyze-logs.py <logs folder>

Reads boot-trace.log and boot-trace.prev.log (whichever has the game run - the longer one),
samples.log (profiler) and crash.log, and prints: the system facts the boot trace records (CPU
clock, Pro mode, CPU features, clock source, timer precision, fastmem placement), the frame rate
over the run, long frames, thread priorities and pinning, fastmem coverage, JIT code cache
clears, the busiest threads (profiler) and the last crash.
"""
import os
import re
import statistics
import sys


def read(path):
    try:
        with open(path, errors="replace") as f:
            return f.read().splitlines()
    except OSError:
        return []


def message(line):
    # "[  12.345] [int ..., flex ..., dmem ...] text" -> (seconds, text)
    m = re.match(r"\[\s*([0-9.]+)\]\s*(?:\[[^\]]*\]\s*)?(.*)", line)
    return (float(m.group(1)), m.group(2)) if m else (None, line)


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else "."
    traces = [(name, read(os.path.join(folder, name))) for name in ("boot-trace.log", "boot-trace.prev.log")]
    traces = [t for t in traces if t[1]]
    if not traces:
        print("no boot-trace.log here")
        return
    # The run with a game in it: the one with the most perf lines (else the longest).
    name, lines = max(traces, key=lambda t: (sum("perf:" in l for l in t[1]), len(t[1])))
    print(f"== {name} ({len(lines)} lines)")

    facts = ("main: cpu ", "main: cpu features", "main: clock", "main: 1 ms timed waits", "fastmem: reserved",
             "fastmem: could not", "main: renderer backend", "library:", "shader cache:")
    for line in lines:
        _, text = message(line)
        if any(text.startswith(f) for f in facts):
            print("  " + text)

    perf = []
    for line in lines:
        t, text = message(line)
        m = re.search(r"perf: game ([0-9.]+) fps.*?([0-9.]+|-nan) ms per frame, waiting for the GPU ([0-9.]+)%"
                      r"(?: of the time, (\d+) fastmem faults)?(?:, GPU memory (\d+) MiB)?", text)
        if m and float(m.group(1)) > 1:
            dmem = re.search(r"dmem (\d+) MiB", line)
            perf.append((t, float(m.group(1)), float(m.group(3)), int(m.group(4) or 0), int(m.group(5) or 0),
                         int(dmem.group(1)) if dmem else 0))
    if perf:
        fps = [p[1] for p in perf]
        print(f"\n== frame rate over {len(perf)} 5 s windows: median {statistics.median(fps):.1f}, "
              f"min {min(fps):.1f}, max {max(fps):.1f}; GPU wait median {statistics.median(p[2] for p in perf):.0f}%; "
              f"fastmem faults {sum(p[3] for p in perf)}")
        print(f"  memory: GPU up to {max(p[4] for p in perf)} MiB, direct memory free as low as {min(p[5] for p in perf)} MiB")
        buckets = {}
        for p in perf:
            buckets.setdefault(int(p[1] // 5) * 5, 0)
            buckets[int(p[1] // 5) * 5] += 1
        for b in sorted(buckets):
            print(f"  {b:3d}-{b + 4:<3d} fps  {'#' * buckets[b]}")

    gaps = []
    for line in lines:
        t, text = message(line)
        m = re.search(r"frame: (\d+) ms gap.*?(\d+) fastmem faults, (\d+) new pipelines", text)
        if m:
            gaps.append((t, int(m.group(1)), int(m.group(2)), int(m.group(3))))
    if gaps:
        print(f"\n== long frames: {len(gaps)} (worst {max(g[1] for g in gaps)} ms)")
        for t, ms, faults, pipes in sorted(gaps, key=lambda g: -g[1])[:8]:
            print(f"  {t:8.1f} s  {ms:5d} ms  ({faults} fastmem faults, {pipes} new pipelines)")

    builds = []
    for line in lines:
        t, text = message(line)
        m = re.search(r"shader: new pipeline #(\d+) built in (\d+) ms", text)
        if m:
            builds.append((t, int(m.group(2))))
    if builds:
        print(f"\n== slow shader builds (>= 30 ms): {len(builds)}, {sum(b[1] for b in builds) / 1000:.1f} s in total, "
              f"worst {max(b[1] for b in builds)} ms")
        for t, ms in sorted(builds, key=lambda b: -b[1])[:5]:
            print(f"  {t:8.1f} s  {ms:5d} ms")

    emulator = read(os.path.join(folder, "emulator.log"))
    skipped = [l for l in emulator if "PS4: skipping draws" in l]
    if skipped:
        print(f"\n== GPU: pipelines skipped (geometry/tessellation shaders the PS4 can't run): "
              f"{re.search(r'(\d+) pipelines skipped', skipped[-1]).group(1) if re.search(r'(\d+) pipelines skipped', skipped[-1]) else len(skipped)}")
    oom = [l for l in emulator if re.search(r"OUT_OF_(DEVICE|HOST)_MEMORY|Out of memory|failed to allocate", l, re.I)]
    if oom:
        print("\n== out of memory messages (emulator.log)")
        for l in oom[:5]:
            print("  " + l.strip()[:200])

    for title, key in (("thread priorities", ": priority "), ("pinned threads", "pinned to core"),
                       ("lazy DRAM mapping", "lazy mapping"), ("shader builders", "pipeline builder threads"),
                       ("fastmem coverage", "fastmem ("), ("JIT code cache clears", "jit: core")):
        found = [message(l)[1] for l in lines if key in l]
        if found:
            print(f"\n== {title}")
            for text in found[-12:]:
                print("  " + text)

    samples = read(os.path.join(folder, "samples.log"))
    if samples:
        busy = {}
        for line in samples:
            m = re.match(r"== (\d+) s, (.+?): \d+ samples, CPU (\d+)% busy", line)
            if m:
                busy.setdefault(m.group(2), []).append(int(m.group(3)))
        if busy:
            print("\n== busiest threads (profiler: median CPU % of a core)")
            for thread, values in sorted(busy.items(), key=lambda kv: -statistics.median(kv[1]))[:8]:
                print(f"  {thread:20s} {statistics.median(values):4.0f}%  (max {max(values)}%)")

    crash = read(os.path.join(folder, "crash.log"))
    if crash:
        last = max(i for i, l in enumerate(crash) if l.startswith("==="))
        print("\n== last crash (crash.log)")
        for line in crash[last:last + 4]:
            print("  " + line)


if __name__ == "__main__":
    main()
