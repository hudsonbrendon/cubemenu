#!/usr/bin/env bash
# Build Cube Menu with the libogc2 container (devkitPPC + libogc2). Usage: ./build.sh [make args]
set -euo pipefail
cd "$(dirname "$0")"
python3 tools/mkassets.py
docker run --rm -v "$PWD":/src -w /src ghcr.io/extremscorner/libogc2:latest bash -lc 'make "$@"' _ "$@"
