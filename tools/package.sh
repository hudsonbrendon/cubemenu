#!/bin/sh
# Release zip: boot.iso + cubemenu.dol + the card tool + docs.
set -e
cd "$(dirname "$0")/.."
V=$(cat VERSION)
OUT=dist/cubemenu-$V
rm -rf "$OUT" "$OUT.zip"
mkdir -p "$OUT/tools/cube_card"
cp dist/boot.iso dist/cubemenu.dol README.md LICENSE CHANGELOG.md "$OUT/"
cp tools/__init__.py "$OUT/tools/"
cp tools/cube_card/*.py "$OUT/tools/cube_card/"
cp -r docs "$OUT/docs"
rm -rf "$OUT/docs/screenshots"
(cd dist && zip -qr "cubemenu-$V.zip" "cubemenu-$V")
echo "dist/cubemenu-$V.zip"
