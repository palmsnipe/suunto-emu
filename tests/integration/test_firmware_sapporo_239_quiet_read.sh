#!/bin/sh
# TEST_TAGS: sapporo_239_quiet_read
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 quiet read: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
log_hash=740750cbc6460fe8b9c3b420a5509d992dc0757e50de9102da316df7d21be1ec
snapshot_hash=15b5f2076d7107e50b693333d1d19bcd3bd18014b8208c33f02ed8e45676dd19
if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 quiet read: set SEMU_SAPPORO_239_FULL_FLASH"
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
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-quiet-read.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

run()
{
    name=$1
    limit=$2
    shift 2
    code=0
    "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --until normal-frame --max-instructions "$limit" \
        --max-time 30000000000 --snapshot-save "$run_dir/$name.sems" \
        "$@" >"$run_dir/$name.log" 2>&1 || code=$?
    if [ "$code" -ne 3 ]; then
        echo "error: quiet-read checkpoint returned $code" >&2
        cat "$run_dir/$name.log" >&2
        exit 1
    fi
}
run first 607105617
run second 607105617
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log" ||
   ! cmp -s "$run_dir/first.sems" "$run_dir/second.sems" ||
   [ "$(hash "$run_dir/first.log")" != "$log_hash" ] ||
   [ "$(hash "$run_dir/first.sems")" != "$snapshot_hash" ]; then
    echo "error: quiet-read checkpoint artifacts differ" >&2
    exit 1
fi
if ! grep -F -x -q \
    'stop=budget pc=0x000cb852 instructions=607105617 virtual_time_ns=2146062159' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|status=refuse' \
    "$run_dir/first.log" ||
   [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 76258 ]; then
    echo "error: quiet-read boundary or compatibility count changed" >&2
    exit 1
fi

# The original ticket-749 prefix stays exact, not its superseded next refusal.
run prefix 459796107
if [ "$(hash "$run_dir/prefix.log")" != ea04ac89a277fc58cc1c653e59e595f2a40f25d7202afd16b5adc309e6bf2732 ] ||
   [ "$(hash "$run_dir/prefix.sems")" != 13e104c98a6fdf5a741a15615bf1b77ea53ebe05db39e7bf224cf0818560b5ee ]; then
    echo "error: historical checkpoint changed" >&2
    exit 1
fi
run resumed 607105617 --snapshot-load "$run_dir/prefix.sems"
if ! cmp -s "$run_dir/first.sems" "$run_dir/resumed.sems"; then
    echo "error: resumed quiet-read checkpoint differs" >&2
    exit 1
fi

# The next real store still takes the precise fault vector; no timer bypass.
run fault 607105618 --snapshot-load "$run_dir/first.sems"
if ! grep -F -x -q \
    'stop=budget pc=0x001c0db4 instructions=607105618 virtual_time_ns=2146062160' \
    "$run_dir/fault.log"; then
    echo "error: next precise fault changed" >&2
    cat "$run_dir/fault.log" >&2
    exit 1
fi
if [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: source flash was modified" >&2
    exit 1
fi
echo "PASS sapporo-2.39.20 native quiet reads, old prefix, resume and precise fault"
