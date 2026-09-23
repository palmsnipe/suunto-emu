#!/bin/sh
# TEST_TAGS: sapporo_235_gps_awake
# Ticket 789 / E-EMU-SAPPORO-BRANCH-GATES-001: paired F57F correction audit.
# E-SAP-0048: eight synthetic GPIO24 awake pulses; ninth admission refuses.
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
[ "$(grep -c 'event=layer-hit layer=sapporo-2.35-gps-awake' "$run_dir/reopen-1.log")" -eq 8 ]
[ "$(shasum -a 256 "$run_dir/reopen-1.log" | awk '{print $1}')" = \
    988c24e77f3b25b2be945f3d81a7f6ca3f17dfae94bd63cbb1eecb14533fbfe3 ]
grep -Fqx 'stop=compat-refused pc=0x001259fe instructions=1638733422 virtual_time_ns=54660679480 detail=2.35 GPS awake fixture disabled, exhausted or unexpected state' "$run_dir/reopen-1.log"
echo 'PASS sapporo-2.35.34 bounded GPS awake sequence'
