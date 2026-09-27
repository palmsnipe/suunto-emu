#!/bin/sh
# TEST_TAGS: sapporo_239_ohr2_main_identity
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 OHR2 MAIN identity runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
expected_flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
expected_log_hash=1422edc900159944aba591e09ec540631ad4194e0665b48f7bb4a29c899b30d5
expected_snapshot_hash=1f93dcf9e15c6ace65d8c808e02a6e56a36561fa226cf05ee94e4613a80388d0

if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 OHR2 MAIN identity runner: set SEMU_SAPPORO_239_FULL_FLASH"
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

run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-ohr2-main.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

run_once()
{
    output=$1
    snapshot=$2
    if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --until normal-frame --max-instructions 371304339 \
        --max-time 30000000000 --snapshot-save "$snapshot" \
        >"$output" 2>&1;
    then
        run_code=0
    else
        run_code=$?
    fi
    if [ "$run_code" -ne 3 ]; then
        echo "error: Sapporo 2.39 OHR2 MAIN identity run returned $run_code" >&2
        cat "$output" >&2
        exit 1
    fi
}

run_once "$run_dir/first.log" "$run_dir/first.sems"
run_once "$run_dir/second.log" "$run_dir/second.sems"
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log" ||
   ! cmp -s "$run_dir/first.sems" "$run_dir/second.sems"; then
    echo "error: Sapporo 2.39 OHR2 MAIN identity runs differ" >&2
    diff -u "$run_dir/first.log" "$run_dir/second.log" >&2 || true
    exit 1
fi
if [ "$(shasum -a 256 "$run_dir/first.log" | awk '{print $1}')" != \
     "$expected_log_hash" ] ||
   [ "$(shasum -a 256 "$run_dir/first.sems" | awk '{print $1}')" != \
     "$expected_snapshot_hash" ]; then
    echo "error: Sapporo 2.39 OHR2 MAIN identity artifact hash changed" >&2
    cat "$run_dir/first.log" >&2
    exit 1
fi
for expected in \
    'kind=request command=0x0000 sequence=1 state=BSL status=ok ready=1' \
    'kind=request command=0x0003 sequence=2 state=MAIN status=ok ready=0' \
    'kind=request command=0x0010 sequence=2 state=MAIN status=ok ready=1' \
    'kind=request command=0x0000 sequence=3 state=MAIN status=ok ready=1' \
    'kind=response command=0x0000 sequence=3 state=MAIN status=ok ready=0'
do
    if ! grep -F -q "$expected" "$run_dir/first.log"; then
        echo "error: missing OHR2 transcript: $expected" >&2
        exit 1
    fi
done
if ! grep -F -x -q \
    'stop=budget pc=0x0014e8ee instructions=371304339 virtual_time_ns=1891096187' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|command=0x0000.*state=MAIN.*status=refuse' \
    "$run_dir/first.log"; then
    echo "error: Sapporo 2.39 OHR2 MAIN identity checkpoint changed" >&2
    cat "$run_dir/first.log" >&2
    exit 1
fi
if [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 118 ]; then
    echo "error: Sapporo 2.39 compatibility count changed" >&2
    exit 1
fi

if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
    --until normal-frame --max-instructions 371304340 \
    --max-time 30000000000 --snapshot-load "$run_dir/first.sems" \
    >"$run_dir/resume.log" 2>&1;
then
    resume_code=0
else
    resume_code=$?
fi
if [ "$resume_code" -ne 3 ] || ! grep -F -x -q \
    'stop=budget pc=0x0014e9c0 instructions=371304340 virtual_time_ns=1891096188' \
    "$run_dir/resume.log" || ! grep -F -q \
    'kind=request command=0x0000 sequence=3 state=MAIN status=ok ready=1' \
    "$run_dir/first.log"; then
    echo "error: Sapporo 2.39 post-MAIN-identity boundary changed" >&2
    cat "$run_dir/resume.log" >&2
    exit 1
fi
if [ "$(shasum -a 256 "$full_flash" | awk '{print $1}')" != \
     "$expected_flash_hash" ]; then
    echo "error: Sapporo 2.39 source flash was modified" >&2
    exit 1
fi

echo "PASS sapporo-2.39.20 OHR2 MAIN identity checkpoint"
