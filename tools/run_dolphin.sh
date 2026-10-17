#!/usr/bin/env bash
# Boot an image in Dolphin with an isolated user dir, dump frames + audio, stop after N seconds.
# usage: tools/run_dolphin.sh <image> <seconds>
set -uo pipefail
cd "$(dirname "$0")/.."
U="$PWD/.dolphin"
rm -rf "$U/Dump/Frames" "$U/Dump/Audio" "$U/Logs/dolphin.log"
/Applications/Dolphin.app/Contents/MacOS/Dolphin -u "$U" -b -e "$1" >/dev/null 2>&1 &
PID=$!
sleep "${2:-10}"
kill $PID 2>/dev/null; sleep 2; kill -9 $PID 2>/dev/null
ls -la "$U/Dump/Frames" "$U/Dump/Audio" 2>/dev/null
