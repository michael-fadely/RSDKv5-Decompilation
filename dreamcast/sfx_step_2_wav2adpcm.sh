#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[[ $# -eq 1 ]] || die "Usage: ${0##*/} <stage_dir>"

stage=$1
[[ -d "$stage" ]] || die "not a directory: $stage"
[[ -d "$stage/StagingSoundFX" ]] || die "missing directory: $stage/StagingSoundFX"

# Ensure KOS_BASE is set
: "${KOS_BASE:?KOS_BASE environment variable is not set}"

wav2adpcm="$KOS_BASE/utils/wav2adpcm/wav2adpcm"
[[ -x "$wav2adpcm" ]] || die "wav2adpcm not found or not executable: $wav2adpcm"

orig_dir=$(pwd -P)
restore_dir() { cd -- "$orig_dir" || true; }
trap restore_dir EXIT

cd -- "$stage/StagingSoundFX"

find . -type f -iname 'resample_*.wav' -print0 |
while IFS= read -r -d '' file; do
  dir=$(dirname -- "$file")
  base=$(basename -- "$file")

  out="$dir/adpcm_$base"
  "$wav2adpcm" -t "$file" "$out"
done

