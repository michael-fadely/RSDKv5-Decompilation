#!/usr/bin/env sh

START_DIR=${1:-.}

# Ensure KOS_BASE is set
if [ -z "$KOS_BASE" ]; then
    echo "Error: KOS_BASE environment variable is not set."
    exit 1
fi

find "$START_DIR" -type f -iname "22k_*.wav" -print0 |
while IFS= read -r -d '' file; do
    dir=$(dirname "$file")
    base=$(basename "$file")

    out="$dir/adpcm_$base"

    # all of the files created in step 1 get run through wav2adpcm
    "$KOS_BASE/utils/wav2adpcm/wav2adpcm" -t "$file" "$out"
done

#echo "Done."

