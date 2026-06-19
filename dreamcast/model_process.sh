#!/usr/bin/env bash

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

if [[ $# -ne 2 ]]; then
  die "pass in source_dir and stage_dir only"
fi

source_mesh_dir=$1/Meshes
staged_mesh_dir=$2/Meshes

if [[ -d "$staged_mesh_dir" ]]; then
  rm -r "$staged_mesh_dir"
fi

pushd ../dependencies/dreamcast/mania-mesh-optimizer

mkdir "cmake-build-release" || true
cmake -DCMAKE_BUILD_TYPE=Release -S . -B "./cmake-build-release"
cmake --build "./cmake-build-release"

optimizer=$PWD/cmake-build-release/mania-mesh-optimizer

popd

# note that --optimize is on by default. it can be disabled using --optimize false

# just removing any duplicate & unused vertices
"$optimizer" -i "$source_mesh_dir/Continue/Count0.bin" -o "$staged_mesh_dir/Continue/Count0.bin"
"$optimizer" -i "$source_mesh_dir/Continue/Count1.bin" -o "$staged_mesh_dir/Continue/Count1.bin"
"$optimizer" -i "$source_mesh_dir/Continue/Count2.bin" -o "$staged_mesh_dir/Continue/Count2.bin"
"$optimizer" -i "$source_mesh_dir/Continue/Count3.bin" -o "$staged_mesh_dir/Continue/Count3.bin"
"$optimizer" -i "$source_mesh_dir/Continue/Count4.bin" -o "$staged_mesh_dir/Continue/Count4.bin"
"$optimizer" -i "$source_mesh_dir/Continue/Count5.bin" -o "$staged_mesh_dir/Continue/Count5.bin"
"$optimizer" -i "$source_mesh_dir/Continue/Count6.bin" -o "$staged_mesh_dir/Continue/Count6.bin"
"$optimizer" -i "$source_mesh_dir/Continue/Count7.bin" -o "$staged_mesh_dir/Continue/Count7.bin"
"$optimizer" -i "$source_mesh_dir/Continue/Count8.bin" -o "$staged_mesh_dir/Continue/Count8.bin"
"$optimizer" -i "$source_mesh_dir/Continue/Count9.bin" -o "$staged_mesh_dir/Continue/Count9.bin"

"$optimizer" -i "$source_mesh_dir/Decoration/Bird.bin" -o "$staged_mesh_dir/Decoration/Bird.bin" --simplify --stripify --bake-lighting --simplify-index-threshold 0.015 --simplify-target-error 0.0225
"$optimizer" -i "$source_mesh_dir/Decoration/Fish.bin" -o "$staged_mesh_dir/Decoration/Fish.bin" --simplify --stripify --bake-lighting --simplify-index-threshold 0.015 --simplify-target-error 0.0225
"$optimizer" -i "$source_mesh_dir/Decoration/Flower1.bin" -o "$staged_mesh_dir/Decoration/Flower1.bin" --simplify --stripify --bake-lighting --simplify-index-threshold 0.005 --simplify-target-error 0.1
"$optimizer" -i "$source_mesh_dir/Decoration/Flower2.bin" -o "$staged_mesh_dir/Decoration/Flower2.bin" --simplify --stripify --bake-lighting --simplify-index-threshold 0.005 --simplify-target-error 0.1
"$optimizer" -i "$source_mesh_dir/Decoration/Flower3.bin" -o "$staged_mesh_dir/Decoration/Flower3.bin" --simplify --stripify --bake-lighting --simplify-index-threshold 0.005 --simplify-target-error 0.1
"$optimizer" -i "$source_mesh_dir/Decoration/Pillar1.bin" -o "$staged_mesh_dir/Decoration/Pillar1.bin" --simplify --stripify --bake-lighting --simplify-index-threshold 0.005 --simplify-target-error 0.1
"$optimizer" -i "$source_mesh_dir/Decoration/Pillar2.bin" -o "$staged_mesh_dir/Decoration/Pillar2.bin" --simplify --stripify --bake-lighting --simplify-index-threshold 0.005 --simplify-target-error 0.1
"$optimizer" -i "$source_mesh_dir/Decoration/Tree.bin" -o "$staged_mesh_dir/Decoration/Tree.bin" --simplify --stripify --bake-lighting --simplify-index-threshold 0.005 --simplify-target-error 0.1

# just removing any duplicate & unused vertices
"$optimizer" -i "$source_mesh_dir/GHZ/DDWBall.bin" -o "$staged_mesh_dir/GHZ/DDWBall.bin"
"$optimizer" -i "$source_mesh_dir/GHZ/DDWEyes.bin" -o "$staged_mesh_dir/GHZ/DDWEyes.bin"
"$optimizer" -i "$source_mesh_dir/GHZ/DDWStache.bin" -o "$staged_mesh_dir/GHZ/DDWStache.bin"
"$optimizer" -i "$source_mesh_dir/GHZ/DDWStripe1.bin" -o "$staged_mesh_dir/GHZ/DDWStripe1.bin"
"$optimizer" -i "$source_mesh_dir/GHZ/DDWStripe2.bin" -o "$staged_mesh_dir/GHZ/DDWStripe2.bin"

# unused
#"$optimizer" -i "$source_mesh_dir/Global/Sonic.bin" -o "$staged_mesh_dir/Global/Sonic.bin"

# just removing any duplicate & unused vertices
"$optimizer" -i "$source_mesh_dir/Global/SpecialRing.bin" -o "$staged_mesh_dir/Global/SpecialRing.bin"
"$optimizer" -i "$source_mesh_dir/Pinball/Bumper.bin" -o "$staged_mesh_dir/Pinball/Bumper.bin"
"$optimizer" -i "$source_mesh_dir/Pinball/ChuteHoop.bin" -o "$staged_mesh_dir/Pinball/ChuteHoop.bin"
"$optimizer" -i "$source_mesh_dir/Pinball/Flipper.bin" -o "$staged_mesh_dir/Pinball/Flipper.bin"
"$optimizer" -i "$source_mesh_dir/Pinball/TargetBumper.bin" -o "$staged_mesh_dir/Pinball/TargetBumper.bin"

"$optimizer" -i "$source_mesh_dir/Special/EmeraldBlue.bin" -o "$staged_mesh_dir/Special/EmeraldBlue.bin" --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/EmeraldCyan.bin" -o "$staged_mesh_dir/Special/EmeraldCyan.bin" --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/EmeraldGreen.bin" -o "$staged_mesh_dir/Special/EmeraldGreen.bin" --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/EmeraldGrey.bin" -o "$staged_mesh_dir/Special/EmeraldGrey.bin" --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/EmeraldPurple.bin" -o "$staged_mesh_dir/Special/EmeraldPurple.bin" --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/EmeraldRed.bin" -o "$staged_mesh_dir/Special/EmeraldRed.bin" --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/EmeraldYellow.bin" -o "$staged_mesh_dir/Special/EmeraldYellow.bin" --stripify --bake-lighting
# TODO: ItemBox special case (changes normal during light baking)
"$optimizer" -i "$source_mesh_dir/Special/ItemBox.bin" -o "$staged_mesh_dir/Special/ItemBox.bin" --simplify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/KnuxBall.bin" -o "$staged_mesh_dir/Special/KnuxBall.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/KnuxDash.bin" -o "$staged_mesh_dir/Special/KnuxDash.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/KnuxJog.bin" -o "$staged_mesh_dir/Special/KnuxJog.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/KnuxJump.bin" -o "$staged_mesh_dir/Special/KnuxJump.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/KnuxTumble.bin" -o "$staged_mesh_dir/Special/KnuxTumble.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/MightyBall.bin" -o "$staged_mesh_dir/Special/MightyBall.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/MightyDash.bin" -o "$staged_mesh_dir/Special/MightyDash.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/MightyJog.bin" -o "$staged_mesh_dir/Special/MightyJog.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/MightyJump.bin" -o "$staged_mesh_dir/Special/MightyJump.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/MightyTumble.bin" -o "$staged_mesh_dir/Special/MightyTumble.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/RayBall.bin" -o "$staged_mesh_dir/Special/RayBall.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/RayDash.bin" -o "$staged_mesh_dir/Special/RayDash.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/RayJog.bin" -o "$staged_mesh_dir/Special/RayJog.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/RayJump.bin" -o "$staged_mesh_dir/Special/RayJump.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/RayTumble.bin" -o "$staged_mesh_dir/Special/RayTumble.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/Shadow.bin" -o "$staged_mesh_dir/Special/Shadow.bin"
"$optimizer" -i "$source_mesh_dir/Special/SonicBall.bin" -o "$staged_mesh_dir/Special/SonicBall.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/SonicDash.bin" -o "$staged_mesh_dir/Special/SonicDash.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/SonicJog.bin" -o "$staged_mesh_dir/Special/SonicJog.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/SonicJump.bin" -o "$staged_mesh_dir/Special/SonicJump.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/SonicTumble.bin" -o "$staged_mesh_dir/Special/SonicTumble.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/Springboard.bin" -o "$staged_mesh_dir/Special/Springboard.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/TailsBall.bin" -o "$staged_mesh_dir/Special/TailsBall.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/TailsDash.bin" -o "$staged_mesh_dir/Special/TailsDash.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/TailsJog.bin" -o "$staged_mesh_dir/Special/TailsJog.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/TailsJump.bin" -o "$staged_mesh_dir/Special/TailsJump.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/TailsTumble.bin" -o "$staged_mesh_dir/Special/TailsTumble.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/Special/UFOChase.bin" -o "$staged_mesh_dir/Special/UFOChase.bin" --simplify --stripify --bake-lighting

"$optimizer" -i "$source_mesh_dir/SSZ/EggTower.bin" -o "$staged_mesh_dir/SSZ/EggTower.bin" --simplify --simplify-index-threshold 0.05 --simplify-target-error 0.1
# turning this into strips blows up the entire game, guaranteed crash
"$optimizer" -i "$source_mesh_dir/SSZ/MonarchPlans.bin" -o "$staged_mesh_dir/SSZ/MonarchPlans.bin" --simplify --simplify-index-threshold 0.05 --simplify-target-error 0.1
# --bake-lighting

# unused
#"$optimizer" -i "$source_mesh_dir/TMZ/MonarchBG.bin" -o "$staged_mesh_dir/TMZ/MonarchBG.bin" --simplify --stripify --bake-lighting
"$optimizer" -i "$source_mesh_dir/TMZ/OrbNet.bin" -o "$staged_mesh_dir/TMZ/OrbNet.bin"
