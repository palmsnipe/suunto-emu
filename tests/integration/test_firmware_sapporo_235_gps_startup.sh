#!/bin/sh
# TEST_TAGS: sapporo_235_gps_startup
# Ticket 789 / E-EMU-SAPPORO-BRANCH-GATES-001: paired F57F correction audit.
# E-SAP-0046: two synthetic responses, native parsing and initial UART close.
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.35.34 ]; then
    echo "SKIP Sapporo 2.35 GPS runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi
emulator=${SEMU_EMULATOR:?}
manifest=${SEMU_FIRMWARE_MANIFEST:?}
"$emulator" validate --profile sapporo-2.35.34 --firmware "$manifest"
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-235-gps.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
for pass in 1 2; do
    rc=0
    "$emulator" run --profile sapporo-2.35.34 --firmware "$manifest" \
        --layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup \
        --layer sapporo-2.35-gps-startup \
        --max-instructions 500000000 --max-time 5000000000 \
        > "$run_dir/startup-$pass.log" 2>&1 || rc=$?
    if [ "$rc" -ne 3 ]; then cat "$run_dir/startup-$pass.log"; exit 1; fi
done
cmp "$run_dir/startup-1.log" "$run_dir/startup-2.log"
[ "$(shasum -a 256 "$run_dir/startup-1.log" | awk '{print $1}')" = \
    7bd0d0f42cd498d8b42853de13c1e4f86afc8cf759f4ca8a3565f90aacfeba1d ]
grep -Fqx 'stop=budget pc=0x00093222 instructions=500000000 virtual_time_ns=3343660033' "$run_dir/startup-1.log"
[ "$(grep -c 'event=layer-hit layer=sapporo-2.35-gps-startup' "$run_dir/startup-1.log")" -eq 2 ]
if grep -Eq 'machine-reset-request|status=refuse' "$run_dir/startup-1.log"; then
    echo 'error: initial GPS prefix refused or reset' >&2
    exit 1
fi
for pass in 1 2; do
    rc=0
    "$emulator" run --profile sapporo-2.35.34 --firmware "$manifest" \
        --layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup \
        --layer sapporo-2.35-gps-startup \
        --max-instructions 1500000000 --max-time 30000000000 \
        > "$run_dir/boundary-$pass.log" 2>&1 || rc=$?
    [ "$rc" -eq 3 ]
done
cmp "$run_dir/boundary-1.log" "$run_dir/boundary-2.log"
[ "$(shasum -a 256 "$run_dir/boundary-1.log" | awk '{print $1}')" = \
    96104386daadc0084bf653621b2219558ccd8bd3717ad268da33c7c2bc360df5 ]
grep -Fqx 'stop=compat-refused pc=0x001254ec instructions=1059785208 virtual_time_ns=14811876715 detail=2.35 GPS startup fixture disabled, exhausted or unexpected state/request' "$run_dir/boundary-1.log"
[ "$(grep -c 'event=layer-hit layer=sapporo-2.35-gps-startup' "$run_dir/boundary-1.log")" -eq 2 ]
echo 'PASS sapporo-2.35.34 initial GPS status/version exchange'
