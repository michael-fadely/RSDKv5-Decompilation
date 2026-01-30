#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

shopt -s nullglob

for inpath in *.mpg *.MPG; do
  [[ -f "$inpath" ]] || continue

  outpath="${inpath%.*}.ogv"

  echo "Renaming: $inpath -> $outpath"
  mv -- "$inpath" "$outpath"
done

