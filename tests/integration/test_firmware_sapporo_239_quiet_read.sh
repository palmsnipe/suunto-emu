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
log_hash=fb515ff6b15a773c57ee75d9fc6d0bd5eb65b2c558b3f71585f888d78b484e68
snapshot_hash=fec90410d4d5c5f1c76208669fd021f173af63ddc4f3ccc1cc554765e6400a8a
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
    'stop=budget pc=0x00093b5a instructions=607105617 virtual_time_ns=2453829522' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|status=refuse' \
    "$run_dir/first.log" ||
   [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 76286 ]; then
    echo "error: quiet-read boundary or compatibility count changed" >&2
    exit 1
fi

# The original ticket-749 prefix stays exact, not its superseded next refusal.
run prefix 459796107
if [ "$(hash "$run_dir/prefix.log")" != 98343f94333ab137a96ccac9694c48d0795e3df5a2b36c3f9f63558100ee1666 ] ||
   [ "$(hash "$run_dir/prefix.sems")" != e7aeb2971567ebcacd7fd7aaabad07b59813aaaa5a19009306972ed1ea39b10d ]; then
    echo "error: historical checkpoint changed" >&2
    exit 1
fi
run resumed 607105617 --snapshot-load "$run_dir/prefix.sems"
if ! cmp -s "$run_dir/first.sems" "$run_dir/resumed.sems"; then
    echo "error: resumed quiet-read checkpoint differs" >&2
    exit 1
fi

# RE-SCOPED under E-SAP239-DEEPCLEAN-001 (ticket 777): the old
# pc=0x001c0db4 precise-fault stop was the pre-capacity-law timer-store
# path; with the storage wall gone the next instruction executes and
# stops at budget at pc=0x0009369c. The fault path itself is unchanged
# in the engine; only this pinned continuation moved.
run fault 607105618 --snapshot-load "$run_dir/first.sems"
if ! grep -F -x -q \
    'stop=budget pc=0x0009369c instructions=607105618 virtual_time_ns=2453829523' \
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
