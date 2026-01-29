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
  IM_IDENTIFY=(magick identify)
  IM_CONVERT=(magick)
elif command -v convert >/dev/null 2>&1 && command -v identify >/dev/null 2>&1; then
  IM_IDENTIFY=(identify)
  IM_CONVERT=(convert)
else
  die "ImageMagick not found (need magick or convert+identify)"
fi

orig_dir=$(pwd -P)
restore_dir() { cd -- "$orig_dir" || true; }
trap restore_dir EXIT

cd -- "$stage/Sprites"
shopt -s nullglob

get_palette0_hex() {
  # Extract palette entry 0 as "#RRGGBB" from the first frame of a GIF.
  local gif=$1
  LC_ALL=C "${IM_IDENTIFY[@]}" -verbose "${gif}[0]" |
    awk '
      $1=="Colormap:" {inmap=1; next}
      inmap && $1=="0:" {
        for (i=1;i<=NF;i++)
          if ($i ~ /^#[0-9A-Fa-f]{6}/) { print substr($i,1,7); exit }
      }
    '
}

for dir in Global TMZ1 UI; do
  [[ -d "$dir" ]] || die "missing sprites subdir: $PWD/$dir"

  for file in "$dir"/*.gif "$dir"/*.GIF; do
    base=${file%.[gG][iI][fF]}

    palette0=$(get_palette0_hex "$file" || true)
    [[ -n "$palette0" ]] || die "could not extract palette entry 0 from: $file"

    # Convert: palette0 becomes transparent, output RGBA PNG
    "${IM_CONVERT[@]}" "$file" \
      -alpha on \
      -transparent "$palette0" \
      -define png:color-type=6 \
      "$base.png"
  done
done

