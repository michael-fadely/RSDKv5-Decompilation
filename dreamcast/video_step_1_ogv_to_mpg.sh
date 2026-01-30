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

for inpath in *.ogv *.OGV; do
  [[ -f "$inpath" ]] || continue

  base="${inpath%.*}"
  outpath="${base}.mpg"

  echo "Converting: $inpath -> $outpath"

  ffmpeg -hide_banner -y \
    -fflags +genpts -err_detect ignore_err \
    -i "$inpath" \
    -map "0:v:0" -map "0:a:0?" \
    -vf "scale=320:240:force_original_aspect_ratio=decrease:flags=lanczos,pad=320:240:-1:-1,setsar=1,fps=30,format=yuv420p" \
    -c:v mpeg1video -b:v 742k -minrate 742k -maxrate 742k -bufsize 742k \
    -g 12 -sc_threshold 0 -bf 0 \
    -c:a mp2 -b:a 16k -ar 32000 -ac 1 \
    -f mpeg -muxpreload 0 -muxdelay 0 \
    -shortest \
    "$outpath"
done

