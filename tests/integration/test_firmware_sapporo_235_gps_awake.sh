#!/bin/sh
# TEST_TAGS: sapporo_235_gps_awake
# Ticket 789 / E-EMU-SAPPORO-BRANCH-GATES-001: paired F57F correction audit.
# E-SAP-0048/E-SAP-0049: lane-backed synthetic GPIO24 awake pulses (budget 64).
# This bounded run stops on budget at 70 s with eleven healthy polls; the 11 hits are the polls.
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.35.34 ]; then
    echo "SKIP Sapporo 2.35 GPS awake runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi
emulator=${SEMU_EMULATOR:?}
manifest=${SEMU_FIRMWARE_MANIFEST:?}
"$emulator" validate --profile sapporo-2.35.34 --firmware "$manifest"
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-235-awake.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
for pass in 1 2; do
    rc=0
    "$emulator" run --profile sapporo-2.35.34 --firmware "$manifest" \
        --layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup \
        --layer sapporo-2.35-gps-startup --layer sapporo-2.35-gps-reopen \
        --layer sapporo-2.35-gps-awake \
        --max-instructions 3000000000 --max-time 70000000000 \
        > "$run_dir/reopen-$pass.log" 2>&1 || rc=$?
    if [ "$rc" -ne 3 ]; then cat "$run_dir/reopen-$pass.log"; exit 1; fi
done
cmp "$run_dir/reopen-1.log" "$run_dir/reopen-2.log"
[ "$(grep -c 'event=layer-hit layer=sapporo-2.35-gps-awake' "$run_dir/reopen-1.log")" -eq 11 ]
[ "$(shasum -a 256 "$run_dir/reopen-1.log" | awk '{print $1}')" = \
    16e3d4d3bb88b27e669c73bdb254d5621fa6fcc35a5ed3b6998e4407dd64540b ]
grep -Fqx 'stop=budget pc=0x000e1862 instructions=1860847385 virtual_time_ns=70000000000' "$run_dir/reopen-1.log"
echo 'PASS sapporo-2.35.34 bounded GPS awake sequence'
