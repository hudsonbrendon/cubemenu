#!/usr/bin/env bash
# Build Cube Menu with the libogc2 container (devkitPPC + libogc2). Usage: ./build.sh [make args]
set -euo pipefail
cd "$(dirname "$0")"
# data/ (fonts, logo) is committed; regenerate it when a Python with Pillow + numpy is around
PY=""
for p in "${PYTHON:-}" python3 python3.14 python3.13 python3.12; do
    [ -n "$p" ] && command -v "$p" >/dev/null && "$p" -c "import PIL, numpy" 2>/dev/null && { PY=$p; break; }
done
if [ -n "$PY" ]; then "$PY" tools/mkassets.py; else echo "no Python with Pillow + numpy: using the committed data/" >&2; fi
docker run --rm -v "$PWD":/src -w /src ghcr.io/extremscorner/libogc2:latest bash -lc 'make "$@"' _ "$@"
