#!/usr/bin/env bash
set -euo pipefail

WORKDIR="$(pwd)"
# For packaging we require the GLES2 renderer; force it here so
# cross-builds always produce the GLES2-targeted binary.
RENDER_BACKEND="GLES2"
if [ "${WIZBALL_RENDER_BACKEND:-}" != "" ] && [ "${WIZBALL_RENDER_BACKEND}" != "GLES2" ]; then
  echo "Warning: ignoring WIZBALL_RENDER_BACKEND='${WIZBALL_RENDER_BACKEND}', forcing GLES2 for packaging."
fi

echo "Pulling builder image..."
docker compose pull builder

echo "Starting qemu service..."
docker compose up -d qemu

echo "Running builder container to build aarch64..."
docker compose run --rm builder bash -lc '
  set -euo pipefail
  RENDER_BACKEND='"$RENDER_BACKEND"'
  if command -v apt >/dev/null 2>&1; then
    apt update
    apt install -y ninja-build || true
  fi
  cd /workspace
  cmake -S . -B build-aarch64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DWIZBALL_RENDER_BACKEND=${RENDER_BACKEND}
  cmake --build build-aarch64 -j1
  cmake --install build-aarch64 --prefix /workspace/staging
'

echo "Packaging staging into wizball/WizBall.zip..."
rm -rf "$WORKDIR/wizball"
mkdir -p "$WORKDIR/wizball/wizball"

# Copy launcher and port metadata
cp -v portmaster/WizBall.sh "$WORKDIR/wizball/WizBall.sh" || true
cp -v portmaster/port.json "$WORKDIR/wizball/wizball.port.json" || true

# Copy built files from staging (if present)
if [ -d "$WORKDIR/staging/wizball" ]; then
  cp -rv "$WORKDIR/staging/wizball/"* "$WORKDIR/wizball/wizball/" || true
fi

zip -r "$WORKDIR/WizBall.zip" "$WORKDIR/wizball"

echo "Stopping qemu and cleaning up compose services..."
docker compose down

echo "Build and package complete: $WORKDIR/WizBall.zip"
