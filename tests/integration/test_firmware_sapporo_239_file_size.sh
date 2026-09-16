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
# E-ULS-0047 (ticket 777) re-derivation. The era drift recorded by E-ULS-0041
# (accepted integration batch d311da0..6555d38) replaced guest instructions with
# equal-count paths: the instruction, virtual-time, and transcript anchors below
# are unchanged from the original pins. The PC at the budget cap moved and the
# intervention census completed earlier in the era (512 -> 595 by this cap; the
# same 595 total was already pinned one instruction window later by the seek
# checkpoint), each re-derived from two byte-identical runs. The post-boundary
# run is pinned as a budget-cap continuation (exit code 3) per the E-ULS-0041
# BKPT-to-NOP precedent.
log_hash=c84aee5768414f622b20620a5068d5add63ae07402ea8b6e44499b9acd16538d
snapshot_hash=d3a9e0fbc854e5b80d7a71ca2778692d7c893ee6b2423734db67bb2a8b8f0a17
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
    'stop=budget pc=0x001be364 instructions=405895301 virtual_time_ns=1927243545' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|status=refuse' \
    "$run_dir/first.log" ||
   [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 595 ]; then
    echo "error: logical file size execution boundary changed" >&2
    exit 1
fi

# Resume from the pinned boundary; the next guest instruction executes exactly
# as recorded, stopping at the budget cap with exit code 3 (E-ULS-0041 precedent).
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
if [ "$code" -ne 3 ] || ! grep -F -x -q \
    'stop=budget pc=0x001be368 instructions=405895302 virtual_time_ns=1927243546' \
    "$run_dir/resume.log"; then
    echo "error: post-file-size boundary continuation changed" >&2
    cat "$run_dir/resume.log" >&2
    exit 1
fi
if [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: source flash was modified" >&2
    exit 1
fi
echo "PASS sapporo-2.39.20 logical file size checkpoint and boundary continuation"
