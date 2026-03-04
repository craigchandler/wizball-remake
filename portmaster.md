# Building WizBall for PortMaster

This document describes how to build WizBall for **PortMaster** targets:

* `aarch64`
* `armhf`

The build uses the **PortMaster builder container** and **CMake + Ninja**.

---

# Prerequisites

Install Docker on the host machine.

Clone the repository:

```bash
git clone <your_repo>
cd <your_repo>
```

---

# Pull the PortMaster builder image

```bash
docker pull ghcr.io/monkeyx-net/portmaster-build-templates/portmaster-builder:aarch64-latest
```

Enable QEMU for cross-architecture builds:

```bash
docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
```

---

# Start the builder container

```bash
docker run -it --rm \
  -v "$(pwd)":/workspace \
  --platform=linux/arm64 \
  ghcr.io/monkeyx-net/portmaster-build-templates/portmaster-builder:aarch64-latest
```

Inside the container the project will appear at:

```
/workspace
```

---

# Install build tools (inside container)

```bash
apt update
apt install -y ninja-build
```

---

# Build aarch64 version

```bash
cd /workspace

cmake -S . \
      -B build-aarch64 \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Release

cmake --build build-aarch64 -j1

cmake --install build-aarch64 --prefix /workspace/staging
```

Output will be installed to:

```
staging/wizball/wizball/
```

Result:

```
wizball.aarch64
datafiles/
```

---

# Build armhf version

Open the **armhf build environment** (PortMaster VM or container).

Then run:

```bash
cd /workspace

cmake -S . \
      -B build-armhf \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Release

cmake --build build-armhf -j1

cmake --install build-armhf --prefix /workspace/staging
```

Now the staging folder contains both binaries.

---

# Final staging layout

```
staging/
  wizball/
    wizball/
      wizball.aarch64
      wizball.armhf
      datafiles/
```

---

# PortMaster packaging structure

Create the final port directory:

```
wizball/
  wizball.port.json
  README.md
  screenshot.png
  gameinfo.xml
  WizBall.sh
  wizball/
    wizball.aarch64
    wizball.armhf
    datafiles/
```

---

# Launcher script (WizBall.sh)

```bash
#!/bin/bash

source /opt/system/Tools/PortMaster/control.txt
get_controls

GAMEDIR="/$directory/ports/wizball/wizball"
cd "$GAMEDIR"

export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

./wizball.${DEVICE_ARCH}

pm_finish
```

---

# Port JSON

Ensure the port supports both architectures:

```json
"arch": ["aarch64","armhf"]
```

---

# Packaging

Zip the outer folder:

```bash
zip -r wizball.zip wizball
```

This zip can be distributed as a PortMaster port.

---

# Notes

* Ninja is recommended because it avoids some `make` crashes when building under QEMU.
* Use `-j1` for stability in emulated environments.
* Ensure the game loads assets relative to the working directory:

```
./datafiles/
```
