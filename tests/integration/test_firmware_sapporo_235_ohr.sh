#!/bin/sh
# TEST_TAGS: sapporo_235_ohr
# Ticket 789 / E-EMU-SAPPORO-BRANCH-GATES-001: paired F57F correction audit.
# E-SAP-0041/0043: explicit synthetic OHR startup; UI completion is not implied.
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.35.34 ]; then
    echo "SKIP Sapporo 2.35 OHR runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi
emulator=${SEMU_EMULATOR:?}
manifest=${SEMU_FIRMWARE_MANIFEST:?}
"$emulator" validate --profile sapporo-2.35.34 --firmware "$manifest"
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-235-ohr.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
run_once()
{
    output=$1
    instructions=$2
    shift 2
    if "$emulator" run --profile sapporo-2.35.34 --firmware "$manifest" \
        --layer sapporo-2.35-production-data --max-instructions "$instructions" \
        --max-time 5000000000 "$@" > "$output" 2>&1; then
        code=0
    else
        code=$?
    fi
    if [ "$code" -ne 3 ]; then cat "$output" >&2; exit 1; fi
}
for pass in 1 2; do
    run_once "$run_dir/enabled-$pass.log" 500000000 --layer sapporo-2.35-ohr-startup
    run_once "$run_dir/disabled-$pass.log" 280000000
done
cmp "$run_dir/enabled-1.log" "$run_dir/enabled-2.log"
cmp "$run_dir/disabled-1.log" "$run_dir/disabled-2.log"
[ "$(shasum -a 256 "$run_dir/enabled-1.log" | awk '{print $1}')" = \
    de26f3f13d8c1ccf2111224d56ad672fd1a6953640c046427d0765ba38a018b1 ]
[ "$(shasum -a 256 "$run_dir/disabled-1.log" | awk '{print $1}')" = \
    b452c5669e536c0227a43b319807ad7ccc6fc9fe49fddaba9967051d0f7bd879 ]
grep -Fqx 'stop=budget pc=0x0008ce2e instructions=500000000 virtual_time_ns=2719206417' "$run_dir/enabled-1.log"
grep -Fqx 'stop=budget pc=0x000a6bc8 instructions=280000000 virtual_time_ns=1590729375' "$run_dir/disabled-1.log"
[ "$(grep -c 'event=layer-hit layer=sapporo-2.35-ohr-startup' "$run_dir/enabled-1.log")" -eq 8 ]
[ "$(grep -c 'kind=response.*status=ok' "$run_dir/enabled-1.log")" -eq 8 ]
if grep -Eq 'machine-reset-request|status=refuse' "$run_dir/enabled-1.log"; then
    echo 'error: enabled OHR startup refused or reset' >&2
    exit 1
fi
[ "$(grep -c 'machine-reset-request' "$run_dir/disabled-1.log")" -eq 1 ]
echo 'PASS sapporo-2.35.34 explicit OHR startup; eight responses and disabled control'
