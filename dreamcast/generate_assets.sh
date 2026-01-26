#!/bin/bash

sourcedir=$(realpath "$1")

mkdir -p "$2"

stagedir=$(realpath "$2")

cp -r "$sourcedir"/Music "$stagedir"
cp -r "$sourcedir"/SoundFX "$stagedir"
mkdir -p "$stagedir"/Sprites
cp -r "$sourcedir"/Sprites/Global "$stagedir"/Sprites
cp -r "$sourcedir"/Sprites/TMZ1 "$stagedir"/Sprites
cp -r "$sourcedir"/Sprites/UI "$stagedir"/Sprites

./sfx_step_1_downsample.sh "$stagedir"
./sfx_step_2_wav2adpcm.sh "$stagedir"
./sfx_step_3_cleanup.sh "$stagedir"
./sfx_step_4_move.sh "$stagedir"

./music_step_1_ogg_to_pcm.sh "$stagedir"
./music_step_2_rename_to_ogg.sh "$stagedir"

./gfx_step_1_toalphapng.sh "$stagedir"
./gfx_step_2_todtex.sh "$stagedir"

echo "-- DONE --"

exit 0

