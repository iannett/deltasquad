# Deltasquad

![decompiled](https://img.shields.io/badge/decompiled-7.94%25-blue)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)
![CMake](https://img.shields.io/badge/build-CMake-064F8C?logo=cmake)
![Clang](https://img.shields.io/badge/compiler-Clang-262D3A?logo=llvm)
![Target](https://img.shields.io/badge/target-Xbox-107C10)
![Host](https://img.shields.io/badge/builds%20on-macOS-000000?logo=apple)

Deltasquad is a work-in-progress decompilation of *Star Wars: Republic Commando* for the original Xbox (USA release). The project reconstructs the game's C++ source from `Default.xbe`.

This decompilation is non-matching. 

> [!IMPORTANT]
> This repository contains no game files. You need your own copy of the game to build and run the project.

## Status

<!-- progress -->
**7.94%** decompiled (213,871 / 2,694,583 bytes, 1,309 / 12,356 functions)
<!-- /progress -->

Progress is measured against the game's own code in `Default.xbe` and weighted by function size. The C runtime, the Xbox system libraries and Xbox-specific engine code (Direct3D renderer, audio, Xbox Live, memory allocator and similar) are excluded. The excluded code is listed in `progress/platform-roots.txt` and expanded by `tools/platform.sh`.

A function counts as decompiled once it is implemented in `src/`, compiles, and is listed in `progress/done.txt`. To update the figure above, run:

```
python3 tools/progress.py --readme
```

## Target

| | |
|---|---|
| Game | Star Wars: Republic Commando (USA), original Xbox |
| Executable | `Default.xbe` (built 2005-01-26, original name `XBoxLaunch.exe`) |
| Title ID | `0x4C410013` |
| SDK | XDK 5849 (Visual C++ 7.1) |
| Engine | Unreal Engine 2 (modified), package version 159 |

## Building

### Requirements

- macOS with the Xcode Command Line Tools
- CMake, Ninja and SDL2:

  ```
  brew install cmake ninja sdl2
  ```

- An extracted copy of the game

### 1. Extract the game

Unpack your disc image with [xdvdfs](https://github.com/antangelo/xdvdfs):

```
xdvdfs unpack "<path to iso>" <extract dir>
```

### 2. Link the game files

Create a link named `game` in the repository root that points to the extracted directory:

```
ln -s <extract dir> game
```

`game/` is ignored by git. Game files must never be committed.

### 3. Build

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### 4. Run

```
./build/src/Launch/deltasquad BASEDIR=$PWD/game/System/
```

`BASEDIR` must point to the game's `System` directory. The engine reads `System.ini` and `User.ini` from there.

### Tests

```
./build/src/Tests/coretests
```

### Troubleshooting

If linking fails with `unknown architecture arm64e.x1`, the installed macOS SDK is newer than the linker. Pass a matching SDK to CMake, for example:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
```

## Code requirements

The code must not depend on pointer size. Script bytecode stores object and name references as 4-byte indices.

## Repository layout

| Path | Contents |
|---|---|
| `src/` | Decompiled C++, one directory per Unreal module |
| `docs/` | Findings on the engine, classes and file formats |
| `tools/` | Extraction, analysis and code generation scripts |
| `symbols/` | Function, class and native names recovered from the executable |
| `progress/` | Function list exported from Ghidra and the list of finished functions |
| `scripts/` | Decompiled UnrealScript, generated locally and ignored by git |
| `game/` | Link to your extracted game files, ignored by git |

Analysis is done in Ghidra. The Ghidra project is kept outside the repository.
