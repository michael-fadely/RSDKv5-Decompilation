#!/bin/sh

orig_dir=$(pwd)
#trap 'cd "$orig_dir"' EXIT

cd "$1"/Sprites

# Fail on error
set -e

# Check KOS_BASE
if [ -z "$KOS_BASE" ]; then
    echo "Error: KOS_BASE is not set"
    exit 1
fi

PVRTXEX="$KOS_BASE/utils/pvrtex/pvrtex"

# Check pvrtex exists
if [ ! -x "$PVRTXEX" ]; then
    echo "Error: pvrtex not found or not executable at:"
    echo "  $PVRTXEX"
    exit 1
fi

# Loop over PNG files
for dir in Global TMZ1 UI; do
  for file in "$dir"/*.png; do
    # Handle case where no PNGs exist
    [ -e "$file" ] || continue

    base="${file%.png}"

    echo "Converting: $file"

    "$PVRTXEX" \
        -i "$file" \
        -o "$base.tex" \
        -f ARGB1555 \
        --compress "small"

    rm "$file"
    rm "$base.gif"
    mv "$base.tex" "$base.gif"
  done
done

cd "$orig_dir"
