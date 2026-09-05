#!/bin/sh
# TEST_TAGS: sapporo_239_history_budget
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 history budget: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
log_hash=6f47fad1eeb3b6032955b463e2c4ba26310dbf5ddc453ae3f0f350acf15a9348
snapshot_hash=3c56bfb5f3f7b541433ca05a3de999c941df3151484a5e080ad09a89b3672ae1
if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 history budget: set SEMU_SAPPORO_239_FULL_FLASH"
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
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-history.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

run()
{
    name=$1
    limit=$2
    shift 2
    code=0
    if [ "$name" != refusal ]; then
        set -- --snapshot-save "$run_dir/$name.sems" "$@"
    fi
    "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --until normal-frame --max-instructions "$limit" \
        --max-time 30000000000 \
        "$@" >"$run_dir/$name.log" 2>&1 || code=$?
    if [ "$code" -ne 3 ]; then
        echo "error: history checkpoint returned $code" >&2
        cat "$run_dir/$name.log" >&2
        exit 1
    fi
}
run first 439081594
run second 439081594
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log" ||
   ! cmp -s "$run_dir/first.sems" "$run_dir/second.sems" ||
   [ "$(hash "$run_dir/first.log")" != "$log_hash" ] ||
   [ "$(hash "$run_dir/first.sems")" != "$snapshot_hash" ]; then
    echo "error: history checkpoint artifacts differ" >&2
    exit 1
fi
if ! grep -F -x -q \
    'stop=budget pc=0x000920b4 instructions=439081594 virtual_time_ns=1978038136' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|status=refuse' \
    "$run_dir/first.log" ||
   [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 75764 ] ||
   [ "$(grep -c 'operation=read path=sleepln/sleep.bin result=72 ' "$run_dir/first.log")" -ne 35712 ] ||
   [ "$(grep -c 'operation=seek path=sleepln/sleep.bin ' "$run_dir/first.log")" -ne 35714 ]; then
    echo "error: native history scans or boundary changed" >&2
    exit 1
fi

# Total sleep seeks include two earlier header rewinds plus 35,712 record seeks.

# Preserve the actual ticket-746 prefix, not its superseded next-step refusal.
run prefix 435333559
if [ "$(hash "$run_dir/prefix.log")" != 476cf8603492ff3cfc456a61babd1c0cc7c5347b4bc81a7e8a39594e595f9b71 ] ||
   [ "$(hash "$run_dir/prefix.sems")" != 27b6e51ee66569d9e68ef56c7608c280cfd4e48f6cfe5d4beb1aaaf68f4fa79f ]; then
    echo "error: historical pre-refusal checkpoint changed" >&2
    exit 1
fi
run resumed 439081594 --snapshot-load "$run_dir/prefix.sems"
if ! cmp -s "$run_dir/first.sems" "$run_dir/resumed.sems"; then
    echo "error: resumed history checkpoint differs" >&2
    exit 1
fi

# Unknown create refuses before any instruction, file, or guest-memory mutation.
run refusal 439081595 --snapshot-load "$run_dir/first.sems"
if ! grep -F -x -q \
    'stop=compat-refused pc=0x000920b4 instructions=439081594 virtual_time_ns=1978038136 detail=unknown Sapporo 2.39 writable file path' \
    "$run_dir/refusal.log"; then
    echo "error: unknown writable-path refusal changed" >&2
    cat "$run_dir/refusal.log" >&2
    exit 1
fi
if [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: source flash was modified" >&2
    exit 1
fi
echo "PASS sapporo-2.39.20 native history scans, old prefix, resume and unknown-path refusal"
