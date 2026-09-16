#!/bin/sh
# TEST_TAGS: sapporo_239_wbsto_cache
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 WbStorage cache runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
expected_flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
# E-ULS-0047 (ticket 777) re-derivation from two byte-identical runs. The
# former advanced checkpoint pinned a run-ending unmapped write to 0x0f676e34
# (E-SAP-COMPAT-WBSTO-239-001). Ticket 729's logical writable-files layer
# (E-SAP-COMPAT-FILES-239-001, this gate's dependency) removed that FAT
# underflow as accepted behavior, so the cache-layer era now runs fault-free
# to the full 500,000,000-instruction budget cap; that cap stop line is the
# re-derived advanced checkpoint and the underflow is now asserted absent.
expected_log_hash=6682af6ef5457a2155866229c29e25dda161cbb787e4c140619d79358d379d2d

if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 WbStorage cache runner: set SEMU_SAPPORO_239_FULL_FLASH"
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

run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-wbsto.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

run_once()
{
    output=$1
    expected_status=$2
    budget=$3
    shift 3
    if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --until normal-frame \
        --max-instructions "$budget" --max-time 30000000000 "$@" \
        >"$output" 2>&1;
    then
        run_status=0
    else
        run_status=$?
    fi
    if [ "$run_status" -ne "$expected_status" ]; then
        echo "error: Sapporo 2.39 WbStorage run returned $run_status" >&2
        cat "$output" >&2
        exit 1
    fi
}

run_once "$run_dir/layer-off.log" 3 72774982
if ! grep -F -x -q \
    'stop=budget pc=0x00079e1e instructions=72774982 virtual_time_ns=521257564' \
    "$run_dir/layer-off.log";
then
    echo "error: Sapporo 2.39 layer-off checkpoint changed" >&2
    cat "$run_dir/layer-off.log" >&2
    exit 1
fi

run_once "$run_dir/first.log" 3 500000000 --layer sapporo-2.39-synthetic-wbsto
run_once "$run_dir/second.log" 3 500000000 --layer sapporo-2.39-synthetic-wbsto
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log"; then
    echo "error: Sapporo 2.39 WbStorage cache runs differ" >&2
    diff -u "$run_dir/first.log" "$run_dir/second.log" >&2 || true
    exit 1
fi
if [ "$(shasum -a 256 "$run_dir/first.log" | awk '{print $1}')" != \
     "$expected_log_hash" ]; then
    echo "error: Sapporo 2.39 WbStorage cache log hash changed" >&2
    cat "$run_dir/first.log" >&2
    exit 1
fi
for trigger in wbsto-session-cache wbsto-preload-result; do
    if [ "$(grep -c "trigger=$trigger ordinal=1" "$run_dir/first.log")" -ne 1 ]; then
        echo "error: Sapporo 2.39 intervention $trigger count changed" >&2
        cat "$run_dir/first.log" >&2
        exit 1
    fi
done
if grep -F -q 'event=machine-reset-request' "$run_dir/first.log"; then
    echo "error: Sapporo 2.39 WbStorage cache run reset" >&2
    cat "$run_dir/first.log" >&2
    exit 1
fi
if ! grep -F -x -q \
    'stop=budget pc=0x00070f3e instructions=500000000 virtual_time_ns=2038956542' \
    "$run_dir/first.log" ||
   grep -F -q '0x0f676e34' "$run_dir/first.log";
then
    echo "error: Sapporo 2.39 WbStorage advanced checkpoint changed" >&2
    cat "$run_dir/first.log" >&2
    exit 1
fi
if [ "$(shasum -a 256 "$full_flash" | awk '{print $1}')" != \
     "$expected_flash_hash" ]; then
    echo "error: Sapporo 2.39 source flash was modified" >&2
    exit 1
fi

echo "PASS sapporo-2.39.20 synthetic WbStorage session-cache checkpoint"
