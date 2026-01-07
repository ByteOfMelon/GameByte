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