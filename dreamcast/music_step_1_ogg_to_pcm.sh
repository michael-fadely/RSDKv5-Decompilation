#!/usr/bin/env bash
set -euo pipefail

in_dir="${1:-.}"
out_dir="${2:-"$in_dir/s8_22050_raw"}"

mkdir -p "$out_dir"

shopt -s nullglob
for in_file in "$in_dir"/*.ogg "$in_dir"/*.OGG; do
  base="$(basename "$in_file")"
  name="${base%.*}"
  out_file="$out_dir/$name.s8"

  ffmpeg -hide_banner -loglevel error -y \
    -i "$in_file" \
    -ar 22050 -ac 2 \
    -f s8 \
    "$out_file"

  echo "Wrote: $out_file"
done
