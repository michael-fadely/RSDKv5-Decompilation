#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[[ $# -eq 1 ]] || die "Usage: ${0##*/} <stage_dir>"

stage=$1
[[ -d "$stage" ]] || die "not a directory: $stage"
[[ -d "$stage/SoundFX" ]] || die "missing directory: $stage/SoundFX"

command -v ffmpeg  >/dev/null 2>&1 || die "ffmpeg not found in PATH"
command -v ffprobe >/dev/null 2>&1 || die "ffprobe not found in PATH"

orig_dir=$(pwd -P)
restore_dir() { cd -- "$orig_dir" || true; }
trap restore_dir EXIT

# Make staging directory variable an absolute path
stage_abs=$(
  cd -P -- "$stage" && pwd -P
)

sfx_dir="$stage_abs/SoundFX"
outdir="$stage_abs/StagingSoundFX"
mkdir -p -- "$outdir"

cd -- "$sfx_dir"

# temp file must end with .wav for ffmpeg autodetect
tmp_wav="$(mktemp -t sfx_tmp.XXXXXX).wav"
cleanup() { rm -f -- "$tmp_wav"; }
trap cleanup EXIT

# Return sample count for a wav file, or fail
get_samples() {
  local f=$1
  local d sr
  d=$(ffprobe -v error -select_streams a:0 -show_entries stream=duration \
      -of default=nokey=1:noprint_wrappers=1 -- "$f" | head -n1 || true)
  sr=$(ffprobe -v error -select_streams a:0 -show_entries stream=sample_rate \
      -of default=nokey=1:noprint_wrappers=1 -- "$f" | head -n1 || true)

  [[ -n "$d" && -n "$sr" && "$d" != "N/A" && "$sr" != "N/A" ]] || return 1
  awk -v d="$d" -v sr="$sr" 'BEGIN{ printf "%.0f\n", d*sr }'
}

while IFS= read -r -d '' file <&3; do
  exec </dev/null
  # file is like "./path/to/name.wav"
  rel=${file#./}
  dir=${rel%/*}
  base=${rel##*/}

  # If the wav is in the top dir, dir==rel (no slash). Fix that.
  if [[ "$rel" == "$base" ]]; then
    dir=""
  fi

  mkdir -p -- "$outdir/$dir"
  out1="$outdir/$dir/resample_$base"

  # Stereo files: the original game reads interleaved L/R samples as sequential
  # mono, so each sample appears twice (L=R). Reinterpret the raw PCM as mono.
  channels=$(ffprobe -v error -select_streams a:0 \
    -show_entries stream=channels \
    -of default=nokey=1:noprint_wrappers=1 -- "$file" | head -n1)
  input="$file"
  if [[ "$channels" -gt 1 ]]; then
    sr=$(ffprobe -v error -select_streams a:0 -show_entries stream=sample_rate \
      -of default=nokey=1:noprint_wrappers=1 -- "$file" | head -n1)
    codec=$(ffprobe -v error -select_streams a:0 -show_entries stream=codec_name \
      -of default=nokey=1:noprint_wrappers=1 -- "$file" | head -n1)
    if [[ "$codec" == "pcm_u8" ]]; then
      ffmpeg -nostdin -v error -y -i "$file" -f u8 -acodec pcm_u8 pipe: | \
        ffmpeg -nostdin -v error -y -f u8 -ar "$sr" -ac 1 -i pipe: \
          -c:a pcm_u8 -- "$tmp_wav"
    else
      ffmpeg -nostdin -v error -y -i "$file" -f s16le -acodec pcm_s16le pipe: | \
        ffmpeg -nostdin -v error -y -f s16le -ar "$sr" -ac 1 -i pipe: \
          -c:a pcm_s16le -- "$tmp_wav"
    fi
    input="$tmp_wav"
  fi

  bits=$(ffprobe -v error -select_streams a:0 \
    -show_entries stream=bits_per_sample \
    -of default=nokey=1:noprint_wrappers=1 -- "$input" | head -n1)

  if [[ "$bits" == "8" ]]; then
    # 8-bit file: convert u8 -> s16 temp, then stretch + resample
    ffmpeg -nostdin -v error -y -f wav -c:a pcm_u8 -i "$input" \
      -f wav -c:a pcm_s16le -- "$tmp_wav"

    if tsamples=$(get_samples "$tmp_wav"); then
      if (( tsamples > 65534 )); then
        ffmpeg -nostdin -v error -y -f wav -c:a pcm_u8 -i "$input" \
          -f wav -c:a pcm_s16le -ar 22050 -- "$tmp_wav"
      fi
    fi

    ffmpeg -nostdin -v error -y -f wav -c:a pcm_s16le -i "$tmp_wav" \
      -filter:a "rubberband=tempo=2.0:pitch=2.0" \
      -c:a pcm_s16le -ac 1 -- "$out1"
  else
    # 16-bit file: stretch + resample directly
    ffmpeg -nostdin -v error -y -f wav -c:a pcm_s16le -i "$input" \
      -filter:a "rubberband=tempo=2.0:pitch=2.0" \
      -c:a pcm_s16le -ar 44100 -ac 1 -- "$out1"

    if [[ -f "$out1" ]]; then
      if samples=$(get_samples "$out1"); then
        if (( samples > 65534 )); then
          rm -f -- "$out1"
          ffmpeg -nostdin -v error -y -f wav -c:a pcm_s16le -i "$input" \
            -filter:a "rubberband=tempo=2.0:pitch=2.0" \
            -c:a pcm_s16le -ar 22050 -ac 1 -- "$out1"
        fi
      fi
    fi
  fi
done 3< <(find . -type f -iname "*.wav" -print0)

