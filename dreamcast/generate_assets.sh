#!/bin/bash

mkdir -p "$2"
mkdir -p "$2"/Sprites
cp -r "$1"/Sprites/Global "$2"/Sprites
cp -r "$1"/Sprites/TMZ1 "$2"/Sprites
cp -r "$1"/Sprites/UI "$2"/Sprites

./gfx_step_1_toalphapng.sh "$2"
./gfx_step_2_todtex.sh "$2"

echo "-- DONE --"

exit 0

