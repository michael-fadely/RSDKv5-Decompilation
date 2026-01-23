#!/usr/bin/env sh

orig_dir=$(pwd)

cd "$1"/StagingSoundFX

# Ensure KOS_BASE is set
if [ -z "$KOS_BASE" ]; then
    echo "Error: KOS_BASE environment variable is not set."
    exit 1
fi

find ./ -type f -iname "resample_*.wav" -print0 |
while IFS= read -r -d '' file; do
    dir=$(dirname "$file")
    base=$(basename "$file")

    out="$dir/adpcm_$base"

    "$KOS_BASE/utils/wav2adpcm/wav2adpcm" -t "$file" "$out"
done

echo "Done."

cd "$orig_dir"
