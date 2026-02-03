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

mkdir -p Staged

shopt -s nullglob

ffmpeg -i Mania.ogv -i ../Music/IntroTee.ogg \
  -filter_complex "[1:a]adelay=4110:all=1[a]" \
  -map 0:v -map "[a]" \
  -c:v copy -c:a libvorbis -q:a 10 \
  -shortest \
  Staged/ManiaTee.ogv
ffmpeg -fflags "+genpts" -i Staged/ManiaTee.ogv -vf "scale=320:240:force_original_aspect_ratio=decrease:flags=lanczos,pad=320:240:-1:-1,minterpolate=fps=30,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 Staged/ManiaTee.mpg
#rm Staged/ManiaTee.ogv

ffmpeg -i Mania.ogv -i ../Music/IntroHP.ogg -filter:v "trim=start=1.8,setpts=PTS-STARTPTS" -map 0:v:0 -map 1:a:0 -c:v libtheora -q:v 10 -c:a copy Staged/ManiaHP.ogv
ffmpeg -fflags "+genpts" -i Staged/ManiaHP.ogv -vf "scale=320:240:force_original_aspect_ratio=decrease:flags=lanczos,pad=320:240:-1:-1,minterpolate=fps=30,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 Staged/ManiaHP.mpg
#rm Staged/ManiaHP.ogv

ffmpeg -safe 0 -f concat -i $orig_dir/vids_badknux.txt -i ../Music/BadEnd.ogg -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -af "apad=pad_dur=36000" -shortest Staged/BadKnuxMux.ogv
ffmpeg -fflags "+genpts" -i Staged/BadKnuxMux.ogv -vf "scale=320:240:force_original_aspect_ratio=decrease:flags=lanczos,pad=320:240:-1:-1,minterpolate=fps=30,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 Staged/BadKnuxMux.mpg
#rm Staged/BadKnuxMux.ogv

ffmpeg -safe 0 -f concat -i $orig_dir/vids_badmighty.txt -i ../Music/BadEnd.ogg -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -af "apad=pad_dur=36000" -shortest Staged/BadMightyMux.ogv
ffmpeg -fflags "+genpts" -i Staged/BadMightyMux.ogv -vf "scale=320:240:force_original_aspect_ratio=decrease:flags=lanczos,pad=320:240:-1:-1,minterpolate=fps=30,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 Staged/BadMightyMux.mpg
#rm Staged/BadMightyMux.ogv

ffmpeg -safe 0 -f concat -i $orig_dir/vids_badray.txt -i ../Music/BadEnd.ogg -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -af "apad=pad_dur=36000" -shortest Staged/BadRayMux.ogv
ffmpeg -fflags "+genpts" -i Staged/BadRayMux.ogv -vf "scale=320:240:force_original_aspect_ratio=decrease:flags=lanczos,pad=320:240:-1:-1,minterpolate=fps=30,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 Staged/BadRayMux.mpg
#rm Staged/BadRayMux.ogv

ffmpeg -safe 0 -f concat -i $orig_dir/vids_badsonic.txt -i ../Music/BadEnd.ogg -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -af "apad=pad_dur=36000" -shortest Staged/BadSonicMux.ogv
ffmpeg -fflags "+genpts" -i Staged/BadSonicMux.ogv -vf "scale=320:240:force_original_aspect_ratio=decrease:flags=lanczos,pad=320:240:-1:-1,minterpolate=fps=30,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 Staged/BadSonicMux.mpg
#rm Staged/BadSonicMux.ogv

ffmpeg -safe 0 -f concat -i $orig_dir/vids_badsonic2.txt -i ../Music/BadEnd.ogg -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -af "apad=pad_dur=36000" -shortest Staged/BadSonic2Mux.ogv
ffmpeg -fflags "+genpts" -i Staged/BadSonic2Mux.ogv -vf "scale=320:240:force_original_aspect_ratio=decrease:flags=lanczos,pad=320:240:-1:-1,minterpolate=fps=30,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 Staged/BadSonic2Mux.mpg
#rm Staged/BadSonic2Mux.ogv

ffmpeg -safe 0 -f concat -i $orig_dir/vids_badtails.txt -i ../Music/BadEnd.ogg -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -af "apad=pad_dur=36000" -shortest Staged/BadTailsMux.ogv
ffmpeg -fflags "+genpts" -i Staged/BadTailsMux.ogv -vf "scale=320:240:force_original_aspect_ratio=decrease:flags=lanczos,pad=320:240:-1:-1,minterpolate=fps=30,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 Staged/BadTailsMux.mpg
#rm Staged/BadTailsMux.ogv

ffmpeg -i GoodEnd.ogv -i ../Music/GoodEnd.ogg -map 0:v:0 -map 1:a:0 -c:v copy -c:a libvorbis -af "apad=pad_dur=36000" -shortest Staged/GoodEndMux.ogv
ffmpeg -fflags "+genpts" -i Staged/GoodEndMux.ogv -vf "scale=320:240:force_original_aspect_ratio=decrease:flags=lanczos,pad=320:240:-1:-1,minterpolate=fps=30,format=yuv420p" -b:v 400k -minrate 400k -maxrate 400k -bufsize 300k -c:a mp2 -b:a 64k -ar 32000 -ac 1 -f mpeg -packet_size 2048 Staged/GoodEndMux.mpg
#rm Staged/GoodEndMux.ogv

cp Staged/ManiaTee.mpg ManiaTee.ogv
cp Staged/ManiaHP.mpg ManiaHP.ogv
cp Staged/BadKnuxMux.mpg BadKnux.ogv
cp Staged/BadMightyMux.mpg BadMighty.ogv
cp Staged/BadRayMux.mpg BadRay.ogv
cp Staged/BadSonicMux.mpg BadSonic.ogv
cp Staged/BadSonic2Mux.mpg BadSonic2.ogv
cp Staged/BadTailsMux.mpg BadTails.ogv
cp Staged/GoodEndMux.mpg GoodEnd.ogv

#rm "$orig_dir/vids_badknux.txt"
#rm "$orig_dir/vids_badmighty.txt"
#rm "$orig_dir/vids_badray.txt"
#rm "$orig_dir/vids_badsonic.txt"
#rm "$orig_dir/vids_badsonic2.txt"
#rm "$orig_dir/vids_badtails.txt"
exit 0

