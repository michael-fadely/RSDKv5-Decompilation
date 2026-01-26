#!/bin/bash

set -euo pipefail

orig_dir=$(pwd)

cd "$1"/Music

shopt -s nullglob
for in_file in *.ogg *.OGG; do
  base="$(basename "$in_file")"
  name="${base%.*}"
  out_file="$name.s8"

  ffmpeg -hide_banner -loglevel error -y \
    -i "$in_file" \
    -ar 22050 -ac 2 \
    -f s8 \
    "$out_file"
done

cd "$orig_dir"
