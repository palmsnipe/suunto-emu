#!/bin/sh
# TEST_TAGS: sapporo_239_gps_startup
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 GPS startup: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 GPS startup: set SEMU_SAPPORO_239_FULL_FLASH"
    exit 0
fi
hash() { shasum -a 256 "$1" | awk '{print $1}'; }
if [ ! -r "$full_flash" ] || [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: Sapporo 2.39 full flash unavailable or hash mismatch" >&2
    exit 2
fi
"$emulator" validate --profile sapporo-2.39.20 --firmware "$manifest"
# The inspector validates all components/flash too and only reads guest state.
make build/tests/test_sapporo_239_gps_snapshot
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-gps.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
run()
{
    run_name=$1; run_limit=$2; shift 2
    code=0
    "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --layer sapporo-2.39-gps-startup --max-instructions "$run_limit" \
        --max-time 30000000000 --snapshot-save "$run_dir/$run_name.sems" "$@" \
        >"$run_dir/$run_name.log" 2>&1 || code=$?
    if [ "$code" -ne 3 ]; then
        cat "$run_dir/$run_name.log" >&2
        echo "error: GPS checkpoint $run_name returned $code" >&2
        exit 1
    fi
    build/tests/test_sapporo_239_gps_snapshot --inspect "$manifest" "$full_flash" \
        "$run_dir/$run_name.sems" >"$run_dir/$run_name.state"
}
for attempt in first second; do
    run "$attempt" 393785845
    grep -F -x -q 'stop=budget pc=0x00128ed8 instructions=393785845 virtual_time_ns=2564070074' "$run_dir/$attempt.log"
    grep -F -x -q 'pc=00128ed8 instructions=393785845 time=2564070074 callback=15 pending=14 retry=0 r2=15 hits=2 startup=1 reply=1 rx_events=0' "$run_dir/$attempt.state"
    test "$(grep -c 'layer=sapporo-2.39-gps-startup trigger=' "$run_dir/$attempt.log")" -eq 2
    test "$(hash "$run_dir/$attempt.log")" = b5b23c9f6a96d9ecfbf4f17aa4f3b70801d08cd9b9636330d11c08f2e0647123
    test "$(hash "$run_dir/$attempt.sems")" = bfce8efc3cf6fc28330eb81cf453aad2ff71a4c8f4c9d2102624bae0d937a6c9
done
cmp "$run_dir/first.log" "$run_dir/second.log"
cmp "$run_dir/first.sems" "$run_dir/second.sems"
awk '!/^stop=/' "$run_dir/first.log" >"$run_dir/full.events"

# Prefix snapshots cover unused, startup RX pending, version RX pending,
# and native state 14. Each resumes to the same state-15 snapshot/log suffix.
previous=
for spec in before:357031764 startup:357031765 version:376273275 state14:390944539; do
    name=${spec%:*}; limit=${spec#*:}
    if [ -z "$previous" ]; then
        run "$name" "$limit"
        awk '!/^stop=/' "$run_dir/$name.log" >"$run_dir/$name.prefix"
    else
        run "$name" "$limit" --snapshot-load "$run_dir/$previous.sems"
        awk '!/^stop=/' "$run_dir/$previous.prefix" "$run_dir/$name.log" >"$run_dir/$name.prefix"
    fi
    case "$name" in
        before) grep -q 'hits=0 startup=0 reply=0 rx_events=0$' "$run_dir/$name.state" ;;
        startup) grep -q 'hits=1 startup=1 reply=0 rx_events=1$' "$run_dir/$name.state" ;;
        version) grep -q 'hits=2 startup=1 reply=1 rx_events=1$' "$run_dir/$name.state" ;;
        state14) grep -q 'callback=14 pending=14 retry=0 r2=14 hits=2 startup=1 reply=1 rx_events=0$' "$run_dir/$name.state" ;;
    esac
    run "${name}_resumed" 393785845 --snapshot-load "$run_dir/$name.sems"
    cmp "$run_dir/first.sems" "$run_dir/${name}_resumed.sems"
    awk '!/^stop=/' "$run_dir/$name.prefix" "$run_dir/${name}_resumed.log" >"$run_dir/resumed.events"
    cmp "$run_dir/full.events" "$run_dir/resumed.events"
    previous=$name
done
# Completed exchange also resumes unchanged; later pending-seven is deliberately
# unsupported. Its timeout eventually retries the already-consumed startup hook.
run later 672044891 --snapshot-load "$run_dir/first.sems"
grep -q 'pending=7 retry=0 .*hits=2 startup=1 reply=1 rx_events=0$' "$run_dir/later.state"
run retry 908321039 --snapshot-load "$run_dir/later.sems"
grep -q 'pc=00128d14 .*retry=1 .*hits=2 startup=1 reply=1 rx_events=0$' "$run_dir/retry.state"
run direct_retry 908321039 --snapshot-load "$run_dir/first.sems"
cmp "$run_dir/retry.sems" "$run_dir/direct_retry.sems"
code=0
"$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
    --layer sapporo-2.39-gps-startup --max-instructions 908321040 \
    --max-time 30000000000 --snapshot-load "$run_dir/retry.sems" \
    >"$run_dir/refused.log" 2>&1 || code=$?
test "$code" -ne 0
grep -q 'stop=compat-refused pc=0x00128d14 instructions=908321039' "$run_dir/refused.log"
if grep -q 'event=intervention-hit' "$run_dir/refused.log"; then exit 1; fi
# Layer identity is not silently migrated from the older one-layer format.
code=0
"$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
    --max-instructions 1 --max-time 30000000000 \
    --snapshot-save "$run_dir/one-layer.sems" >"$run_dir/one-layer.log" 2>&1 || code=$?
test "$code" -eq 3
if build/tests/test_sapporo_239_gps_snapshot --inspect "$manifest" "$full_flash" \
    "$run_dir/one-layer.sems" >"$run_dir/mismatch.log" 2>&1; then exit 1; fi
grep -q 'snapshot layer set differs from machine' "$run_dir/mismatch.log"
test "$(hash "$full_flash")" = "$flash_hash"
echo "PASS Sapporo 2.39 GPS startup/version, four snapshot phases and later retry refusal"
