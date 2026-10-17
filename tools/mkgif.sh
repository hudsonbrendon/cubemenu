#!/bin/sh
# README GIF from the demo video: a stretch of browsing + the launch scene. usage: tools/mkgif.sh dist/cubemenu-demo.mp4
set -e
IN=${1:-dist/cubemenu-demo.mp4}
OUT=docs/screenshots/demo.gif
DUR=$(ffprobe -v error -show_entries format=duration -of csv=p=0 "$IN")
LAUNCH=$(python3 -c "print(max(0, $DUR - 6.2))")
T=$(mktemp -d)
ffmpeg -v error -y -ss 3.6 -t 7.5 -i "$IN" -an -vf "fps=15,scale=560:-1:flags=lanczos" "$T/a.mp4"
ffmpeg -v error -y -ss "$LAUNCH" -t 5.4 -i "$IN" -an -vf "fps=15,scale=560:-1:flags=lanczos" "$T/b.mp4"
printf "file '%s'\nfile '%s'\n" "$T/a.mp4" "$T/b.mp4" > "$T/list.txt"
ffmpeg -v error -y -f concat -safe 0 -i "$T/list.txt" -c copy "$T/c.mp4"
ffmpeg -v error -y -i "$T/c.mp4" -vf "palettegen=max_colors=192:stats_mode=diff" "$T/p.png"
ffmpeg -v error -y -i "$T/c.mp4" -i "$T/p.png" -lavfi "paletteuse=dither=bayer:bayer_scale=4:diff_mode=rectangle" "$OUT"
rm -rf "$T"
ls -la "$OUT"
