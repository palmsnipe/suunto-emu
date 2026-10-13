#!/bin/sh
# TEST_TAGS: sapporo_233_files sapporo_233_pressure
# E-SAP-0072: the logical-file READ/WRITE ABI clears the settings assertion.
# E-SAP-0074: negative pressure probes complete; the later OHR/reset refuses.
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.33.16 ]; then
    echo "SKIP Sapporo 2.33 files runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi
emulator=${SEMU_EMULATOR:?}
manifest=${SEMU_FIRMWARE_MANIFEST:?}
"$emulator" validate --profile sapporo-2.33.16 --firmware "$manifest"
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-233-files.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
run_once()
{
    output=$1
    instructions=$2
    time_ns=$3
    shift 3
    if "$emulator" run --profile sapporo-2.33.16 --firmware "$manifest" \
        --max-instructions "$instructions" --max-time "$time_ns" \
        "$@" > "$output" 2>&1; then
        code=0
    else
        code=$?
    fi
    if [ "$code" -ne 3 ]; then
        cat "$output" >&2
        exit 1
    fi
}
staged_once()
{
    run_once "$1" "$2" "$3" \
        --layer sapporo-2.33-production-data \
        --layer sapporo-2.33-logical-files \
        --layer sapporo-2.33-firstboot \
        --layer sapporo-2.33-gps-startup
}
for pass in 1 2; do
    run_once "$run_dir/control-$pass.log" 200000000 200000000
    staged_once "$run_dir/settings-$pass.log" 70000000 1000000000
    staged_once "$run_dir/pressure-$pass.log" 200000000 1000000000
    staged_once "$run_dir/next-$pass.log" 300000000 30000000000
done
for stage in control settings pressure next; do
    cmp "$run_dir/$stage-1.log" "$run_dir/$stage-2.log"
done
[ "$(shasum -a 256 "$run_dir/control-1.log" | awk '{print $1}')" = \
    74c858187b0fcc1c4b33729b9df7194d57e851edf13c079391308c7169e67a33 ]
[ "$(shasum -a 256 "$run_dir/settings-1.log" | awk '{print $1}')" = \
    54047d5d75d07c6a611294fd057c474ec0cab1327245c8f268b388718c667b30 ]
# E-SAP-0074 retires E-SAP-0072's pressure-fault suffix (8a5e06b0...).
# Preserve both earlier prefix pins and pin success before the new OHR limit.
[ "$(shasum -a 256 "$run_dir/pressure-1.log" | awk '{print $1}')" = \
    026ed246926095f3efa15ef8485dfdc83a5402c6d77460b6f6d5fa5d41ced71b ]
[ "$(shasum -a 256 "$run_dir/next-1.log" | awk '{print $1}')" = \
    b75699ac3f4bbcd0f536cbaae0a910bd6519cda06bea81312ac25eef0b73dcef ]
grep -Fqx 'stop=budget pc=0x000dbc0a instructions=48412879 virtual_time_ns=339421948' "$run_dir/control-1.log"
grep -Fqx 'stop=budget pc=0x000c892e instructions=70000000 virtual_time_ns=405355276' "$run_dir/settings-1.log"
grep -Fq 'operation=close path=settings/uiv2.txt result=1 size=256 cursor=0' "$run_dir/settings-1.log"
grep -Fq 'operation=close path=settings/general result=1 size=1487 cursor=0' "$run_dir/settings-1.log"
grep -Fqx 'stop=budget pc=0x000dbc0a instructions=93149169 virtual_time_ns=1000000000' "$run_dir/pressure-1.log"
for stage in settings pressure; do
    if grep -Eq 'wui_dump.bin|machine-reset-request' "$run_dir/$stage-1.log"; then
        echo "error: 2.33 $stage assertion/reset regressed" >&2
        exit 1
    fi
done
grep -Fq 'kind=request command=0x0010 sequence=0 state=BSL status=refuse ready=0' "$run_dir/next-1.log"
grep -Fq 'fault_address=0x40052120' "$run_dir/next-1.log"
grep -Fqx 'stop=device-refused pc=0x000c97f2 instructions=525319888 virtual_time_ns=1800546345 detail=2.33 ui.txt seed already populated' "$run_dir/next-1.log"
echo 'PASS sapporo-2.33.16 native settings and negative pressure probes; bounded OHR/reset refusal'
