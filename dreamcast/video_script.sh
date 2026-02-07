#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[[ $# -eq 2 ]] || die "Usage: ${0##*/} <source_dir> <stage_dir>"

source=$1
[[ -d "$source" ]] || die "not a directory: $source"
[[ -d "$source/Music" ]] || die "missing directory: $source/Music"

stage=$2
[[ -d "$stage" ]] || die "not a directory: $stage"
[[ -d "$stage/Video" ]] || die "missing directory: $stage/Video"

command -v ffmpeg >/dev/null 2>&1 || { echo "error: ffmpeg not found in PATH" >&2; exit 1; }

orig_dir=$(pwd -P)

# Make staging directory variable an absolute path
source_abs=$(
  cd -P -- "$source" && pwd -P
)

# Make staging directory variable an absolute path
stage_abs=$(
  cd -P -- "$stage" && pwd -P
)

mus_dir="$source_abs/Music"

vid_dir="$stage_abs/Video"

cd -- "$vid_dir"

cp "$orig_dir"/*.txt "$vid_dir"

shopt -s nullglob

ffmpeg -i Mania.ogv -i "$mus_dir/IntroTee.ogg" -filter_complex "[1:a]adelay=5125:all=1[a]" -map 0:v -map "[a]" -c:v copy -c:a libvorbis -q:a 10 -shortest ManiaTeeMux.ogv
ffmpeg -i Mania.ogv -i "$mus_dir/IntroHP.ogg" -filter:v "trim=start=1.8,setpts=PTS-STARTPTS" -map 0:v:0 -map 1:a:0 -c:v libtheora -q:v 10 -c:a copy ManiaHPMux.ogv

ffmpeg -safe 0 -f concat -i vids_badknux.txt -i "$mus_dir/BadEnd.ogg" -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -q:a 10 -af "apad=pad_dur=36000" -shortest BadKnuxMux.ogv
ffmpeg -safe 0 -f concat -i vids_badmighty.txt -i "$mus_dir/BadEnd.ogg" -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -q:a 10 -af "apad=pad_dur=36000" -shortest BadMightyMux.ogv
ffmpeg -safe 0 -f concat -i vids_badray.txt -i "$mus_dir/BadEnd.ogg" -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -q:a 10 -af "apad=pad_dur=36000" -shortest BadRayMux.ogv
ffmpeg -safe 0 -f concat -i vids_badsonic.txt -i "$mus_dir/BadEnd.ogg" -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -q:a 10 -af "apad=pad_dur=36000" -shortest BadSonicMux.ogv
ffmpeg -safe 0 -f concat -i vids_badsonic2.txt -i "$mus_dir/BadEnd.ogg" -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -q:a 10 -af "apad=pad_dur=36000" -shortest BadSonic2Mux.ogv
ffmpeg -safe 0 -f concat -i vids_badtails.txt -i "$mus_dir/BadEnd.ogg" -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -q:a 10 -af "apad=pad_dur=36000" -shortest BadTailsMux.ogv
ffmpeg -i GoodEnd.ogv -i "$mus_dir/GoodEnd.ogg" -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -q:a 10 -af "apad=pad_dur=36000" -shortest GoodEndMux.ogv

ffmpeg -fflags "+genpts" -i ManiaTeeMux.ogv -af "volume=1.5" -vf "scale=320:160,fps=23.98,format=yuv420p" -b:v 500k -minrate 500k -maxrate 500k -bufsize 400k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 ManiaTeeMux.mpg
ffmpeg -fflags "+genpts" -i ManiaHPMux.ogv -af "volume=1.5" -vf "scale=320:160,fps=23.98,format=yuv420p" -b:v 500k -minrate 500k -maxrate 500k -bufsize 400k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 ManiaHPMux.mpg
ffmpeg -fflags "+genpts" -i BadKnuxMux.ogv -af "volume=1.5" -vf "scale=320:160,minterpolate=fps=23.98,format=yuv420p" -b:v 500k -minrate 500k -maxrate 500k -bufsize 400k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 BadKnuxMux.mpg
ffmpeg -fflags "+genpts" -i BadMightyMux.ogv -af "volume=1.5" -vf "scale=320:160,minterpolate=fps=23.98,format=yuv420p" -b:v 500k -minrate 500k -maxrate 500k -bufsize 400k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 BadMightyMux.mpg
ffmpeg -fflags "+genpts" -i BadRayMux.ogv -af "volume=1.5" -vf "scale=320:160,minterpolate=fps=23.98,format=yuv420p" -b:v 500k -minrate 500k -maxrate 500k -bufsize 400k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 BadRayMux.mpg
ffmpeg -fflags "+genpts" -i BadSonicMux.ogv -af "volume=1.5" -vf "scale=320:160,minterpolate=fps=23.98,format=yuv420p" -b:v 500k -minrate 500k -maxrate 500k -bufsize 400k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 BadSonicMux.mpg
ffmpeg -fflags "+genpts" -i BadSonic2Mux.ogv -af "volume=1.5" -vf "scale=320:160,minterpolate=fps=23.98,format=yuv420p" -b:v 500k -minrate 500k -maxrate 500k -bufsize 400k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 BadSonic2Mux.mpg
ffmpeg -fflags "+genpts" -i BadTailsMux.ogv -af "volume=1.5" -vf "scale=320:160,minterpolate=fps=23.98,format=yuv420p" -b:v 500k -minrate 500k -maxrate 500k -bufsize 400k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 BadTailsMux.mpg
ffmpeg -fflags "+genpts" -i GoodEndMux.ogv -af "volume=1.5" -vf "scale=320:160,minterpolate=fps=23.98,format=yuv420p" -b:v 500k -minrate 500k -maxrate 500k -bufsize 400k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 GoodEndMux.mpg

cp ManiaHPMux.mpg ManiaHP.mpg
cp ManiaTeeMux.mpg ManiaTee.mpg
cp BadKnuxMux.mpg BadKnux.ogv
cp BadMightyMux.mpg BadMighty.ogv
cp BadRayMux.mpg BadRay.ogv
cp BadSonicMux.mpg BadSonic.ogv
cp BadSonic2Mux.mpg BadSonic2.ogv
cp BadTailsMux.mpg BadTails.ogv
cp GoodEndMux.mpg GoodEnd.ogv
