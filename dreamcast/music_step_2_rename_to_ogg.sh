#!/usr/bin/env sh


orig_dir=$(pwd)

cd "$1"/Music


for file in *.s8 *.S8; do
    [ -e "$file" ] || continue  # skip if no match
    base=${file%.*}
    rm "$base.ogg"
    echo "Renaming $file → $base.ogg"
    mv "$file" "$base.ogg"
done

echo "Done."

cd "$orig_dir"
