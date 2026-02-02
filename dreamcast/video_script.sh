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

# Make staging directory variable an absolute path
stage_abs=$(
  cd -P -- "$stage" && pwd -P
)

vid_dir="$stage_abs/Video"

echo "file '$vid_dir/BadEnd.ogv' \ file '$vid_dir/BadKnux.ogv'" > $orig_dir/vids_badknux.txt
echo "file '$vid_dir/MREnd.ogv'  \ file '$vid_dir/BadMighty.ogv'" > $orig_dir/vids_badmighty.txt
echo "file '$vid_dir/MREnd.ogv'  \ file '$vid_dir/BadRay.ogv'" > $orig_dir/vids_badray.txt
echo "file '$vid_dir/BadEnd.ogv' \ file '$vid_dir/BadSonic.ogv'" > $orig_dir/vids_badsonic.txt
echo "file '$vid_dir/BadEnd.ogv' \ file '$vid_dir/BadSonic2.ogv'" > $orig_dir/vids_badsonic2.txt
echo "file '$vid_dir/BadEnd.ogv' \ file '$vid_dir/BadTails.ogv'" > $orig_dir/vids_badtails.txt


cd -- "$vid_dir"

shopt -s nullglob

ffmpeg -i Mania.ogv -i ../Music/IntroTee.ogg \
  -filter_complex "[1:a]adelay=4110:all=1[a]" \
  -map 0:v -map "[a]" \
  -c:v copy -c:a libvorbis -q:a 10 \
  -shortest \
  ManiaTee.ogv
ffmpeg -fflags "+genpts" -i ManiaTee.ogv -vf "scale=320:160,minterpolate=fps=23.98,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 ManiaTee.mpg
rm ManiaTee.ogv
mv ManiaTee.mpg ManiaTee.ogv

ffmpeg -i Mania.ogv -i ../Music/IntroHP.ogg -filter:v "trim=start=1.8,setpts=PTS-STARTPTS" -map 0:v:0 -map 1:a:0 -c:v libtheora -q:v 10 -c:a copy ManiaHP.ogv
ffmpeg -fflags "+genpts" -i ManiaHP.ogv -vf "scale=320:160,minterpolate=fps=23.98,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 ManiaHP.mpg
rm ManiaHP.ogv
mv ManiaHP.mpg ManiaHP.ogv

ffmpeg -safe 0 -f concat -i $orig_dir/vids_badknux.txt -i ../Music/BadEnd.ogg -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -af "apad=pad_dur=36000" -shortest BadKnuxMux.ogv
ffmpeg -fflags "+genpts" -i BadKnuxMux.ogv -vf "scale=320:160,minterpolate=fps=23.98,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 BadKnuxMux.mpg
rm BadKnuxMux.ogv
mv BadKnuxMux.mpg BadKnux.ogv

ffmpeg -safe 0 -f concat -i $orig_dir/vids_badmighty.txt -i ../Music/BadEnd.ogg -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -af "apad=pad_dur=36000" -shortest BadMightyMux.ogv
ffmpeg -fflags "+genpts" -i BadMightyMux.ogv -vf "scale=320:160,minterpolate=fps=23.98,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 BadMightyMux.mpg
rm BadMightyMux.ogv
mv BadMightyMux.mpg BadMighty.ogv

ffmpeg -safe 0 -f concat -i $orig_dir/vids_badray.txt -i ../Music/BadEnd.ogg -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -af "apad=pad_dur=36000" -shortest BadRayMux.ogv
ffmpeg -fflags "+genpts" -i BadRayMux.ogv -vf "scale=320:160,minterpolate=fps=23.98,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 BadRayMux.mpg
rm BadRayMux.ogv
mv BadRayMux.mpg BadRay.ogv

ffmpeg -safe 0 -f concat -i $orig_dir/vids_badsonic.txt -i ../Music/BadEnd.ogg -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -af "apad=pad_dur=36000" -shortest BadSonicMux.ogv
ffmpeg -fflags "+genpts" -i BadSonicMux.ogv -vf "scale=320:160,minterpolate=fps=23.98,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 BadSonicMux.mpg
rm BadSonicMux.ogv
mv BadSonicMux.mpg BadSonic.ogv

ffmpeg -safe 0 -f concat -i $orig_dir/vids_badsonic2.txt -i ../Music/BadEnd.ogg -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -af "apad=pad_dur=36000" -shortest BadSonic2Mux.ogv
ffmpeg -fflags "+genpts" -i BadSonic2Mux.ogv -vf "scale=320:160,minterpolate=fps=23.98,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 BadSonic2Mux.mpg
rm BadSonic2Mux.ogv
mv BadSonic2Mux.mpg BadSonic2.ogv

ffmpeg -safe 0 -f concat -i $orig_dir/vids_badtails.txt -i ../Music/BadEnd.ogg -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -af "apad=pad_dur=36000" -shortest BadTailsMux.ogv
ffmpeg -fflags "+genpts" -i BadTailsMux.ogv -vf "scale=320:160,minterpolate=fps=23.98,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 BadTailsMux.mpg
rm BadTailsMux.ogv
mv BadTailsMux.mpg BadTails.ogv

ffmpeg -i GoodEnd.ogv -i ../Music/GoodEnd.ogg -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -af "apad=pad_dur=36000" -shortest GoodEndMux.ogv
ffmpeg -fflags "+genpts" -i GoodEndMux.ogv -vf "scale=320:160,minterpolate=fps=23.98,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 GoodEndMux.mpg
rm GoodEndMux.ogv
mv GoodEndMux.mpg GoodEnd.ogv

rm "$orig_dir/vids_badknux.txt"
rm "$orig_dir/vids_badmighty.txt"
rm "$orig_dir/vids_badray.txt"
rm "$orig_dir/vids_badsonic.txt"
rm "$orig_dir/vids_badsonic2.txt"
rm "$orig_dir/vids_badtails.txt"