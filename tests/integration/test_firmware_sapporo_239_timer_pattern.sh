#!/bin/sh
# TEST_TAGS: sapporo_239_timer_pattern
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 CTIMER pattern runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-timer-pattern.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

run_once()
{
    output=$1
    if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --max-instructions 200000000 --max-time 2000000000 >"$output" 2>&1;
    then
        run_status=0
    else
        run_status=$?
    fi
    if [ "$run_status" -ne 3 ]; then
        echo "error: Sapporo 2.39 run returned $run_status instead of 3" >&2
        cat "$output" >&2
        exit 1
    fi
}

run_once "$run_dir/first.log"
run_once "$run_dir/second.log"
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log"; then
    echo "error: Sapporo 2.39 CTIMER pattern runs differ" >&2
    diff -u "$run_dir/first.log" "$run_dir/second.log" >&2 || true
    exit 1
fi
if grep -q 'event=machine-reset-request' "$run_dir/first.log"; then
    echo "error: Sapporo 2.39 CTIMER pattern run reset" >&2
    cat "$run_dir/first.log" >&2
    exit 1
fi
if ! grep -F -x -q \
    'stop=budget pc=0x000e955a instructions=84856056 virtual_time_ns=6372873731' \
    "$run_dir/first.log"; then
    echo "error: Sapporo 2.39 CTIMER pattern idle checkpoint changed" >&2
    tail -n 4 "$run_dir/first.log" >&2
    exit 1
fi

echo "PASS sapporo-2.39.20 CTIMER pattern idle checkpoint"
