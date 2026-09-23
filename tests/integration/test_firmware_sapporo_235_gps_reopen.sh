#!/bin/sh
# TEST_TAGS: sapporo_235_gps_reopen
# Ticket 789 / E-EMU-SAPPORO-BRANCH-GATES-001: paired F57F correction audit.
# E-SAP-0047: native reopen with two synthetic responses; GSTP stays refused.
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.35.34 ]; then
    echo "SKIP Sapporo 2.35 GPS reopen runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi
emulator=${SEMU_EMULATOR:?}
manifest=${SEMU_FIRMWARE_MANIFEST:?}
"$emulator" validate --profile sapporo-2.35.34 --firmware "$manifest"
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-235-reopen.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
for pass in 1 2; do
    rc=0
    "$emulator" run --profile sapporo-2.35.34 --firmware "$manifest" \
        --layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup \
        --layer sapporo-2.35-gps-startup --layer sapporo-2.35-gps-reopen \
        --max-instructions 2000000000 --max-time 30000000000 \
        > "$run_dir/reopen-$pass.log" 2>&1 || rc=$?
    if [ "$rc" -ne 3 ]; then cat "$run_dir/reopen-$pass.log"; exit 1; fi
done
cmp "$run_dir/reopen-1.log" "$run_dir/reopen-2.log"
[ "$(shasum -a 256 "$run_dir/reopen-1.log" | awk '{print $1}')" = \
    74dca7ea9589a670d9058b253e9f591ae50fb8281b633f1f40471f8653d40589 ]
grep -Fqx 'stop=compat-refused pc=0x001be85a instructions=1080305993 virtual_time_ns=16306637984 detail=2.35 GPS reopen fixture disabled, exhausted or unexpected state/request' "$run_dir/reopen-1.log"
[ "$(grep -c 'event=layer-hit layer=sapporo-2.35-gps-reopen' "$run_dir/reopen-1.log")" -eq 2 ]
[ "$(grep -c 'event=layer-hit layer=sapporo-2.35-gps-startup' "$run_dir/reopen-1.log")" -eq 2 ]
echo 'PASS sapporo-2.35.34 GPS reopen status/GSR exchange'
