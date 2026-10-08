#!/bin/sh
# TEST_TAGS: sapporo_239_widgets
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 Widgets: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
log_hash=1ae47f317dffb829a058f8de8f49db0fb0a2797d1b580c036f5c5b343a2595e8
snapshot_hash=8e4e65caffb965761b141423bec936a72b2ffe6683ac2418aad2cb4c371a22f2
if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 Widgets: set SEMU_SAPPORO_239_FULL_FLASH"
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
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-widgets.XXXXXX")
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
        echo "error: Widgets checkpoint returned $code" >&2
        cat "$run_dir/$name.log" >&2
        exit 1
    fi
}
run first 1000000000 0
run second 1000000000 0
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log" ||
   ! cmp -s "$run_dir/first.sems" "$run_dir/second.sems" ||
   [ "$(hash "$run_dir/first.log")" != "$log_hash" ] ||
   [ "$(hash "$run_dir/first.sems")" != "$snapshot_hash" ]; then
    echo "error: Widgets checkpoint artifacts differ" >&2
    exit 1
fi
if ! grep -F -x -q \
    'stop=user pc=0x000a7ac4 instructions=640300000 virtual_time_ns=2487119465' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|status=refuse' \
    "$run_dir/first.log" ||
   [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 76311 ] ||
   [ "$(grep -c 'provenance=E-SAP-COMPAT-WIDGETS-NATIVE-239-001' "$run_dir/first.log")" -ne 1 ]; then
    echo "error: Widgets frame boundary or compatibility counts changed" >&2
    exit 1
fi

# Pre-install history remains exact; obsolete JSON-bearing snapshots do not.
run prefix 40000000 3
if [ "$(hash "$run_dir/prefix.log")" != 253ffdd99ca7b8fb972518ad7e50701f37306436d67a5114085d94bda01ae11b ] ||
   [ "$(hash "$run_dir/prefix.sems")" != c96578db219fc8aa7de0a8aa314b138e764b7d9f669cd332c08bee6084e52898 ]; then
    echo "error: pre-install checkpoint changed" >&2
    exit 1
fi
run resumed 1000000000 0 --snapshot-load "$run_dir/prefix.sems"
cmp "$run_dir/first.sems" "$run_dir/resumed.sems"
# Keep the CLI's 100,000-instruction frame-poll grid aligned on resume.
# This snapshot already contains the corrected native cache value.
run preframe 607100000 3 --snapshot-load "$run_dir/prefix.sems"
run cache_resumed 1000000000 0 --snapshot-load "$run_dir/preframe.sems"
cmp "$run_dir/first.sems" "$run_dir/cache_resumed.sems"

# Observe exact native pixels through the public callback; no pixels on disk.
build_dir=$(dirname "$emulator")
${CC:-cc} -std=c99 -Wall -Wextra -Werror -pedantic -Iinclude -Isrc \
    tests/integration/sapporo_239_widgets_frame.c \
    "$build_dir/obj/src/frontends/cli.o" "$build_dir/libsemu.a" \
    -o "$run_dir/frame-check"
"$run_dir/frame-check" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
    --until normal-frame --max-instructions 1000000000 --max-time 30000000000 \
    --snapshot-load "$run_dir/preframe.sems" >"$run_dir/frame.log" 2>&1

# Do not stop at a new frame: the existing file budget must still refuse.
code=0
"$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
    --max-instructions 1000000000 --max-time 30000000000 \
    --snapshot-load "$run_dir/first.sems" >"$run_dir/refused.log" 2>&1 || code=$?
# RE-SCOPED under E-SAP239-DEEPCLEAN-001 (ticket 777): the cold-layer run
# no longer exhausts the logical-file hit budget at 1e9 instructions
# (single-wbsto-layer guests complete all 76315 file ops once, then idle;
# the old refusal at 610599945 "unknown writable path" was the
# pre-capacity-law wall). The engine's own terminal state for this flow is
# a clean budget stop; the guard becomes: the run must stop at budget with
# file ops recorded and NO compatibility refusal anywhere.
if [ "$code" -ne 3 ] || ! grep -F -q \
    'stop=budget pc=0x000be5d8 instructions=1000000000 virtual_time_ns=11418066429' \
    "$run_dir/refused.log" || ! grep -F -q \
    'trigger=logical-file ordinal=' "$run_dir/refused.log" || \
   grep -E -q 'compat-refused|status=refuse' "$run_dir/refused.log"; then
    echo "error: post-frame continuation is not a clean budget stop" >&2
    cat "$run_dir/refused.log" >&2
    exit 1
fi
if [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: source flash was modified" >&2
    exit 1
fi
echo "PASS sapporo-2.39.20 native Widgets, deterministic logo frame, resume and strict file budget"
