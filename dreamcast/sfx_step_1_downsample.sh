#!/usr/bin/env sh

orig_dir=$(pwd)

cd "$1"/SoundFX

mkdir -p "$1/StagingSoundFX"

outdir="$1/StagingSoundFX"

# Ensure KOS_BASE is set
if [ -z "$KOS_BASE" ]; then
    echo "Error: KOS_BASE environment variable is not set."
    exit 1
fi

find ./ -type f -iname "*.wav" -print0 |
while IFS= read -r -d '' file; do
    dir=$(dirname "$file")
    base=$(basename "$file")
    mkdir -p "$outdir/$dir"
    out1="$outdir/$dir/22k_$base"

#    echo "$out1"

    if echo "$base" | grep "Continue.wav"; then
        if echo "$output" | grep -q "data has size 1"; then
            #echo "$file s16 decode failed, trying u8 decode"
            #rm "$out1"
            ffmpeg -v error -n -f wav -c:a pcm_u8 -i "$file" -c:a pcm_s16le tmpfile.wav
            ffmpeg -v error -n -f wav -c:a pcm_s16le -i tmpfile.wav -filter:a "rubberband=tempo=2.0:pitch=2.0" -c:a pcm_s16le "$out1"
            rm tmpfile.wav
        fi

    else        output="$(ffmpeg -v error -f wav -c:a pcm_s16le -i $file -filter:a "rubberband=tempo=2.0:pitch=2.0" -c:a pcm_s16le -ar 44100 -ac 1 $out1 2>&1 >/dev/null)"
        status=$?

        if echo "$output" | grep -q "data has size 1"; then
            echo "$file s16 decode failed, trying u8 decode"
            rm "$out1"
            ffmpeg -v error -n -f wav -c:a pcm_u8 -i "$file" -c:a pcm_s16le tmpfile.wav
            ffmpeg -v error -n -f wav -c:a pcm_s16le -i tmpfile.wav -filter:a "rubberband=tempo=2.0:pitch=2.0" -c:a pcm_s16le "$out1"
            rm tmpfile.wav
        fi
    fi
done

echo "Done."

cd "$orig_dir"
