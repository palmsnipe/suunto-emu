#!/bin/sh
# TEST_TAGS: sapporo_239_ohr2_echo
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 OHR2 echo runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
expected_flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
expected_log_hash=2ef900dbf79d08a83e94c2e6d8e642c53b51fa40d3faaca977319ac64f4d59cf
expected_snapshot_hash=5168ba1e48997e23553370705d49f4ea0f83c407337576bb3c7a56cacb308686

if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 OHR2 echo runner: set SEMU_SAPPORO_239_FULL_FLASH"
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

run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-ohr2-echo.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

run_once()
{
    output=$1
    snapshot=$2
    if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --until normal-frame --max-instructions 369037329 \
        --max-time 30000000000 --snapshot-save "$snapshot" \
        >"$output" 2>&1;
    then
        run_code=0
    else
        run_code=$?
    fi
    if [ "$run_code" -ne 3 ]; then
        echo "error: Sapporo 2.39 OHR2 echo run returned $run_code" >&2
        cat "$output" >&2
        exit 1
    fi
}

run_once "$run_dir/first.log" "$run_dir/first.sems"
run_once "$run_dir/second.log" "$run_dir/second.sems"
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log" ||
   ! cmp -s "$run_dir/first.sems" "$run_dir/second.sems"; then
    echo "error: Sapporo 2.39 OHR2 echo runs differ" >&2
    diff -u "$run_dir/first.log" "$run_dir/second.log" >&2 || true
    exit 1
fi
if [ "$(shasum -a 256 "$run_dir/first.log" | awk '{print $1}')" != \
     "$expected_log_hash" ] ||
   [ "$(shasum -a 256 "$run_dir/first.sems" | awk '{print $1}')" != \
     "$expected_snapshot_hash" ]; then
    echo "error: Sapporo 2.39 OHR2 echo artifact hash changed" >&2
    cat "$run_dir/first.log" >&2
    exit 1
fi
for expected in \
    'kind=request command=0x000e sequence=5 state=MAIN status=ok ready=1' \
    'kind=response command=0x000e sequence=5 state=MAIN status=ok ready=0' \
    'kind=request command=0x0006 sequence=6 state=MAIN status=ok ready=1' \
    'kind=response command=0x0006 sequence=6 state=MAIN status=ok ready=0'
do
    if ! grep -F -q "$expected" "$run_dir/first.log"; then
        echo "error: missing OHR2 transcript: $expected" >&2
        exit 1
    fi
done
if ! grep -F -x -q \
    'stop=budget pc=0x0014e8ea instructions=369037329 virtual_time_ns=1890385573' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|command=0x0006.*state=MAIN.*status=refuse' \
    "$run_dir/first.log"; then
    echo "error: Sapporo 2.39 OHR2 echo checkpoint changed" >&2
    cat "$run_dir/first.log" >&2
    exit 1
fi
if [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 118 ]; then
    echo "error: Sapporo 2.39 compatibility count changed" >&2
    exit 1
fi

if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
    --until normal-frame --max-instructions 369037330 \
    --max-time 30000000000 --snapshot-load "$run_dir/first.sems" \
    >"$run_dir/resume.log" 2>&1;
then
    resume_code=0
else
    resume_code=$?
fi
if [ "$resume_code" -ne 3 ] || ! grep -F -x -q \
    'stop=budget pc=0x001c0db4 instructions=369037330 virtual_time_ns=1890385574' \
    "$run_dir/resume.log" || ! grep -F -q \
    'kind=request command=0x0002 sequence=7 state=MAIN status=refuse ready=0' \
    "$run_dir/resume.log"; then
    echo "error: Sapporo 2.39 post-echo boundary changed" >&2
    cat "$run_dir/resume.log" >&2
    exit 1
fi
if [ "$(shasum -a 256 "$full_flash" | awk '{print $1}')" != \
     "$expected_flash_hash" ]; then
    echo "error: Sapporo 2.39 source flash was modified" >&2
    exit 1
fi

echo "PASS sapporo-2.39.20 OHR2 exact echo checkpoint"
