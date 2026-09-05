#!/bin/sh
# TEST_TAGS: sapporo_239_file_size
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 logical file size: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
log_hash=8139068b549a4e2be4baf57c94bc3b8eff385cb2bbb8efca506a8b30469de0d8
snapshot_hash=0fa411dde053a15ef42d1b4ce2bf7282ad1990f88532b059dcf1c14ca9824193
if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 logical file size: set SEMU_SAPPORO_239_FULL_FLASH"
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
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-file-size.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

run_once()
{
    if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --until normal-frame --max-instructions 405895301 \
        --max-time 30000000000 --snapshot-save "$run_dir/$1.sems" \
        >"$run_dir/$1.log" 2>&1;
    then
        code=0
    else
        code=$?
    fi
    if [ "$code" -ne 3 ]; then
        echo "error: logical file size checkpoint returned $code" >&2
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
    echo "error: logical file size checkpoint artifacts differ" >&2
    exit 1
fi
for expected in \
    'operation=size path=sleepln/sleep.bin result=17888 size=17888 cursor=24' \
    'operation=size path=tssln/tss.bin result=2384 size=2384 cursor=24'
do
    if ! grep -F -q "$expected" "$run_dir/first.log"; then
        echo "error: missing logical file size transcript: $expected" >&2
        exit 1
    fi
done
if ! grep -F -x -q \
    'stop=budget pc=0x00079e1c instructions=405895301 virtual_time_ns=1927243545' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|status=refuse' \
    "$run_dir/first.log" ||
   [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 512 ]; then
    echo "error: logical file size execution boundary changed" >&2
    exit 1
fi

# Preserve the real firmware BKPT; it is not an instruction compatibility hook.
if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
    --until normal-frame --max-instructions 405895302 \
    --max-time 30000000000 --snapshot-load "$run_dir/first.sems" \
    >"$run_dir/resume.log" 2>&1;
then
    code=0
else
    code=$?
fi
if [ "$code" -ne 0 ] || ! grep -F -x -q \
    'stop=halt pc=0x00079e1e instructions=405895302 virtual_time_ns=1927243546' \
    "$run_dir/resume.log"; then
    echo "error: post-file-size firmware breakpoint changed" >&2
    cat "$run_dir/resume.log" >&2
    exit 1
fi
if [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: source flash was modified" >&2
    exit 1
fi
echo "PASS sapporo-2.39.20 logical file size checkpoint and firmware breakpoint"
