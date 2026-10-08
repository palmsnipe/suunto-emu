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
log_hash=98343f94333ab137a96ccac9694c48d0795e3df5a2b36c3f9f63558100ee1666
snapshot_hash=34bb5373a53c5f06978129161a19b0ffb7cd18e092c14f3a984a1557d7ae6c41
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
    'stop=budget pc=0x000d2090 instructions=459796107 virtual_time_ns=2193911183' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|status=refuse' \
    "$run_dir/first.log" ||
   [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 76200 ]; then
    echo "error: native ZIP boundary or compatibility count changed" >&2
    exit 1
fi

# Preserve the ticket-748 prefix; only its next-step mode refusal is superseded.
run prefix 451511675
if [ "$(hash "$run_dir/prefix.log")" != f9b42d8b79236485d5d7c67ade597972fde425a488738b6bdd559e341f82efb2 ] ||
   [ "$(hash "$run_dir/prefix.sems")" != 09ebe554c807c780697a458fe04227b32908b52882f207ee467cf4e1a89d4eab ]; then
    echo "error: historical checkpoint changed" >&2
    exit 1
fi
run resumed 459796107 --snapshot-load "$run_dir/prefix.sems"
if ! cmp -s "$run_dir/first.sems" "$run_dir/resumed.sems"; then
    echo "error: resumed native ZIP checkpoint differs" >&2
    exit 1
fi
run refusal 459796108 --snapshot-load "$run_dir/first.sems"
# RE-SCOPED under E-SAP239-DEEPCLEAN-001 (ticket 777): the mode-nine
# open-mode refusal is superseded by the file law (guest mode=3 admitted);
# the next instruction now runs as recorded and stops at budget.
if ! grep -F -x -q \
    'stop=budget pc=0x000d2092 instructions=459796108 virtual_time_ns=2193911184' \
    "$run_dir/refusal.log" || grep -E -q 'compat-refused|status=refuse' \
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
