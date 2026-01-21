#!/bin/sh

# Fail on error
set -e

# Check KOS_BASE
if [ -z "$KOS_BASE" ]; then
    echo "Error: KOS_BASE is not set"
    exit 1
fi

PVRTXEX="$KOS_BASE/utils/pvrtex/pvrtex"

# Check pvrtex exists
if [ ! -x "$PVRTXEX" ]; then
    echo "Error: pvrtex not found or not executable at:"
    echo "  $PVRTXEX"
    exit 1
fi

# Loop over PNG files
for png in *.png; do
    # Handle case where no PNGs exist
    [ -e "$png" ] || continue

    base="${png%.png}"

    echo "Converting: $png"

    "$PVRTXEX" \
        -i "$png" \
        -o "$base.tex" \
        -f ARGB1555 \
        --compress "small"

    mv "$base.tex" "$base.gif"
done

