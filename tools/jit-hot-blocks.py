#!/usr/bin/env python3
"""The hottest JIT blocks of a PC profile, disassembled with samples per instruction.

    perf script -i profile.data -F comm,ip | tools/jit-hot-blocks.py <EDEN_JIT_DUMP file> [blocks to show]

The dump comes from running eden-cli with EDEN_JIT_DUMP=<file> (dynarmic's perf_map.cpp writes
every compiled block's machine code there); scripts/profile-pc.sh does both with JITDUMP=1.
Only the emulated CPU cores' samples count. Shows which guest blocks the time goes to and which
host instructions in them - e.g. fallbacks, flag handling or memory-access sequences that are
cheap on a modern PC but not on the PS4's Jaguar cores.
"""
import bisect
import collections
import struct
import subprocess
import sys
import tempfile


def load_dump(path):
    blocks = {}
    with open(path, "rb") as f:
        data = f.read()
    pos = 0
    while pos + 16 <= len(data):
        address, size, name_size = struct.unpack_from("<QII", data, pos)
        pos += 16
        name = data[pos:pos + name_size].decode(errors="replace")
        pos += name_size
        code = data[pos:pos + size]
        pos += size
        if len(code) < size:
            break
        blocks[address] = (size, name, code)  # a cleared cache reuses addresses: keep the latest
    return blocks


def main():
    blocks = load_dump(sys.argv[1])
    shown = int(sys.argv[2]) if len(sys.argv) > 2 else 6
    starts = sorted(blocks)
    per_block = collections.Counter()
    per_ip = collections.Counter()
    total = jit_total = 0
    for line in sys.stdin:
        parts = line.split()
        if len(parts) < 2 or not parts[0].startswith("CPUCore"):
            continue
        try:
            ip = int(parts[-1], 16)
        except ValueError:
            continue
        total += 1
        i = bisect.bisect_right(starts, ip) - 1
        if i >= 0 and ip < starts[i] + blocks[starts[i]][0]:
            per_block[starts[i]] += 1
            per_ip[ip] += 1
            jit_total += 1
    if not total:
        print("no CPUCore samples")
        return
    print(f"{len(blocks)} blocks compiled; CPU-core samples {total}, {jit_total * 100 / total:.0f}% in JIT blocks")
    print(f"top blocks (share of the emulated cores' time):")
    for start, n in per_block.most_common(25):
        size, name, _ = blocks[start]
        print(f"  {n * 100 / total:5.2f}%  {name:28s} {size:5d} bytes")
    for start, n in per_block.most_common(shown):
        size, name, code = blocks[start]
        print(f"\n== {name}: {n * 100 / total:.2f}%, {size} bytes")
        with tempfile.NamedTemporaryFile(suffix=".bin") as tmp:
            tmp.write(code)
            tmp.flush()
            out = subprocess.run(["objdump", "-D", "-b", "binary", "-m", "i386:x86-64", "-M", "intel",
                                  f"--adjust-vma={start:#x}", tmp.name], capture_output=True, text=True).stdout
        for line in out.splitlines():
            head = line.split(":", 1)[0].strip()
            try:
                address = int(head, 16)
            except ValueError:
                continue
            hits = per_ip.get(address, 0)
            mark = f"{hits:5d}" if hits else "     "
            print(f"{mark} {line.strip()}")


if __name__ == "__main__":
    main()
