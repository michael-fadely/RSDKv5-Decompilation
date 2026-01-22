#!/usr/bin/env sh

orig_dir=$(pwd)

cd "$1"/SoundFX

# Ensure KOS_BASE is set
if [ -z "$KOS_BASE" ]; then
    echo "Error: KOS_BASE environment variable is not set."
    exit 1
fi

find ./ -type f -iname "*.wav" -print0 |
while IFS= read -r -d '' file; do
    dir=$(dirname "$file")
    base=$(basename "$file")

    out1="$dir/22k_$base"

    output="$(ffmpeg -v error -f wav -c:a pcm_s16le -i $file -c:a pcm_s16le -ar 22050 -ac 1 $out1 2>&1 >/dev/null)"
    status=$?

    if echo "$output" | grep -q "data has size 1"; then
        echo "$file s16 decode failed, trying u8 decode"
        ffmpeg -hide_banner -loglevel error -y -f wav -c:a pcm_u8 -i "$file" -c:a pcm_s16le -ar 22050 -ac 1 "$out1"
    fi
done

echo "Done."

cd "$orig_dir"
