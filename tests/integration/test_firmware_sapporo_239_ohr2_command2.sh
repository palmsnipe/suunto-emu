#!/bin/sh
# TEST_TAGS: sapporo_239_ohr2_command2
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 OHR2 command 2: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
# E-ULS-0047 (ticket 777) re-derivation. The era drift recorded by E-ULS-0041
# (accepted integration batch d311da0..6555d38) replaced guest instructions with
# equal-count paths: the instruction, virtual-time, and command 2 transcript
# anchors below are unchanged from the original pins. The PC at the budget cap
# moved and the intervention census at this cap is 452 (was 449; the same
# redistribution the other era checkpoints record), each re-derived from two
# byte-identical runs. The post-boundary run is pinned as a budget-cap
# continuation (exit code 3) per the E-ULS-0041 BKPT-to-NOP precedent.
log_hash=d077cb02a257a4299906aadcf67876af4e13053e8a908e7a480bf02ff6027b4a
snapshot_hash=aa81dee0e14c6988f7a5d2ece6b2a824177bfcc7cd092bb676d32ff0f79fbbf5
if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 OHR2 command 2: set SEMU_SAPPORO_239_FULL_FLASH"
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
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-command2.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

run_once()
{
    if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --until normal-frame --max-instructions 393235868 \
        --max-time 30000000000 --snapshot-save "$run_dir/$1.sems" \
        >"$run_dir/$1.log" 2>&1;
    then
        code=0
    else
        code=$?
    fi
    if [ "$code" -ne 3 ]; then
        echo "error: command 2 checkpoint returned $code" >&2
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
    echo "error: command 2 checkpoint artifacts differ" >&2
    exit 1
fi
for expected in \
    'kind=request command=0x0002 sequence=7 state=MAIN status=ok ready=1' \
    'kind=response command=0x0002 sequence=7 state=MAIN status=ok ready=0'
do
    if ! grep -F -q "$expected" "$run_dir/first.log"; then
        echo "error: missing command 2 transcript: $expected" >&2
        exit 1
    fi
done
if ! grep -F -x -q \
    'stop=budget pc=0x000a7ac6 instructions=393235868 virtual_time_ns=1913027716' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|status=refuse' \
    "$run_dir/first.log" ||
   [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 193 ]; then
    echo "error: command 2 execution boundary changed" >&2
    exit 1
fi

# Resume from the pinned boundary; the next guest instruction executes exactly
# as recorded, stopping at the budget cap with exit code 3 (E-ULS-0041 precedent).
if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
    --until normal-frame --max-instructions 393235869 \
    --max-time 30000000000 --snapshot-load "$run_dir/first.sems" \
    >"$run_dir/resume.log" 2>&1;
then
    code=0
else
    code=$?
fi
if [ "$code" -ne 3 ] || ! grep -F -x -q \
    'stop=budget pc=0x000a7ac8 instructions=393235869 virtual_time_ns=1913027717' \
    "$run_dir/resume.log"; then
    echo "error: post-command-2 boundary continuation changed" >&2
    cat "$run_dir/resume.log" >&2
    exit 1
fi
if [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: source flash was modified" >&2
    exit 1
fi
echo "PASS sapporo-2.39.20 OHR2 command 2 checkpoint and boundary continuation"
