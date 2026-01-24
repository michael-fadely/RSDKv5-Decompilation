#!/bin/bash

orig_dir=$(pwd)

cd "$1"/StagingSoundFX

find ./ -type f -iname "*.wav" -print0 |
while IFS= read -r -d '' file; do
    dir=$(dirname "$file")
    base=$(basename "$file")

    case "$base" in
        adpcm_resample_*)
            newbase=${base#adpcm_resample_}
            ;;
        *)
            rm "$file"
            continue
            ;;
    esac
done

cd "$orig_dir"
