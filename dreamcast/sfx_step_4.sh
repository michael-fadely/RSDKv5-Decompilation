#!/usr/bin/env sh

orig_dir=$(pwd)  

cd "$1"/SoundFX

find ./ -type f -iname "*.wav" -print0 |
while IFS= read -r -d '' file; do
    dir=$(dirname "$file")
    base=$(basename "$file")
    newbase=${base#adpcm_22k_}
    dest="$dir/$newbase"
    mv "$file" "$dest"
done

echo "Done."

cd "$orig_dir"
