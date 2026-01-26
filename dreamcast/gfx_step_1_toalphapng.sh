#!/bin/bash
set -e

orig_dir=$(pwd)

cd "$1"/Sprites

for dir in Global TMZ1 UI; do
  for file in "$dir"/*.gif; do
    [ -e "$file" ] || continue

    base=${file%.gif}

    palette0=$(
      magick identify -verbose "$file[0]" \
        | awk '
          $1=="Colormap:" {inmap=1; next}
          inmap && $1=="0:" {
              for (i=1;i<=NF;i++)
                  if ($i ~ /^#[0-9A-Fa-f]{6}/) { print substr($i,1,7); exit }
          }
        '
    )

    magick "$file" \
      -alpha on \
      -transparent "$palette0" \
      -define png:color-type=6 \
      "$base.png"
  done
done

cd "$orig_dir"
