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

    out1="$dir/22k_$base"

    # first try forced s16le pcm
    # some of the WAV files dumped by RetroED report they are IEEE float but they aren't, ask me how I know
    output="$(ffmpeg -v error -f wav -c:a pcm_s16le -i $file -c:a pcm_s16le -ar 22050 -ac 1 $out1 2>&1 >/dev/null)"
    status=$?

    if echo "$output" | grep -q "data has size 1"; then
        echo "$file s16 decode failed, trying u8 decode"
        # if the prev step failed, they were u8 WAV, try again
        ffmpeg -hide_banner -loglevel error -y -f wav -c:a pcm_u8 -i "$file" -c:a pcm_s16le -ar 22050 -ac 1 "$out1"
    fi
done

#echo "Done."

