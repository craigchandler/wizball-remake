---
title: "Build & Install"
---

## Build on Linux

The current build is SDL2-based and uses `pkg-config` to discover its libraries.

### Dependencies

- C++ compiler with C++11 support
- CMake 3.16 or newer
- pkg-config
- SDL2
- SDL2_mixer
- SDL2_image

### Build

Default build:

```bash
cmake -S . -B build -DWIZBALL_PORTMASTER=OFF
cmake --build build -j
```

Release build:

```bash
cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release -DWIZBALL_PORTMASTER=OFF
cmake --build build_release -j
```

### Run

Run from the `wizball/` data directory:

```bash
cd wizball
../build/wizball
```

Useful flags:

- `-debug` enables extra logging

If you changed scripts or global parameter content, regenerate the compiled script output with:

```bash
cmake --build build --target rebuild_wizball_scripts
```

## PortMaster
(put your packaging + install notes here)
