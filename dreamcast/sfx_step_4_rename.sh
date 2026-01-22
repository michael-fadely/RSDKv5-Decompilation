#!/usr/bin/env sh

# rename any of the files created in step 2
# to match the original wav filenames from the RSDK file

START_DIR=${1:-.}

find "$START_DIR" -type f -iname "*.wav" -print0 |
while IFS= read -r -d '' file; do
    dir=$(dirname "$file")
    base=$(basename "$file")
    newbase=${base#adpcm_22k_}
    dest="$dir/$newbase"
    mv "$file" "$dest"
done

#echo "Done."

