#!/bin/sh
set -eu

warning=300
split_review=500

find include src tests -type f \( -name '*.c' -o -name '*.h' \) | sort |
while IFS= read -r path; do
    lines=$(wc -l <"$path" | tr -d ' ')
    if [ "$lines" -gt "$split_review" ]; then
        echo "warning: $path has $lines lines; consider splitting by responsibility (advisory $split_review)" >&2
    elif [ "$lines" -gt "$warning" ]; then
        echo "warning: $path has $lines lines; review threshold is $warning" >&2
    fi
done

# File length is advisory; correctness and readability remain review gates.
