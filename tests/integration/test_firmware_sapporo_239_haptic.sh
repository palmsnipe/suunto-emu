#!/bin/sh
# TEST_TAGS: sapporo_239_haptic
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 haptic runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
expected_flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
expected_log_hash=f1c41ec3d40617174d8cbb299883c69b028a6bfb445b44a0bb7aaf2622915354
expected_snapshot_hash=629ba604acfbb1eb1265a755283c6133b9650d45dcf2c44d9439752365e22247

if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 haptic runner: set SEMU_SAPPORO_239_FULL_FLASH"
    exit 0
fi
if [ ! -r "$full_flash" ]; then
    echo "error: Sapporo 2.39 full flash is not readable: $full_flash" >&2
    exit 2
fi
flash_hash=$(shasum -a 256 "$full_flash" | awk '{print $1}')
if [ "$flash_hash" != "$expected_flash_hash" ]; then
    echo "error: Sapporo 2.39 full flash hash mismatch" >&2
    exit 2
fi

run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-haptic.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

run_once()
{
    output=$1
    snapshot=$2
    if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --until normal-frame --max-instructions 122457908 \
        --max-time 30000000000 --snapshot-save "$snapshot" \
        >"$output" 2>&1;
    then
        run_code=0
    else
        run_code=$?
    fi
    if [ "$run_code" -ne 3 ]; then
        echo "error: Sapporo 2.39 haptic run returned $run_code" >&2
        cat "$output" >&2
        exit 1
    fi
}

run_once "$run_dir/first.log" "$run_dir/first.sems"
run_once "$run_dir/second.log" "$run_dir/second.sems"
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log" ||
   ! cmp -s "$run_dir/first.sems" "$run_dir/second.sems"; then
    echo "error: Sapporo 2.39 haptic runs differ" >&2
    diff -u "$run_dir/first.log" "$run_dir/second.log" >&2 || true
    exit 1
fi
if [ "$(shasum -a 256 "$run_dir/first.log" | awk '{print $1}')" != \
     "$expected_log_hash" ] ||
   [ "$(shasum -a 256 "$run_dir/first.sems" | awk '{print $1}')" != \
     "$expected_snapshot_hash" ]; then
    echo "error: Sapporo 2.39 haptic artifact hash changed" >&2
    cat "$run_dir/first.log" >&2
    exit 1
fi
if ! grep -F -x -q \
    'stop=budget pc=0x0014e8ea instructions=122457908 virtual_time_ns=1230996595' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|unknown Sapporo' \
    "$run_dir/first.log"; then
    echo "error: Sapporo 2.39 haptic checkpoint changed" >&2
    cat "$run_dir/first.log" >&2
    exit 1
fi
if [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 118 ]; then
    echo "error: Sapporo 2.39 compatibility count changed" >&2
    exit 1
fi

if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
    --until normal-frame --max-instructions 122457909 \
    --max-time 30000000000 --snapshot-load "$run_dir/first.sems" \
    >"$run_dir/resume.log" 2>&1;
then
    resume_code=0
else
    resume_code=$?
fi
if [ "$resume_code" -ne 3 ] || ! grep -F -x -q \
    'stop=budget pc=0x001c0db4 instructions=122457909 virtual_time_ns=1230996596' \
    "$run_dir/resume.log"; then
    echo "error: Sapporo 2.39 haptic next-boundary resume changed" >&2
    cat "$run_dir/resume.log" >&2
    exit 1
fi
if [ "$(shasum -a 256 "$full_flash" | awk '{print $1}')" != \
     "$expected_flash_hash" ]; then
    echo "error: Sapporo 2.39 source flash was modified" >&2
    exit 1
fi

echo "PASS sapporo-2.39.20 native haptic selector checkpoint"
