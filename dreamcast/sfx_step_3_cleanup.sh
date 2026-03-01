#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[[ $# -eq 1 ]] || die "Usage: ${0##*/} <stage_dir>"
stage=$1

[[ -d "$stage" ]] || die "not a directory: $stage"
[[ -d "$stage/StagingSoundFX" ]] || die "missing directory: $stage/StagingSoundFX"

orig_dir=$(pwd -P)
restore_dir() { cd -- "$orig_dir" || true; }
trap restore_dir EXIT

cd -- "$stage/StagingSoundFX"

find . -type f -iname '*.wav' -print0 |
while IFS= read -r -d '' file; do
  base=$(basename -- "$file")
  case "$base" in
    adpcm_resample_*) 
      # keep
      ;;
    *)
      rm -f -- "$file"
      ;;
  esac
done

