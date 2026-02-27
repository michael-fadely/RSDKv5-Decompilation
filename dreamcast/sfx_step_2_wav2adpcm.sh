#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[[ $# -eq 1 ]] || die "Usage: ${0##*/} <stage_dir>"

stage=$1
[[ -d "$stage" ]] || die "not a directory: $stage"
[[ -d "$stage/StagingSoundFX" ]] || die "missing directory: $stage/StagingSoundFX"

command -v ffmpeg >/dev/null 2>&1 || { echo "error: ffmpeg not found in PATH" >&2; exit 1; }

orig_dir=$(pwd -P)
restore_dir() { cd -- "$orig_dir" || true; }
trap restore_dir EXIT

cd -- "$stage/StagingSoundFX"

find . -type f -iname 'resample_*.wav' -print0 |
while IFS= read -r -d '' file; do
  dir=$(dirname -- "$file")
  base=$(basename -- "$file")

  out="$dir/adpcm_$base"
  ffmpeg -i "$file" -acodec adpcm_yamaha "$out"
done

