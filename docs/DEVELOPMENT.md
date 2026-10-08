# Development notes

## Debugging a crash

After a crash, `/data/switch-emu/logs/crash.log` on the PS4 lists the signal, the registers and a backtrace of
code addresses. These are addresses in the emulator's ELF. `scripts/package-eden.sh` keeps each
packaged version's ELF in the build volume as `/build/oelf/eden-cli-v<version>.elf`, so the
addresses can be turned into function names in the container:

```sh
docker/run.sh llvm-symbolizer-21 --demangle --obj=/build/oelf/eden-cli-v01.00.elf 0x1234560 0x1234abc
```

Use the version of the package that crashed. `/data/switch-emu/logs/boot-trace.log` shows how far startup
got: one line per stage, the memory setup, and which executable-memory method the jailbreak
allowed.

## Profiling

Turn on **Settings → Debug → Profiler**, then play for a minute. A helper thread samples every
emulator thread 1000 times a second and appends a report to `/data/switch-emu/logs/samples.log` every 10
seconds:

- per thread: CPU use, and how the time splits between recompiled game code (`jit`), the emulator
  (`eden`) and PS4 system libraries (`sys`)
- the busiest addresses in each

Copy `samples.log` into `out/` and summarize it by function:

```sh
docker/run.sh python3 tools/summarize-samples.py out/samples.log /build/oelf/eden-cli-v01.00.elf
```

While a game runs, `boot-trace.log` gets a `perf:` line every 5 seconds (frame rate, emulation
speed and share of time waiting for the GPU) and a `frame:` line for every unusually long frame,
with the fastmem faults and new shaders since the previous one.

## Settings files

The launcher stores settings as files in `/data/switch-emu/`, which the emulator reads when it starts:

| File | Meaning |
|---|---|
| `options.txt` | All-games settings, as the launcher shows them |
| `titles/<game file>.opts` | One game's own settings |
| `null-renderer` | Graphics off: the status screen instead of the game's video |
| `handheld` | Console mode Handheld |
| `res-075`, `res-050` | Resolution 0.75x or 0.5x |
| `singlecore`, `no-fastmem`, `cpu-unsafe`, `async-shaders` | The matching Performance settings |
| `spread-threads` | Spread game threads |
| `gpu-high` | GPU accuracy High |
| `cross-is-a` | Button layout Cross = A |
| `profile` | Profiler |

Settings like multi-core and fastmem only take effect in a new process. **Run all benchmarks**
therefore restarts the app between its configurations and adds a line per run to
`/data/switch-emu/logs/bench-results.txt`.

## Folders on the console

The first start creates `/data/switch-emu/` and its folders (`games`, `keys`, `install`, `logs`,
`nand/system/Contents/registered`, `save-backups`, `titles`, `sdmc`) plus a `README.txt` guide;
see `createFolders()` in `eden/externals/ps4sup/crashlog.cpp`. Everything written for debugging
goes to `logs/`, including the GPU driver's diagnostics (`scripts/build-mesa.sh` points the driver
there).

## The logo

`assets/logo.svg` is the logo; `assets/icon.svg` is the same on a square background and becomes
the PS4 icon when packaging. The launcher's header shows a 64x64 copy built into the app: after
changing the logo, render `assets/icon.svg` to a 512x512 PNG and run
`tools/gen_logo_header.py <png>` to regenerate `eden/externals/ps4sup/status_logo.h`.

## Working on the launcher without a PS4

The launcher (`eden/externals/ps4sup/status_screen.cpp` for drawing, `ps4_menu.cpp` for logic)
builds and runs on a desktop computer too:

```sh
tools/ui-preview/run.sh     # every screen as PNGs in tools/ui-preview/out/
tools/menu-test/run.sh      # scripted button presses against the menu logic
```

Both need `clang++` (C++20). The preview converts images with `sips` on macOS or ImageMagick
(`magick`) elsewhere.

## Test programs

`scripts/build-*-test.sh` build small Switch homebrew programs into `out/` with devkitPro's
container. Copy one to `/data/switch-emu/games/` and start it from the launcher:

| Program | Checks |
|---|---|
| `gfx-test.nro` | Drawing to the screen (CPU framebuffer) |
| `gpu-test.nro` | 3D on the GPU (deko3d examples, one after another) |
| `pad-test.nro` | Controller input |
| `audio-test.nro` | Audio output |
| `test.nro` | CPU and memory speed, without graphics; also run by **Run all benchmarks** |

`scripts/build-eden-linux.sh` and `scripts/bench-pc.sh` build a desktop Linux version and run
`test.nro` on it, as a speed reference.
