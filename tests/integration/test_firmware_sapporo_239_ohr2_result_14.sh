#!/bin/sh
# TEST_TAGS: sapporo_239_ohr2_result_14
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 OHR2 result-14 runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
expected_flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
expected_log_hash=6acd49e2ac13bee2aecfd80382023ed706735fd204e7b129a3dfc7ac25509479
expected_snapshot_hash=985fc5151d50f839eb023d76e553c871c8f71dc8ccff5e61b2b8f7ea944bcf6c

if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 OHR2 result-14 runner: set SEMU_SAPPORO_239_FULL_FLASH"
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

run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-ohr2-result14.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

run_once()
{
    output=$1
    snapshot=$2
    if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --until normal-frame --max-instructions 371351594 \
        --max-time 30000000000 --snapshot-save "$snapshot" \
        >"$output" 2>&1;
    then
        run_code=0
    else
        run_code=$?
    fi
    if [ "$run_code" -ne 3 ]; then
        echo "error: Sapporo 2.39 OHR2 result-14 run returned $run_code" >&2
        cat "$output" >&2
        exit 1
    fi
}

run_once "$run_dir/first.log" "$run_dir/first.sems"
run_once "$run_dir/second.log" "$run_dir/second.sems"
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log" ||
   ! cmp -s "$run_dir/first.sems" "$run_dir/second.sems"; then
    echo "error: Sapporo 2.39 OHR2 result-14 runs differ" >&2
    diff -u "$run_dir/first.log" "$run_dir/second.log" >&2 || true
    exit 1
fi
if [ "$(shasum -a 256 "$run_dir/first.log" | awk '{print $1}')" != \
     "$expected_log_hash" ] ||
   [ "$(shasum -a 256 "$run_dir/first.sems" | awk '{print $1}')" != \
     "$expected_snapshot_hash" ]; then
    echo "error: Sapporo 2.39 OHR2 result-14 artifact hash changed" >&2
    cat "$run_dir/first.log" >&2
    exit 1
fi
for expected in \
    'kind=request command=0x000d sequence=4 state=MAIN status=ok ready=1' \
    'kind=response command=0x000d sequence=4 state=MAIN status=ok ready=0' \
    'kind=request command=0x000e sequence=5 state=MAIN status=ok ready=1' \
    'kind=response command=0x000e sequence=5 state=MAIN status=ok ready=0'
do
    if ! grep -F -q "$expected" "$run_dir/first.log"; then
        echo "error: missing OHR2 transcript: $expected" >&2
        exit 1
    fi
done
if ! grep -F -x -q \
    'stop=budget pc=0x0014e8ee instructions=371351594 virtual_time_ns=1891143442' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|command=0x000e.*state=MAIN.*status=refuse' \
    "$run_dir/first.log"; then
    echo "error: Sapporo 2.39 OHR2 result-14 checkpoint changed" >&2
    cat "$run_dir/first.log" >&2
    exit 1
fi
if [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 118 ]; then
    echo "error: Sapporo 2.39 compatibility count changed" >&2
    exit 1
fi

if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
    --until normal-frame --max-instructions 371351595 \
    --max-time 30000000000 --snapshot-load "$run_dir/first.sems" \
    >"$run_dir/resume.log" 2>&1;
then
    resume_code=0
else
    resume_code=$?
fi
if [ "$resume_code" -ne 3 ] || ! grep -F -x -q \
    'stop=budget pc=0x0014e9c0 instructions=371351595 virtual_time_ns=1891143443' \
    "$run_dir/resume.log" || ! grep -F -q \
    'kind=request command=0x000e sequence=5 state=MAIN status=ok ready=1' \
    "$run_dir/first.log"; then
    echo "error: Sapporo 2.39 post-result-14 boundary changed" >&2
    cat "$run_dir/resume.log" >&2
    exit 1
fi
if [ "$(shasum -a 256 "$full_flash" | awk '{print $1}')" != \
     "$expected_flash_hash" ]; then
    echo "error: Sapporo 2.39 source flash was modified" >&2
    exit 1
fi

echo "PASS sapporo-2.39.20 OHR2 result-14 checkpoint"
