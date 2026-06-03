#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[[ $# -eq 1 ]] || die "Usage: ${0##*/} <stage_dir>"
stage=$1
[[ -d "$stage" ]] || die "not a directory: $stage"
[[ -d "$stage/Sprites" ]] || die "missing directory: $stage/Sprites"

# ImageMagick compatibility (IM7: magick, IM6: convert/identify)
if command -v magick >/dev/null 2>&1; then
  IM_CONVERT=(magick)
elif command -v convert >/dev/null 2>&1 && command -v identify >/dev/null 2>&1; then
  IM_CONVERT=(convert)
else
  die "ImageMagick not found (need magick or convert)"
fi

orig_dir=$(pwd -P)
restore_dir() { cd -- "$orig_dir" || true; }
trap restore_dir EXIT

cd -- "$stage/Sprites"
shopt -s nullglob

for dir in Global TMZ1 UI; do
  [[ -d "$dir" ]] || die "missing sprites subdir: $PWD/$dir"

  for file in "$dir"/*.gif "$dir"/*.GIF; do
    base=${file%.[gG][iI][fF]}

    # Convert: palette0 becomes transparent, output RGBA PNG
    "${IM_CONVERT[@]}" "$file" \
      -alpha on \
      -transparent "#FF00FF" \
      -transparent "#01F001" \
      -transparent "#00FF00" \
      -define png:color-type=6 \
      "$base.png"
  done
done

