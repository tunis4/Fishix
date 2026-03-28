#!/usr/bin/env bash

# ===============================
# Recursive Cat Script
# Usage:
#   ./cat.sh output.file [folder]
# ===============================

# cek argumen
if [ -z "$1" ]; then
    echo "Usage: $0 output.file [folder]"
    exit 1
fi

OUTPUT="$1"
TARGET="${2:-.}"   # default folder = current dir

# kosongkan file output
> "$OUTPUT"

# fungsi rekursif
process_dir() {
    local dir="$1"

    for item in "$dir"/*; do
        # skip jika tidak ada file
        [ -e "$item" ] || continue

        if [ -d "$item" ]; then
            # rekursif masuk folder
            process_dir "$item"

        elif [ -f "$item" ]; then
            # tulis header
            echo "=== ${item#./} ===" >> "$OUTPUT"

            # isi file
            cat "$item" >> "$OUTPUT"

            # newline pemisah
            echo -e "\n" >> "$OUTPUT"
        fi
    done
}

process_dir "$TARGET"

echo "Selesai -> $OUTPUT"