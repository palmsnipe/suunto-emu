#!/bin/sh
# TEST_TAGS: sapporo_239_gps_reopen
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 GPS reopen: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 GPS reopen: set SEMU_SAPPORO_239_FULL_FLASH"
    exit 0
fi
hash() { shasum -a 256 "$1" | awk '{print $1}'; }
if [ ! -r "$full_flash" ] || [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: Sapporo 2.39 full flash unavailable or hash mismatch" >&2
    exit 2
fi
"$emulator" validate --profile sapporo-2.39.20 --firmware "$manifest"
# The inspector validates all components/flash too and only reads guest state.
make build/tests/test_sapporo_239_gps_reopen_snapshot
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-gps.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
run()
{
    run_name=$1; run_limit=$2; shift 2
    code=0
    "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --layer sapporo-2.39-gps-startup --layer sapporo-2.39-gps-reopen --max-instructions "$run_limit" \
        --max-time 30000000000 --snapshot-save "$run_dir/$run_name.sems" "$@" \
        >"$run_dir/$run_name.log" 2>&1 || code=$?
    if [ "$code" -ne 3 ]; then
        cat "$run_dir/$run_name.log" >&2
        echo "error: GPS checkpoint $run_name returned $code" >&2
        exit 1
    fi
    build/tests/test_sapporo_239_gps_reopen_snapshot --inspect "$manifest" "$full_flash" \
        "$run_dir/$run_name.sems" >"$run_dir/$run_name.state"
}
for attempt in first second; do
    run "$attempt" 825147087
    grep -F -x -q 'stop=budget pc=0x00128ed8 instructions=825147087 virtual_time_ns=10875951888' "$run_dir/$attempt.log"
    grep -F -x -q 'pc=00128ed8 instructions=825147087 time=10875951888 callback=12 pending=10 retry=0 r2=12 hits=2 startup=1 reply=1 reopen=1 gsr=1 rx_events=0' "$run_dir/$attempt.state"
    test "$(grep -c 'layer=sapporo-2.39-gps-startup trigger=' "$run_dir/$attempt.log")" -eq 2
    test "$(grep -c 'layer=sapporo-2.39-gps-reopen trigger=' "$run_dir/$attempt.log")" -eq 2
    test "$(hash "$run_dir/$attempt.log")" = f63cab509a2da82a867580bf9eac35b3e764df53d08155bb11c47c6dfa328007
    test "$(hash "$run_dir/$attempt.sems")" = 0d3b67903adff5826de946b56738ce443dffa784410392363e1a7ddc0623c27d
done
cmp "$run_dir/first.log" "$run_dir/second.log"
cmp "$run_dir/first.sems" "$run_dir/second.sems"
awk '!/^stop=/' "$run_dir/first.log" >"$run_dir/full.events"

# Snapshot before reopen, each pending reply, and completed native state 10.
# Each resumes to the identical native state-12 image and full event log.
previous=
for spec in before:672045164 startup:672045165 gsr:676735188 state10:688672231; do
    name=${spec%:*}; limit=${spec#*:}
    if [ -z "$previous" ]; then
        run "$name" "$limit"
        awk '!/^stop=/' "$run_dir/$name.log" >"$run_dir/$name.prefix"
    else
        run "$name" "$limit" --snapshot-load "$run_dir/$previous.sems"
        awk '!/^stop=/' "$run_dir/$previous.prefix" "$run_dir/$name.log" >"$run_dir/$name.prefix"
    fi
    case "$name" in
        before) grep -q 'hits=2 startup=1 reply=1 reopen=0 gsr=0 rx_events=0$' "$run_dir/$name.state" ;;
        startup) grep -q 'hits=2 startup=1 reply=1 reopen=1 gsr=0 rx_events=1$' "$run_dir/$name.state" ;;
        gsr) grep -q 'hits=2 startup=1 reply=1 reopen=1 gsr=1 rx_events=1$' "$run_dir/$name.state" ;;
        state10) grep -q 'callback=10 pending=10 retry=0 r2=10 hits=2 startup=1 reply=1 reopen=1 gsr=1 rx_events=0$' "$run_dir/$name.state" ;;
    esac
    run "${name}_resumed" 825147087 --snapshot-load "$run_dir/$name.sems"
    cmp "$run_dir/first.sems" "$run_dir/${name}_resumed.sems"
    awk '!/^stop=/' "$run_dir/$name.prefix" "$run_dir/${name}_resumed.log" >"$run_dir/resumed.events"
    cmp "$run_dir/full.events" "$run_dir/resumed.events"
    previous=$name
done
# Receiver liveness remains unsupported: native recovery sends GSTP, whose
# final byte causes the unchanged precise UART fault, not an added response.
run gstp_tx 940946122 --snapshot-load "$run_dir/first.sems"
grep -F -x -q 'tx=GSTP' "$run_dir/gstp_tx.state"
run gstp_fault 940963736 --snapshot-load "$run_dir/gstp_tx.sems"
grep -F -x -q 'stop=budget pc=0x001c0db4 instructions=940963736 virtual_time_ns=16349008531' "$run_dir/gstp_fault.log"
grep -F -x -q 'fault_address=4001d000 stacked_pc=00171798' "$run_dir/gstp_fault.state"
grep -q 'hits=2 startup=1 reply=1 reopen=1 gsr=1 rx_events=0$' "$run_dir/gstp_fault.state"
if grep -q 'event=intervention-hit' "$run_dir/gstp_fault.log"; then exit 1; fi
# Explicit dependency, duplicates, and old layer sets must not silently migrate.
for selected in sapporo-2.39-gps-reopen sapporo-2.39-gps-startup; do
    code=0
    "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --layer "$selected" --max-instructions 1 --max-time 30000000000 \
        --snapshot-load "$run_dir/first.sems" >"$run_dir/mismatch.log" 2>&1 || code=$?
    test "$code" -ne 0
    grep -E -q 'GPS layer dependency or ownership conflict|snapshot layer set differs' "$run_dir/mismatch.log"
done
code=0
"$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --layer sapporo-2.39-gps-startup \
    --layer sapporo-2.39-gps-reopen --layer sapporo-2.39-gps-reopen \
    --max-instructions 1 --max-time 30000000000 >"$run_dir/duplicate.log" 2>&1 || code=$?
test "$code" -ne 0
grep -q 'GPS layer dependency or ownership conflict' "$run_dir/duplicate.log"
for old in one two; do
    set --
    if [ "$old" = two ]; then set -- --layer sapporo-2.39-gps-startup; fi
    code=0
    "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto "$@" \
        --max-instructions 1 --max-time 30000000000 \
        --snapshot-save "$run_dir/old.sems" >"$run_dir/old.log" 2>&1 || code=$?
    test "$code" -eq 3
    if build/tests/test_sapporo_239_gps_reopen_snapshot --inspect "$manifest" "$full_flash" \
        "$run_dir/old.sems" >"$run_dir/old-mismatch.log" 2>&1; then exit 1; fi
    grep -q 'snapshot layer set differs' "$run_dir/old-mismatch.log"
done
code=0
"$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
    --layer sapporo-2.39-gps-reopen --layer sapporo-2.39-gps-startup \
    --snapshot-load "$run_dir/first.sems" --max-instructions 1 \
    --max-time 30000000000 >"$run_dir/order.log" 2>&1 || code=$?
test "$code" -ne 0
grep -q 'snapshot layer identity differs' "$run_dir/order.log"
test "$(hash "$full_flash")" = "$flash_hash"
echo "PASS Sapporo 2.39 GPS reopen/GSR, four snapshot phases and precise later GSTP refusal"
