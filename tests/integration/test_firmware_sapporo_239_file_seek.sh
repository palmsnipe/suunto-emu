#!/bin/sh
# TEST_TAGS: sapporo_239_file_seek
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 logical seek return: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
log_hash=4f8e749ebe80774b968a091dda8086aabeb85229e6dd08b916cb615e24e6497e
snapshot_hash=92e7f05339ff218402f0b788a6263dc8173c81fba30d56d4cf8c66c1bab09a16
if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 logical seek return: set SEMU_SAPPORO_239_FULL_FLASH"
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
# Check every manifest component before either authentic execution.
"$emulator" validate --profile sapporo-2.39.20 --firmware "$manifest"
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-file-seek.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

run_once()
{
    if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --until normal-frame --max-instructions 416256851 \
        --max-time 30000000000 --snapshot-save "$run_dir/$1.sems" \
        >"$run_dir/$1.log" 2>&1;
    then
        code=0
    else
        code=$?
    fi
    if [ "$code" -ne 3 ]; then
        echo "error: logical seek return checkpoint returned $code" >&2
        cat "$run_dir/$1.log" >&2
        exit 1
    fi
}
run_once first
run_once second
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log" ||
   ! cmp -s "$run_dir/first.sems" "$run_dir/second.sems" ||
   [ "$(hash "$run_dir/first.log")" != "$log_hash" ] ||
   [ "$(hash "$run_dir/first.sems")" != "$snapshot_hash" ]; then
    echo "error: logical seek return checkpoint artifacts differ" >&2
    exit 1
fi
for expected in \
    'operation=seek path=tssln/tss.bin result=32 size=2384 cursor=32' \
    'operation=read path=tssln/tss.bin result=56 size=2384 cursor=2384'
do
    if ! grep -F -q "$expected" "$run_dir/first.log"; then
        echo "error: missing logical seek return transcript: $expected" >&2
        exit 1
    fi
done
if ! grep -F -x -q \
    'stop=budget pc=0x00079e1c instructions=416256851 virtual_time_ns=1955213393' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|status=refuse' \
    "$run_dir/first.log" ||
   [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 595 ]; then
    echo "error: logical seek return execution boundary changed" >&2
    exit 1
fi

if [ "$(grep -c 'operation=read path=tssln/tss.bin result=56' "$run_dir/first.log")" -ne 42 ]; then
    echo "error: native training record scan changed" >&2
    exit 1
fi

# Preserve the real firmware BKPT; it is not an instruction compatibility hook.
if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
    --until normal-frame --max-instructions 416256852 \
    --max-time 30000000000 --snapshot-load "$run_dir/first.sems" \
    >"$run_dir/resume.log" 2>&1;
then
    code=0
else
    code=$?
fi
if [ "$code" -ne 0 ] || ! grep -F -x -q \
    'stop=halt pc=0x00079e1e instructions=416256852 virtual_time_ns=1955213394' \
    "$run_dir/resume.log"; then
    echo "error: post-file-seek firmware breakpoint changed" >&2
    cat "$run_dir/resume.log" >&2
    exit 1
fi
if [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: source flash was modified" >&2
    exit 1
fi
echo "PASS sapporo-2.39.20 logical seek return checkpoint and firmware breakpoint"
