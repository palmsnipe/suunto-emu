#!/bin/sh
# TEST_TAGS: sapporo_239_ctimer13_inten
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 Timer13 INTEN: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
log_hash=2223de22981528b7cd2df049be68ea2e4022627763da13aab9293ef1fbbf7e16
snapshot_hash=74e45df965216d809cf41e090ee0fc56b740affbc6d32aec35413dd65db1aa0c
if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 Timer13 INTEN: set SEMU_SAPPORO_239_FULL_FLASH"
    exit 0
fi
hash()
{
    shasum -a 256 "$1" | awk '{print $1}'
}
if [ ! -r "$full_flash" ] || [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: Sapporo 2.39 full flash unavailable or hash mismatch" >&2
    exit 2
fi
"$emulator" validate --profile sapporo-2.39.20 --firmware "$manifest"
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-timer13.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

run()
{
    name=$1
    limit=$2
    expected_code=$3
    shift 3
    code=0
    "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --until normal-frame --max-instructions "$limit" \
        --max-time 30000000000 --snapshot-save "$run_dir/$name.sems" \
        "$@" >"$run_dir/$name.log" 2>&1 || code=$?
    if [ "$code" -ne "$expected_code" ]; then
        echo "error: Timer13 checkpoint returned $code" >&2
        cat "$run_dir/$name.log" >&2
        exit 1
    fi
}
run first 608140266 3
run second 608140266 3
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log" ||
   ! cmp -s "$run_dir/first.sems" "$run_dir/second.sems" ||
   [ "$(hash "$run_dir/first.log")" != "$log_hash" ] ||
   [ "$(hash "$run_dir/first.sems")" != "$snapshot_hash" ]; then
    echo "error: Timer13 checkpoint artifacts differ" >&2
    exit 1
fi
if ! grep -F -x -q \
    'stop=budget pc=0x00079e1c instructions=608140266 virtual_time_ns=2147096849' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|status=refuse' \
    "$run_dir/first.log" ||
   [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 76258 ]; then
    echo "error: Timer13 boundary or compatibility count changed" >&2
    exit 1
fi

# Preserve ticket 751 before its now-superseded exact INTEN refusal.
run prefix 607105617 3
if [ "$(hash "$run_dir/prefix.log")" != 740750cbc6460fe8b9c3b420a5509d992dc0757e50de9102da316df7d21be1ec ] ||
   [ "$(hash "$run_dir/prefix.sems")" != 15b5f2076d7107e50b693333d1d19bcd3bd18014b8208c33f02ed8e45676dd19 ]; then
    echo "error: historical checkpoint changed" >&2
    exit 1
fi
run resumed 608140266 3 --snapshot-load "$run_dir/prefix.sems"
if ! cmp -s "$run_dir/first.sems" "$run_dir/resumed.sems"; then
    echo "error: resumed Timer13 checkpoint differs" >&2
    exit 1
fi

# Execute the real firmware BKPT, without a fatal-status translation.
run halt 608140267 0 --snapshot-load "$run_dir/first.sems"
if ! grep -F -x -q \
    'stop=halt pc=0x00079e1e instructions=608140267 virtual_time_ns=2147096850' \
    "$run_dir/halt.log"; then
    echo "error: next firmware breakpoint changed" >&2
    cat "$run_dir/halt.log" >&2
    exit 1
fi
if [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: source flash was modified" >&2
    exit 1
fi
echo "PASS sapporo-2.39.20 combined Timer13 INTEN, old prefix, resume and native halt"
