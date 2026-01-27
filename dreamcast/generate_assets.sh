#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

die() { printf 'error: %s\n' "$*" >&2; exit 1; }
usage() {
  printf 'Usage: %s <source_dir> <stage_dir>\n' "${0##*/}" >&2
  exit 2
}

abspath_dir() {
  # Prints an absolute path for an existing directory.
  # Uses physical path (-P) to resolve symlinks in the path.
  local d=$1
  [[ -d "$d" ]] || return 1
  (cd -P -- "$d" && pwd -P)
}

[[ $# -eq 2 ]] || usage

src_in=$1
stage_in=$2

# Resolve source (must exist)
sourcedir=$(abspath_dir "$src_in") || die "source directory not found: $src_in"

# Create stage dir (can be new)
mkdir -p -- "$stage_in"
stagedir=$(abspath_dir "$stage_in") || die "stage directory not accessible: $stage_in"

# Guard against obvious mistakes
if [[ "$sourcedir" == "$stagedir" ]]; then
  die "source and stage are the same directory: $sourcedir"
fi

# If stage is inside source, copying can explode / recurse depending on layout
case "$stagedir/" in
  "$sourcedir/"*) die "stage directory is inside source directory (refusing): $stagedir" ;;
esac

# Locate helper scripts relative to *this* script, not current working directory
script_dir=$(
  cd -P -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P
)

# Verify expected source subdirs exist
for d in "Music" "SoundFX" "Sprites/Global" "Sprites/TMZ1" "Sprites/UI"; do
  [[ -d "$sourcedir/$d" ]] || die "missing expected directory: $sourcedir/$d"
done

cp -R -- "$sourcedir/Music"   "$stagedir/"
cp -R -- "$sourcedir/SoundFX" "$stagedir/"

mkdir -p -- "$stagedir/Sprites"
cp -R -- "$sourcedir/Sprites/Global" "$stagedir/Sprites/"
cp -R -- "$sourcedir/Sprites/TMZ1"   "$stagedir/Sprites/"
cp -R -- "$sourcedir/Sprites/UI"     "$stagedir/Sprites/"

"$script_dir/sfx_step_1_downsample.sh"  "$stagedir"
"$script_dir/sfx_step_2_wav2adpcm.sh"   "$stagedir"
"$script_dir/sfx_step_3_cleanup.sh"     "$stagedir"
"$script_dir/sfx_step_4_move.sh"        "$stagedir"

"$script_dir/music_step_1_ogg_to_pcm.sh"    "$stagedir"
"$script_dir/music_step_2_rename_to_ogg.sh" "$stagedir"

"$script_dir/gfx_step_1_toalphapng.sh"  "$stagedir"
"$script_dir/gfx_step_2_todtex.sh"      "$stagedir"

printf '%s\n' '-- DONE --'

