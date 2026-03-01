#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[[ $# -eq 1 ]] || die "Usage: ${0##*/} <stage_dir>"

stage=$1
[[ -d "$stage" ]] || die "not a directory: $stage"
[[ -d "$stage/Music" ]] || die "missing directory: $stage/Music"

command -v ffmpeg >/dev/null 2>&1 || die "ffmpeg not found in PATH"

orig_dir=$(pwd -P)
restore_dir() { cd -- "$orig_dir" || true; }
trap restore_dir EXIT

cd -- "$stage/Music"

shopt -s nullglob
for in_file in *.ogg *.OGG; do
  name="${in_file%.*}"
  out_file="${name}.s8"

  ffmpeg -hide_banner -loglevel error -y \
    -i "$in_file" \
    -ar 22050 -ac 2 \
    -f s8 \
    -- "$out_file"
done

