#!/bin/sh
# TEST_TAGS: sapporo_235_snapshot
# Ticket 792: Sapporo 2.35 save/load. The 30M production boot checkpoint
# saves; the restored continuation reproduces the uninterrupted 80M
# suffix twice, byte-identically, with byte-identical saves.
# Ticket 799 / E-EMU-RENDERER-SNAPSHOT-002 (2026-10-02): paired
# codec-2 saves replace 8a28ce51... with cdf9f3ff...; boot and
# uninterrupted/restored continuation transcripts are unchanged.
set -eu
# Ticket 803 / E-EMU-SAP235-EXERCISE-003: Apollo4 codec 1 snapshot pins.
# Paired attribution changes only section 5 (+17 bytes); old logs are unchanged.
if [ "${TEST_PROFILE-}" != sapporo-2.35.34 ]; then
    echo "SKIP Sapporo 2.35 snapshot runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi
emulator=${SEMU_EMULATOR:?}
manifest=${SEMU_FIRMWARE_MANIFEST:?}
"$emulator" validate --profile sapporo-2.35.34 --firmware "$manifest"
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-235-snapshot.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

run_cold()
{
    output=$1
    instructions=$2
    time_ns=$3
    shift 3
    if "$emulator" run --profile sapporo-2.35.34 --firmware "$manifest" \
        --layer sapporo-2.35-production-data --max-instructions "$instructions" \
        --max-time "$time_ns" "$@" > "$output" 2>&1; then
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
    run_cold "$run_dir/boot-$pass.log" 30000000 1000000000 \
        --snapshot-save "$run_dir/snap-$pass.sems"
    run_cold "$run_dir/cont-$pass.log" 80000000 400000000 \
        --snapshot-load "$run_dir/snap-$pass.sems"
done
run_cold "$run_dir/control.log" 80000000 400000000

cmp "$run_dir/snap-1.sems" "$run_dir/snap-2.sems"
cmp "$run_dir/cont-1.log" "$run_dir/cont-2.log"
cmp "$run_dir/cont-1.log" "$run_dir/control.log"
[ "$(shasum -a 256 "$run_dir/snap-1.sems" | awk '{print $1}')" = \
    9008c521b2632689684ebc29577d1d1c4f0f1ee927d321727024af9da6aa8f1a ]
[ "$(shasum -a 256 "$run_dir/boot-1.log" | awk '{print $1}')" = \
    113607e088231e666455ab8e585af603b5fc2a4de7a571c93a23fa2c77ea35ba ]
[ "$(shasum -a 256 "$run_dir/cont-1.log" | awk '{print $1}')" = \
    0ab8519944dce5e574e8cbb5b16d06bd798872204c5f602958c3c0cafabd2d81 ]
grep -Fqx 'stop=budget pc=0x000932a8 instructions=30000000 virtual_time_ns=35339893' \
    "$run_dir/boot-1.log"
grep -Fqx 'stop=budget pc=0x000e1862 instructions=73528280 virtual_time_ns=494546055' \
    "$run_dir/cont-1.log"

# A truncated image must refuse without starting the guest.
size=$(wc -c < "$run_dir/snap-1.sems")
head -c $((size - 4)) "$run_dir/snap-1.sems" > "$run_dir/truncated.sems"
if "$emulator" run --profile sapporo-2.35.34 --firmware "$manifest" \
    --layer sapporo-2.35-production-data --max-instructions 1000 \
    --max-time 1000000 --snapshot-load "$run_dir/truncated.sems" \
    > "$run_dir/refused.log" 2>&1; then
    cat "$run_dir/refused.log" >&2
    exit 1
fi
grep -q "snapshot load" "$run_dir/refused.log"

echo "PASS sapporo-2.35.34 snapshot save/load; continuation reproduces the suffix twice"
