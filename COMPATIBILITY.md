# Game compatibility

How Switch games run on PS4 Switch Emu. The list only includes games that have been tested on a
PS4; a game that isn't listed hasn't been tested yet, it isn't necessarily broken.

## Status

| Status | Meaning |
|---|---|
| **Playable** | Can be played from start to finish with no game-breaking problems. It may have minor graphics or audio glitches, or slowdown. |
| **Ingame** | Gets into gameplay but has problems that stop it from being finished: crashes, freezes, missing graphics, or speeds too low to play. |
| **Menus** | Reaches the title screen or menus but not gameplay. |
| **Boots** | Shows something (a logo or the first screen) and then stops. |
| **Nothing** | Doesn't start, or crashes straight away. |

## Games

| Game | Title ID | Status | Notes |
|---|---|---|---|
| PICO PARK | 01004EA00DF70000 | **Playable** | Full 60 fps. Has frozen once during play; being looked into (an emulator problem, not specific to the game). |
| Super Meat Boy | 01004F8006A78000 | **Playable** | About 30-60 fps on a PS4 Pro. |

## Testing setup

Tested on a PS4 Pro running GoldHEN. The emulator runs it as a base PS4 (the Pro's own enhanced
mode isn't supported yet), so a base PS4 or PS4 Slim should get similar results, though their CPU
is slower without the Pro's Boost Mode.

## Adding a game

Tested a game? Open an issue with:

- the game's name and title ID (16 hexadecimal digits, like `01004F8006A78000`),
- the game's version and whether an update or DLC is installed,
- your PS4 model and firmware, and the emulator version (Settings > About),
- which status above fits, and what happened: how far you got, the frame rate (the performance
  overlay shows it), and any glitches, freezes or crashes.

If the game crashed or froze, attach the files in `/data/switch-emu/logs` from that run. Please
don't attach or link game files, keys or firmware.
