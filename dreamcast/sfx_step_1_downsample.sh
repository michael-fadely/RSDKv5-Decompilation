#!/bin/bash

orig_dir=$(pwd)

cd "$1"/SoundFX

mkdir -p "$1/StagingSoundFX"

outdir="$1/StagingSoundFX"

find ./ -type f -iname "*.wav" -print0 |
while IFS= read -r -d '' file; do
    dir=$(dirname "$file")
    base=$(basename "$file")
    mkdir -p "$outdir/$dir"
    out1="$outdir/$dir/resample_$base"

    # i know continue is < 50000 samples @ 22khz
    if echo "$base" | grep -q "Continue.wav"; then
        ffmpeg -v error -n -f wav -c:a pcm_u8 -i "$file" -c:a pcm_s16le tmpfile.wav
        ffmpeg -v error -n -f wav -c:a pcm_s16le -i tmpfile.wav -filter:a "rubberband=tempo=2.0:pitch=2.0" -c:a pcm_s16le -ar 22050 -ac 1 "$out1"
        rm tmpfile.wav
    else
	output="$(ffmpeg -v error -f wav -c:a pcm_s16le -i $file -filter:a "rubberband=tempo=2.0:pitch=2.0" -c:a pcm_s16le -ar 44100 -ac 1 $out1 2>&1 >/dev/null)"
        status=$?
        samples=$(soxi -s "$out1")
        if (( samples > 65534 )); then
            rm "$out1"
            ffmpeg -v error -f wav -c:a pcm_s16le -i "$file" -filter:a "rubberband=tempo=2.0:pitch=2.0" -c:a pcm_s16le -ar 22050 -ac 1 $out1
        fi

        if echo "$output" | grep -q "data has size 1"; then
            rm "$out1"
            ffmpeg -v error -n -f wav -c:a pcm_u8 -i "$file" -c:a pcm_s16le tmpfile.wav
            tsamples=$(soxi -s tmpfile.wav)
            if (( tsamples > 65534 )); then
                rm tmpfile.wav
                ffmpeg -v error -f wav -c:a pcm_u8 -i "$file" -c:a pcm_s16le -ar 22050 tmpfile.wav
            fi
            ffmpeg -v error -n -f wav -c:a pcm_s16le -i tmpfile.wav -filter:a "rubberband=tempo=2.0:pitch=2.0" -c:a pcm_s16le "$out1"
            rm tmpfile.wav
        fi
    fi
done

cd "$orig_dir"
