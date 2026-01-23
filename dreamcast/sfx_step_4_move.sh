#!/usr/bin/env sh

orig_dir=$(pwd)  

cd "$1"/StagingSoundFX

find ./ -type f -iname "*.wav" -print0 |
while IFS= read -r -d '' file; do
    dir=$(dirname "$file")
    base=$(basename "$file")
    newbase=${base#adpcm_resample_}
    dest="$1/SoundFX/$dir/$newbase"
    rm "$dest"
    mv "$file" "$dest"
    rmdir "$dir"
done

cd "$orig_dir"

rmdir "$1"/StagingSoundFX

echo "Done."

