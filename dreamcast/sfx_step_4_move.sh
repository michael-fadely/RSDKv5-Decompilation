#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[[ $# -eq 1 ]] || die "Usage: ${0##*/} <stage_dir>"

stage=$1
[[ -d "$stage" ]] || die "not a directory: $stage"
[[ -d "$stage/StagingSoundFX" ]] || die "missing directory: $stage/StagingSoundFX"
[[ -d "$stage/SoundFX" ]] || die "missing directory: $stage/SoundFX"

orig_dir=$(pwd -P)
restore_dir() { cd -- "$orig_dir" || true; }
trap restore_dir EXIT

stage_abs=$(
  cd -P -- "$stage" && pwd -P
)

staging="$stage_abs/StagingSoundFX"
destroot="$stage_abs/SoundFX"

cd -- "$staging"

# Move only the files we expect
find . -type f -iname 'adpcm_resample_*.wav' -print0 |
while IFS= read -r -d '' file; do
  rel=${file#./}               # strip leading ./
  rel_dir=${rel%/*}
  base=${rel##*/}

  # handle files in top-level (no slash)
  if [[ "$rel" == "$base" ]]; then
    rel_dir=""
  fi

  newbase=${base#adpcm_resample_}

  destdir="$destroot/$rel_dir"
  dest="$destdir/$newbase"

  mkdir -p -- "$destdir"

  # Overwrite target atomically as much as mv allows
  mv -f -- "$file" "$dest"
done

# Remove empty directories under staging, then staging itself if empty
find . -depth -type d -empty -exec rmdir -- {} + 2>/dev/null || true
rmdir -- "$staging" 2>/dev/null || true

