#!/bin/bash

BASE_URL="http://tablebase.sesse.net/syzygy/3-4-5/"
DEST_DIR="$(dirname "$(realpath "$0")")"

echo "Downloading Syzygy 3-4-5 tablebases to: $DEST_DIR"
echo "Press Ctrl+C to pause — re-run to resume."

# Fetch the file list directly and download each file with -c (resume)
wget -q -O - "$BASE_URL" \
    | grep -o 'href="[^"]*\.rtb[wz]"' \
    | sed 's/href="//;s/"//' \
    | while read -r file; do
        wget \
            --continue \
            --tries=10 \
            --wait=1 \
            --timeout=60 \
            --progress=bar:force \
            --directory-prefix="$DEST_DIR" \
            "${BASE_URL}${file}"
    done

echo "Done."
