#!/usr/bin/env sh

# remove all WAV files that existed prior to step 2

START_DIR=${1:-.}

find "$START_DIR" -type f -iname "*.wav" -print0 |
while IFS= read -r -d '' file; do
    dir=$(dirname "$file")
    base=$(basename "$file")

    case "$base" in
        adpcm_22k_*)
            newbase=${base#adpcm_22k_}
            ;;
        *)
            rm "$file"
            continue
            ;;
    esac
done

#echo "Done."

