#!/usr/bin/env sh

START_DIR=${1:-.}

# Ensure KOS_BASE is set
if [ -z "$KOS_BASE" ]; then
    echo "Error: KOS_BASE environment variable is not set."
    exit 1
fi

find "$START_DIR" -type f -iname "*.wav" -print0 |
while IFS= read -r -d '' file; do
    dir=$(dirname "$file")
    base=$(basename "$file")

    out1="$dir/fast_$base"

    ffmpeg -hide_banner -loglevel error -y -i "$file" -filter:a "rubberband=tempo=2.0:pitch=2.0" -c:a pcm_s16le -ar 44100 "$out1"
done

#echo "Done."

