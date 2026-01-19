#!/usr/bin/env sh

START_DIR=${1:-.}

find "$START_DIR" -type f -iname "*.wav" -print0 |
while IFS= read -r -d '' file; do
    dir=$(dirname "$file")
    base=$(basename "$file")

    case "$base" in
        adpcm_fast_*)
            newbase=${base#adpcm_fast_}
            ;;
        *)
            rm "$file"
            continue
            ;;
    esac

    dest="$dir/$newbase"

    # If destination exists, add _1, _2, ... before extension
    if [ -e "$dest" ]; then
        name=${newbase%.*}
        ext=${newbase##*.}
        i=1
        while :; do
            if [ "$ext" = "$newbase" ]; then
                # No extension
                dest="$dir/${name}_$i"
            else
                dest="$dir/${name}_$i.$ext"
            fi
            [ -e "$dest" ] || break
            i=$((i + 1))
        done
    fi

#    echo "Renaming:"
#    echo "  $file"
#    echo "  -> $dest"
    mv "$file" "$dest"
done

#echo "Done."

