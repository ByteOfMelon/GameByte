# GameByte
A WIP cross-platform research Game Boy emulator made with C++ and SDL3.

> [!WARNING]
> This emulator is very broken and should not be used to accurately play any Game Boy games.
> See other emulators for that sort of thing.
> This is a research emulator to experiment with low-level programming and emulation.

# Cloning
GameByte uses Git submodules for downloading the latest source for SDL3. To properly clone the repository, please run:
```
git clone --recursive https://github.com/ByteOfMelon/GameByte.git
```

If you've already cloned the repo without the submodules, you can run:
```
git submodule update --init --recursive
```

# Building
GameByte uses a standard CMake file to do its builds. Currently, builds are officially supported for Windows and macOS, with Linux testing possibly coming at a later point.

You can build with a regular CMake build command, or utilize the VS Code launch tasks to quickly build and run the project in debug mode.

# Running
You will need to provide your own Game Boy ROM file from a legal source. GameByte will open a file selection dialog and ask you to provide a .gb ROM file. Note that slowdowns and inaccuracies are to be expected.

# Compatibility
GameByte is a research emulator that is not intended to be fully accurate or usable with a lot of Game Boy games. However, a compatibility list showing tested games are listed below. Note that the emulator does not (yet) support sound of any kind or any MBC emulation.

## States
- **Playable** - Games that can be completed with playable performance and no game breaking glitches
- **Ingame** - Games that can get in-game but cannot be finished, have serious glitches or insufficient performance
- **Menus** - Games that initially run but cannot go ingame
- **Nothing** - Games that do not initialize properly and/or do not load

## Compatibility List (tested games only)

| **Game Title**                 | **Publisher** | **Year** | **State**                |
|--------------------------------|---------------|----------|--------------------------|
| Tetris                         | Nintendo      | 1989     | Playable                 |
| Dr. Mario                      | Nintendo      | 1990     | Playable                 |
| Space Invaders (Japan)         | Taito         | 1990     | Playable                 |
| Asteroids                      | Accolade      | 1992     | Playable                 |
| Minesweeper: Soukaitei         | Pack-in-Video | 1991     | Playable                 |
| (Pretty much) Every other game | Anyone        | Any time | Nothing/possibly menus   |