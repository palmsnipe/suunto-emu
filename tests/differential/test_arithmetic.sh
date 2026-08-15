#!/bin/sh
# Differential test runner: CPU arithmetic vectors (ticket 630).
# # TEST_TAGS: arithmetic
#
# Runs synthetic CPU arithmetic vectors through the differential adapter.
# Skips gracefully when RENODE is not provided.

set -eu

renode=${RENODE:-}
emulator=${SEMU_EMULATOR:-./build/suunto-emu}
adapter="$(dirname "$0")/../../tools/differential/run_vector.sh"

if [ -z "$renode" ]; then
    echo "SKIP test_arithmetic: set RENODE to a Renode executable"
    exit 0
fi

if [ ! -x "$adapter" ]; then
    echo "error: adapter not found: $adapter" >&2
    exit 2
fi

# Vector list: synthetic programs to compare.
# Each entry is a path relative to the repo root.
vectors="
"

matched=0
for vector in $vectors; do
    matched=$((matched + 1))
    RENODE="$renode" SEMU_EMULATOR="$emulator" sh "$adapter" "$vector" || {
        echo "FAIL: differential mismatch for $vector"
        exit 1
    }
done

if [ "$matched" -eq 0 ]; then
    echo "SKIP test_arithmetic: no synthetic vectors available yet"
    exit 0
fi

echo "PASS test_arithmetic: $matched vectors compared"
