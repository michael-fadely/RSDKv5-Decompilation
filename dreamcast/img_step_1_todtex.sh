#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[[ $# -eq 1 ]] || die "Usage: ${0##*/} <stage_dir>"

stage=$1
[[ -d "$stage" ]] || die "not a directory: $stage"
[[ -d "$stage/Images" ]] || die "missing directory: $stage/Images"

# Check KOS_BASE
: "${KOS_BASE:?KOS_BASE is not set}"

PVRTXEX="$KOS_BASE/utils/pvrtex/pvrtex"
[[ -x "$PVRTXEX" ]] || die "pvrtex not found or not executable: $PVRTXEX"

orig_dir=$(pwd -P)
restore_dir() { cd -- "$orig_dir" || true; }
trap restore_dir EXIT

cd -- "$stage/Images"

shopt -s nullglob

for file in *.png *.PNG; do
  base=${file%.[pP][nN][gG]}

  "$PVRTXEX" \
    -i "$file" \
    -o "$base.tex" \
    -f RGB565 \
    --compress "small"

  # Clean up inputs/previous outputs
  rm -f -- "$file"

  # Replace .png with the new .tex
  mv -f -- "$base.tex" "$file"
done

