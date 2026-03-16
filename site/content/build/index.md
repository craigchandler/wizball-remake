---
title: "Build & Install"
---

## Build on Linux

The build is SDL2-based with two renderer targets: `SDL` for the desktop and `GLES2` for PortMaster/handheld builds. `pkg-config` is used for dependency discovery.

### Dependencies

- C++ compiler with C++11 support
- CMake 3.16+
- pkg-config
- SDL2
- SDL2_mixer
- SDL2_image
- OpenGLESv2 / GLESv2 development library (GLES2 build only)

### Linux desktop build

Recommended development build:

```bash
cmake -S . -B build -DWIZBALL_PORTMASTER=OFF -DWIZBALL_RENDER_BACKEND=SDL
cmake --build build -j
```

Release build (recommended for playtesting):

```bash
cmake -S . -B build_release -DCMAKE_BUILD_TYPE=Release -DWIZBALL_PORTMASTER=OFF -DWIZBALL_RENDER_BACKEND=SDL
cmake --build build_release -j
```

AddressSanitizer build:

```bash
cmake -S . -B build_asan -DWIZBALL_PORTMASTER=OFF -DWIZBALL_ENABLE_ASAN=ON -DWIZBALL_RENDER_BACKEND=SDL
cmake --build build_asan -j
```

### Local GLES2 build

To test the GLES2 renderer locally on Linux:

```bash
cmake -S . -B build-gles2-local -DCMAKE_BUILD_TYPE=Release -DWIZBALL_PORTMASTER=OFF -DWIZBALL_RENDER_BACKEND=GLES2
cmake --build build-gles2-local -j
```

### Run

Run from the `wizball/` data directory:

```bash
cd wizball
../build/wizball
```

Useful flags:

- `-debug` — enables extra logging
- `-dat` — enables the compiled data mode used by the PortMaster launcher

### Rebuild generated data

Generated `.dat` files are rebuilt by running the binary directly with rebuild flags. There is no separate packfile tool.

Direct commands from the `wizball/` data directory:

```bash
cd wizball
../build/wizball -rebuildscripts wizball
../build/wizball -rebuildtilesets wizball
../build/wizball -rebuildtilemaps wizball
```

CMake also exposes thin wrapper targets for the same commands:

```bash
cmake --build build --target rebuild_wizball_scripts
cmake --build build --target rebuild_wizball_tilesets
cmake --build build --target rebuild_wizball_tilemaps
```

## PortMaster package build

The PortMaster packaging path builds a GLES2-targeted `aarch64` binary inside the builder container and assembles the output into a distributable zip.

Prerequisites: Docker and Docker Compose.

```bash
./scripts/build_portmaster.sh
```

That script will:

- pull the `portmaster-builder:aarch64-latest` image via `docker compose`
- start the `qemu-user-static` helper
- configure and build with `-DWIZBALL_RENDER_BACKEND=GLES2`
- install into `staging/`
- write the final archive to `WizBall.zip`

For an interactive builder shell instead:

```bash
./setup_docker.sh
```

The staging install excludes generated `.dat` files — rebuild them explicitly with the binary commands above before packaging or local `-dat` testing.
