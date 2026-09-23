#!/bin/sh
# TEST_TAGS: sapporo_235_production
# E-SAP-0038/0039: production records and negative pressure probes advance startup.
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.35.34 ]; then
    echo "SKIP Sapporo 2.35 production runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi
emulator=${SEMU_EMULATOR:?}
manifest=${SEMU_FIRMWARE_MANIFEST:?}
"$emulator" validate --profile sapporo-2.35.34 --firmware "$manifest"
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-235-production.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
run_once()
{
    output=$1
    instructions=$2
    time_ns=$3
    if "$emulator" run --profile sapporo-2.35.34 --firmware "$manifest" \
        --layer sapporo-2.35-production-data --max-instructions "$instructions" \
        --max-time "$time_ns" > "$output" 2>&1; then
        code=0
    else
        code=$?
    fi
    if [ "$code" -ne 3 ]; then
        cat "$output" >&2
        exit 1
    fi
}
for pass in 1 2; do
    run_once "$run_dir/boot-$pass.log" 30000000 1000000000
    run_once "$run_dir/next-$pass.log" 80000000 400000000
done
cmp "$run_dir/boot-1.log" "$run_dir/boot-2.log"
cmp "$run_dir/next-1.log" "$run_dir/next-2.log"
[ "$(shasum -a 256 "$run_dir/boot-1.log" | awk '{print $1}')" = \
    113607e088231e666455ab8e585af603b5fc2a4de7a571c93a23fa2c77ea35ba ]
[ "$(shasum -a 256 "$run_dir/next-1.log" | awk '{print $1}')" = \
    0ab8519944dce5e574e8cbb5b16d06bd798872204c5f602958c3c0cafabd2d81 ]
grep -Fqx 'stop=budget pc=0x000932a8 instructions=30000000 virtual_time_ns=35339893' "$run_dir/boot-1.log"
grep -Fqx 'stop=budget pc=0x000e1862 instructions=73528280 virtual_time_ns=494546055' "$run_dir/next-1.log"
if grep -q 'machine-reset-request' "$run_dir/next-1.log"; then
    echo "error: 2.35 pressure startup reset regressed" >&2
    exit 1
fi
echo "PASS sapporo-2.35.34 production records; pressure probes pass without reset"
