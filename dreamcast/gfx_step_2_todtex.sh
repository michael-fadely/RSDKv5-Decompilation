#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[[ $# -eq 1 ]] || die "Usage: ${0##*/} <stage_dir>"

stage=$1
[[ -d "$stage" ]] || die "not a directory: $stage"
[[ -d "$stage/Sprites" ]] || die "missing directory: $stage/Sprites"

# Check KOS_BASE
: "${KOS_BASE:?KOS_BASE is not set}"

PVRTXEX="$KOS_BASE/utils/pvrtex/pvrtex"
[[ -x "$PVRTXEX" ]] || die "pvrtex not found or not executable: $PVRTXEX"

orig_dir=$(pwd -P)
restore_dir() { cd -- "$orig_dir" || true; }
trap restore_dir EXIT

cd -- "$stage/Sprites"

shopt -s nullglob

for dir in Global TMZ1 UI; do
  [[ -d "$dir" ]] || die "missing sprites subdir: $PWD/$dir"

  for file in "$dir"/*.png "$dir"/*.PNG; do
    base=${file%.[pP][nN][gG]}

    "$PVRTXEX" \
      -i "$file" \
      -o "$base.tex" \
      -f ARGB1555 \
      --compress "small"

    # Clean up inputs/previous outputs
    rm -f -- "$file"
    rm -f -- "$base.gif"

    # Replace .gif with the new .tex
    mv -f -- "$base.tex" "$base.gif"
  done
done

