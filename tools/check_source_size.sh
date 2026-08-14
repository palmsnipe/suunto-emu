#!/bin/sh
set -eu

warning=300
maximum=500
failed=0

find include src tests -type f \( -name '*.c' -o -name '*.h' \) | sort |
while IFS= read -r path; do
    lines=$(wc -l <"$path" | tr -d ' ')
    if [ "$lines" -gt "$maximum" ]; then
        echo "error: $path has $lines lines; maximum is $maximum" >&2
        failed=1
    elif [ "$lines" -gt "$warning" ]; then
        echo "warning: $path has $lines lines; review threshold is $warning" >&2
    fi
done

# The pipeline loop runs in a subshell on POSIX shells, so make the hard-limit
# decision in a second expression whose status is directly observable.
oversized=$(find include src tests -type f \( -name '*.c' -o -name '*.h' \) \
    -exec wc -l {} \; | awk -v maximum="$maximum" '$1 > maximum { count++ } END { print count + 0 }')
[ "$oversized" -eq 0 ]

