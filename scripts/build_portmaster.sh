#!/usr/bin/env bash
set -euo pipefail

WORKDIR="$(pwd)"
# For packaging we require the GLES2 renderer; force it here so
# cross-builds always produce the GLES2-targeted binary.
RENDER_BACKEND="GLES2"
if [ "${WIZBALL_RENDER_BACKEND:-}" != "" ] && [ "${WIZBALL_RENDER_BACKEND}" != "GLES2" ]; then
  echo "Warning: ignoring WIZBALL_RENDER_BACKEND='${WIZBALL_RENDER_BACKEND}', forcing GLES2 for packaging."
fi

ARCH="aarch64"
BUILD_DIR="build-aarch64"
COMPOSE_SERVICE="builder"

for arg in "$@"; do
  case "$arg" in
    --armhf)
      ARCH="armhf"
      BUILD_DIR="build-armhf"
      COMPOSE_SERVICE="builder-armhf"
      ;;
    --aarch64) ;; # default, no-op
    *) echo "Unknown argument: $arg"; exit 1 ;;
  esac
done

echo "Building for arch: $ARCH (build dir: $BUILD_DIR, service: $COMPOSE_SERVICE)"

echo "Pulling builder image..."
if [ "$COMPOSE_SERVICE" = "builder-armhf" ]; then
  docker compose build "$COMPOSE_SERVICE"
else
  docker compose pull "$COMPOSE_SERVICE"
fi

echo "Starting qemu service..."
docker compose up -d qemu

echo "Running builder container to build ${ARCH}..."
docker compose run --rm "$COMPOSE_SERVICE" bash -lc '
  set -euo pipefail
  RENDER_BACKEND='"$RENDER_BACKEND"'
  BUILD_DIR='"$BUILD_DIR"'
  ARCH='"$ARCH"'
  if command -v apt >/dev/null 2>&1 && [ "$ARCH" = "aarch64" ]; then
    apt update
    apt install -y ninja-build || true
  fi
  cd /workspace
  TOOLCHAIN_ARG=""
  if [ "$ARCH" = "armhf" ]; then
    TOOLCHAIN_ARG="-DCMAKE_TOOLCHAIN_FILE=/toolchain-armhf.cmake"
  fi
  cmake -S . -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE=Release -DWIZBALL_RENDER_BACKEND=${RENDER_BACKEND} ${TOOLCHAIN_ARG}
  cmake --build "${BUILD_DIR}" -j$(nproc)
  cmake --install "${BUILD_DIR}" --prefix /workspace/staging
'

echo "Packaging staging into WizBall.zip..."

rm -f "$WORKDIR/WizBall.zip"
cd "$WORKDIR/staging"
zip -r "$WORKDIR/WizBall.zip" .
cd "$WORKDIR"

echo "Stopping qemu and cleaning up compose services..."
docker compose down

echo "Build and package complete: $WORKDIR/WizBall.zip"
