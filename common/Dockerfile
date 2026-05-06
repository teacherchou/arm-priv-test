FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-aarch64-linux-gnu \
    binutils-aarch64-linux-gnu \
    make \
    qemu-system-arm \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

# Usage (from project root):
#   docker build -t arm-priv-test -f common/Dockerfile .
#   docker run -d --name arm-build -v $(pwd):/workspace arm-priv-test tail -f /dev/null
#   docker exec arm-build make test
