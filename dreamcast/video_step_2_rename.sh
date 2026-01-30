#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[[ $# -eq 1 ]] || die "Usage: ${0##*/} <stage_dir>"

stage=$1
[[ -d "$stage" ]] || die "not a directory: $stage"
[[ -d "$stage/Video" ]] || die "missing directory: $stage/Video"
  
command -v ffmpeg >/dev/null 2>&1 || { echo "error: ffmpeg not found in PATH" >&2; exit 1; }

orig_dir=$(pwd -P)
restore_dir() { cd -- "$orig_dir" || true; }
trap restore_dir EXIT
    
# Make staging directory variable an absolute path
stage_abs=$(
  cd -P -- "$stage" && pwd -P
)
    
vid_dir="$stage_abs/Video"
    
cd -- "$vid_dir"
    
shopt -s nullglob

for inpath in *.mpg *.MPG; do
  [[ -f "$inpath" ]] || continue

  outpath="${inpath%.*}.ogv"

  echo "Renaming: $inpath -> $outpath"
  mv -- "$inpath" "$outpath"
done

