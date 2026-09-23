#!/bin/sh
# TEST_TAGS: sapporo_235_block_erase
# Ticket 789 / E-EMU-SAPPORO-BRANCH-GATES-001: paired F57F correction audit.
# E-SAP-0043: storage recovery reaches native rendered pixels; later GPS is open.
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.35.34 ]; then
    echo "SKIP Sapporo 2.35 block erase runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi
emulator=${SEMU_EMULATOR:?}
manifest=${SEMU_FIRMWARE_MANIFEST:?}
"$emulator" validate --profile sapporo-2.35.34 --firmware "$manifest"
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-235-block-erase.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
for pass in 1 2; do
    "$emulator" run --profile sapporo-2.35.34 --firmware "$manifest" \
        --layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup \
        --until normal-frame --max-instructions 1000000000 \
        --max-time 10000000000 > "$run_dir/frame-$pass.log" 2>&1
done
cmp "$run_dir/frame-1.log" "$run_dir/frame-2.log"
[ "$(shasum -a 256 "$run_dir/frame-1.log" | awk '{print $1}')" = \
    a6f1f7d798372c89cb03e35edcdb0e95a3a5f3530895c9d840bc21794d0a96e5 ]
grep -Fqx 'stop=user pc=0x000a6bbe instructions=864000000 virtual_time_ns=3782156506' "$run_dir/frame-1.log"
[ "$(grep -c 'event=layer-hit layer=sapporo-2.35-ohr-startup' "$run_dir/frame-1.log")" -eq 8 ]
if grep -Eq 'machine-reset-request|status=refuse' "$run_dir/frame-1.log"; then
    echo 'error: recovered storage prefix refused or reset' >&2
    exit 1
fi
echo 'PASS sapporo-2.35.34 logbook recovery and first visible native frame'
