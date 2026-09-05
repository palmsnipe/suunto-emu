#!/bin/sh
# TEST_TAGS: sapporo_239_zip_read
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 native ZIP read: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
log_hash=ea04ac89a277fc58cc1c653e59e595f2a40f25d7202afd16b5adc309e6bf2732
snapshot_hash=13e104c98a6fdf5a741a15615bf1b77ea53ebe05db39e7bf224cf0818560b5ee
if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 native ZIP read: set SEMU_SAPPORO_239_FULL_FLASH"
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
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-zip-read.XXXXXX")
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
        --max-time 30000000000 "$@" >"$run_dir/$name.log" 2>&1 || code=$?
    if [ "$code" -ne 3 ]; then
        echo "error: native ZIP checkpoint returned $code" >&2
        cat "$run_dir/$name.log" >&2
        exit 1
    fi
}
run first 459796107
run second 459796107
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log" ||
   ! cmp -s "$run_dir/first.sems" "$run_dir/second.sems" ||
   [ "$(hash "$run_dir/first.log")" != "$log_hash" ] ||
   [ "$(hash "$run_dir/first.sems")" != "$snapshot_hash" ]; then
    echo "error: native ZIP checkpoint artifacts differ" >&2
    exit 1
fi
if ! grep -F -x -q \
    'stop=budget pc=0x000920b4 instructions=459796107 virtual_time_ns=1998752649' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|status=refuse' \
    "$run_dir/first.log" ||
   [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 76258 ]; then
    echo "error: native ZIP boundary or compatibility count changed" >&2
    exit 1
fi

# Preserve the ticket-748 prefix; only its next-step mode refusal is superseded.
run prefix 451511675
if [ "$(hash "$run_dir/prefix.log")" != 33f75a3051a8487405a4f5221d9db36a806fc54b2cf26fee8ebff5082bf62a7e ] ||
   [ "$(hash "$run_dir/prefix.sems")" != 42de6549afe8bef32603a4acd497f2aee0bb41a92a77f59022064c23e194998b ]; then
    echo "error: historical checkpoint changed" >&2
    exit 1
fi
run resumed 459796107 --snapshot-load "$run_dir/prefix.sems"
if ! cmp -s "$run_dir/first.sems" "$run_dir/resumed.sems"; then
    echo "error: resumed native ZIP checkpoint differs" >&2
    exit 1
fi
run refusal 459796108 --snapshot-load "$run_dir/first.sems"
if ! grep -F -x -q \
    'stop=compat-refused pc=0x000920b4 instructions=459796107 virtual_time_ns=1998752649 detail=unknown Sapporo 2.39 file open mode' \
    "$run_dir/refusal.log"; then
    echo "error: unhandled mode-nine path refusal changed" >&2
    cat "$run_dir/refusal.log" >&2
    exit 1
fi
if [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: source flash was modified" >&2
    exit 1
fi
echo "PASS sapporo-2.39.20 native ZIP read, old prefix, resume and next-path refusal"
