#!/bin/sh
set -e

orig_dir=$(pwd)

cd "$1"/Sprites

for dir in Global TMZ1 UI; do
  for file in "$dir"/*.gif; do
    [ -e "$file" ] || continue

    base=${file%.gif}

    echo "Converting: $file"

    magick "$file" \
      -alpha on \
      -transparent '#FF00FF' \
      -define png:color-type=6 \
      "$base.png"
  done
done

cd "$orig_dir"
