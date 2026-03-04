#!/bin/bash

docker pull --platform=linux/arm64 ghcr.io/monkeyx-net/portmaster-build-templates/portmaster-builder:aarch64-latest
docker run --rm --privileged --network=host multiarch/qemu-user-static --reset -p yes
docker run -it --rm --network=host \
  -v "$(pwd)":/workspace \
  --platform=linux/arm64 \
  ghcr.io/monkeyx-net/portmaster-build-templates/portmaster-builder:aarch64-latest