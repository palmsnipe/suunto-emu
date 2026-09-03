#!/bin/sh
# TEST_TAGS: sapporo_239_ctimer_combined_inten
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 combined CTIMER INTEN runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
expected_flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb

if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 combined CTIMER INTEN runner: set SEMU_SAPPORO_239_FULL_FLASH"
    exit 0
fi
if [ ! -f "$full_flash" ] || [ ! -r "$full_flash" ]; then
    echo "error: Sapporo 2.39 full flash is not readable: $full_flash" >&2
    exit 2
fi
flash_hash=$(shasum -a 256 "$full_flash" | awk '{print $1}')
if [ "$flash_hash" != "$expected_flash_hash" ]; then
    echo "error: Sapporo 2.39 full flash hash mismatch" >&2
    echo "expected: $expected_flash_hash" >&2
    echo "actual:   $flash_hash" >&2
    exit 2
fi

run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-combined-inten.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

run_once()
{
    output=$1
    if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --until normal-frame \
        --max-instructions 500000000 --max-time 30000000000 \
        >"$output" 2>&1;
    then
        run_status=0
    else
        run_status=$?
    fi
    if [ "$run_status" -ne 0 ]; then
        echo "error: Sapporo 2.39 combined CTIMER INTEN run returned $run_status" >&2
        cat "$output" >&2
        exit 1
    fi
}

run_once "$run_dir/first.log"
run_once "$run_dir/second.log"
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log"; then
    echo "error: Sapporo 2.39 combined CTIMER INTEN runs differ" >&2
    diff -u "$run_dir/first.log" "$run_dir/second.log" >&2 || true
    exit 1
fi
if grep -F -q 'event=machine-reset-request' "$run_dir/first.log"; then
    echo "error: Sapporo 2.39 combined CTIMER INTEN reset returned" >&2
    cat "$run_dir/first.log" >&2
    exit 1
fi
if ! grep -F -x -q \
    'stop=halt pc=0x00079e1e instructions=72774982 virtual_time_ns=521257564' \
    "$run_dir/first.log";
then
    echo "error: Sapporo 2.39 combined CTIMER INTEN checkpoint changed" >&2
    cat "$run_dir/first.log" >&2
    exit 1
fi
if [ "$(shasum -a 256 "$full_flash" | awk '{print $1}')" != \
     "$expected_flash_hash" ]; then
    echo "error: Sapporo 2.39 source flash was modified" >&2
    exit 1
fi

echo "PASS sapporo-2.39.20 combined CTIMER INTEN checkpoint"
