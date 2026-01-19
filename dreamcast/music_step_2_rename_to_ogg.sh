#!/usr/bin/env sh

for file in *.u8 *.U8; do
    [ -e "$file" ] || continue  # skip if no match
    base=${file%.*}
    echo "Renaming $file → $base.ogg"
    mv "$file" "$base.ogg"
done

echo "Done."

