#!/usr/bin/env bash
# Cross-compile a Windows .exe from Linux using the MinGW Docker image.
# Usage: ./scripts/build_windows.sh
#
# Output: build-windows/wizball.exe + required DLLs copied alongside it.
# Pair the resulting build with data.zip when packaging a release.
set -euo pipefail

WORKDIR="$(pwd)"
BUILD_DIR="build-windows"

echo "Building builder-windows image (MinGW cross-compile)..."
docker compose build builder-windows

echo "Running cross-compile inside container..."
docker compose run --rm builder-windows bash -lc '
  set -euo pipefail
  cd /workspace
  cmake -S . -B '"$BUILD_DIR"' -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/toolchain-windows.cmake \
    -DWIZBALL_PORTMASTER=OFF
  cmake --build '"$BUILD_DIR"' --target wizball -j$(nproc)

  # Copy the SDL2 runtime DLLs next to wizball.exe so the build directory is self-contained.
  echo "Copying SDL2 DLLs..."
  cp /usr/x86_64-w64-mingw32/bin/SDL2.dll           '"$BUILD_DIR"'/
  cp /usr/x86_64-w64-mingw32/bin/SDL2_mixer.dll     '"$BUILD_DIR"'/
  cp /usr/x86_64-w64-mingw32/bin/SDL2_image.dll     '"$BUILD_DIR"'/
  # SDL2_mixer optional codec DLLs (ship all that are present)
  for dll in /usr/x86_64-w64-mingw32/bin/lib*.dll; do
    [ -f "$dll" ] && cp "$dll" '"$BUILD_DIR"'/ || true
  done
'

echo ""
echo "Windows build complete."
echo "  Binary : $WORKDIR/$BUILD_DIR/wizball.exe"
echo "  DLLs   : $WORKDIR/$BUILD_DIR/*.dll"
echo ""
echo "Ship the contents of $BUILD_DIR/ together with data.zip."
