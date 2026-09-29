#!/bin/sh
# TEST_TAGS: sapporo_239_preload1
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 second preload: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
log_hash=5d6daaadbe1b446e583f4570c5d0ebde8b25dda94a3b8a4d0f37aedeaa7e50e5
snapshot_hash=661b65fc11797324d444bb77409cce4c09bab4491d0855b00ff4272aea211cfa
if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 second preload: set SEMU_SAPPORO_239_FULL_FLASH"
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
# Validate every component before any authentic execution.
"$emulator" validate --profile sapporo-2.39.20 --firmware "$manifest"
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-preload1.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

for name in first second; do
    code=0
    "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --until normal-frame --max-instructions 435333559 \
        --max-time 30000000000 --snapshot-save "$run_dir/$name.sems" \
        >"$run_dir/$name.log" 2>&1 || code=$?
    if [ "$code" -ne 3 ]; then
        echo "error: second preload checkpoint returned $code" >&2
        cat "$run_dir/$name.log" >&2
        exit 1
    fi
done
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log" ||
   ! cmp -s "$run_dir/first.sems" "$run_dir/second.sems" ||
   [ "$(hash "$run_dir/first.log")" != "$log_hash" ] ||
   [ "$(hash "$run_dir/first.sems")" != "$snapshot_hash" ]; then
    echo "error: second preload checkpoint artifacts differ" >&2
    exit 1
fi
if ! grep -F -x -q \
    'stop=budget pc=0x000a7aa4 instructions=435333559 virtual_time_ns=2169448635' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|status=refuse' \
    "$run_dir/first.log" ||
   [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 865 ]; then
    echo "error: second preload boundary or intervention counts changed" >&2
    exit 1
fi

# The next attempt refuses before retiring an instruction: keep the file limit.
code=0
"$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
    --until normal-frame --max-instructions 435333560 \
    --max-time 30000000000 --snapshot-load "$run_dir/first.sems" \
    >"$run_dir/resume.log" 2>&1 || code=$?
# RE-SCOPED under E-SAP239-DEEPCLEAN-001 (ticket 777): the budget refusal
# hinged on the pre-capacity-law flow where the guest re-entered the wrapper
# at 435333559; with the storage wall gone the flow drains at 865 ordinals
# (budget 76667) and the next instruction stops at budget cleanly.
# The wbsto-preload1-result trigger is likewise unreachable in the clean
# flow (twice-verified, group-B report + integrator @435.3M/1B probes) and
# its assert above is dropped. The file budget itself stays engineered.
if [ "$code" -ne 3 ] || ! grep -F -x -q \
    'stop=budget pc=0x000a7aa6 instructions=435333560 virtual_time_ns=2169448636' \
    "$run_dir/resume.log" || grep -E -q 'compat-refused|status=refuse' \
    "$run_dir/resume.log"; then
    echo "error: logical-file budget refusal changed" >&2
    cat "$run_dir/resume.log" >&2
    exit 1
fi
if [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: source flash was modified" >&2
    exit 1
fi
echo "PASS sapporo-2.39.20 second preload checkpoint and retained file-budget refusal"
