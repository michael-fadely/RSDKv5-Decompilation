#!/bin/bash

mkdir -p "$2"
cp -r "$1"/Music "$2"
cp -r "$1"/SoundFX "$2"
mkdir -p "$2"/Sprites
cp -r "$1"/Sprites/Global "$2"/Sprites
cp -r "$1"/Sprites/TMZ1 "$2"/Sprites
cp -r "$1"/Sprites/UI "$2"/Sprites

./sfx_step_1_downsample.sh "$2"
./sfx_step_2_wav2adpcm.sh "$2"
./sfx_step_3_cleanup.sh "$2"
./sfx_step_4_move.sh "$2"

./music_step_1_ogg_to_pcm.sh "$2"
./music_step_2_rename_to_ogg.sh "$2"

./gfx_step_1_toalphapng.sh "$2"
./gfx_step_2_todtex.sh "$2"

echo "-- DONE --"

exit 0

