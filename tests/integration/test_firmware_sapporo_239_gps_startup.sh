#!/bin/sh
# TEST_TAGS: sapporo_239_gps_startup
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 GPS startup: different profile"
    exit 0
fi
# Anchor law after E-SAP239-DEEPCLEAN-001: handle recycling (64 concurrent,
# not lifetime) plus fixed-table runtime capacity 1037 changed every guest
# instruction/time anchor below the old wall. The 393785845 cold cap is
# stable by construction (a budget stop halts before the next retire), but
# its pc/time and hashes moved: stop pc=0x000a7b32, time 2513930075,
# callback/r2 14/0 (state 15 is now reached at a deeper pc). Cold anchors and
# the whole prefix/later/retry chain were re-derived by ±1 first-cap bisection
# on the current engine (observer sweep hints re-verified byte-identically,
# twice each, before pinning).
# RE-SCOPED (777/E-SAP239-DEEPCLEAN-001) in this pass:
# - the refusal step exits rc=3 (stop=compat-refused is recorded as the stop
#   reason; the run is still an explicit stop) and the stop line carries a
#   "detail=2.39 GPS startup lifecycle or hit budget refused" suffix, so the
#   step asserts its exact full line instead of "rc != 0".
# - the refusal only fires one instruction past the wall: the refused hook
#   executes at guest instruction 932183288 while the budget check consumes
#   that retire, so the step cap is now wall+1 = 932183289 (was retry_cap+1).
# - the pending-seven checkpoint cap moved 672044891 -> 693761069 (first cap
#   where pending=7 holds; predicate text unchanged).
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
step()
{
    # Load-only refused step: no snapshot is produced, so no checkpoint exit
    # contract applies; the caller asserts the exact stop line and rc.
    step_name=$1; step_limit=$2; shift 2
    step_code=0
    "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --layer sapporo-2.39-gps-startup --max-instructions "$step_limit" \
        --max-time 30000000000 "$@" \
        >"$run_dir/$step_name.log" 2>&1 || step_code=$?
}
for attempt in first second; do
    run "$attempt" 393785845
    grep -F -x -q 'stop=budget pc=0x000a7b32 instructions=393785845 virtual_time_ns=2513930075' "$run_dir/$attempt.log"
    grep -F -x -q 'pc=000a7b32 instructions=393785845 time=2513930075 callback=14 pending=14 retry=0 r2=0 hits=2 startup=1 reply=1 rx_events=0' "$run_dir/$attempt.state"
    test "$(grep -c 'layer=sapporo-2.39-gps-startup trigger=' "$run_dir/$attempt.log")" -eq 2
    test "$(hash "$run_dir/$attempt.log")" = 2f4b37b6ed3323dedde1a40feef5601d7b12c3fc16d169abb3b91a59287217f8
    test "$(hash "$run_dir/$attempt.sems")" = 675f54f7a5e36f7c2140fef18f731b825d98b6163193c9db0138d86ff5299817
done
cmp "$run_dir/first.log" "$run_dir/second.log"
cmp "$run_dir/first.sems" "$run_dir/second.sems"
awk '!/^stop=/' "$run_dir/first.log" >"$run_dir/full.events"

# Prefix snapshots cover unused, startup RX pending, version RX pending,
# and native state 14. Each resumes to the same state-15 snapshot/log suffix.
# Caps are the FIRST cap where each predicate holds (±1 bisected); before is
# the startup boundary minus one.
previous=
for spec in before:359381678 startup:359381679 version:378623137 state14:391890495; do
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
        state14) grep -q 'callback=14 pending=14 retry=0 r2=0 hits=2 startup=1 reply=1 rx_events=0$' "$run_dir/$name.state" ;;
    esac
    run "${name}_resumed" 393785845 --snapshot-load "$run_dir/$name.sems"
    cmp "$run_dir/first.sems" "$run_dir/${name}_resumed.sems"
    awk '!/^stop=/' "$run_dir/$name.prefix" "$run_dir/${name}_resumed.log" >"$run_dir/resumed.events"
    cmp "$run_dir/full.events" "$run_dir/resumed.events"
    previous=$name
done
# Completed exchange also resumes unchanged; later pending-seven is deliberately
# unsupported. Its timeout eventually retries the already-consumed startup hook.
run later 693761069 --snapshot-load "$run_dir/first.sems"
grep -q 'pending=7 retry=0 .*hits=2 startup=1 reply=1 rx_events=0$' "$run_dir/later.state"
run retry 929400950 --snapshot-load "$run_dir/later.sems"
grep -q 'pc=00128f3a .*retry=1 .*hits=2 startup=1 reply=1 rx_events=0$' "$run_dir/retry.state"
run direct_retry 929400950 --snapshot-load "$run_dir/first.sems"
cmp "$run_dir/retry.sems" "$run_dir/direct_retry.sems"
# The refused hook executes at guest instruction 932183288: the budget stop at
# that cap retires without invoking the hook, so the wall only fires one
# instruction later. RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): stop line carries
# the refusal detail suffix and rc is 3 (budget-style explicit stop).
step refused 932183289 --snapshot-load "$run_dir/retry.sems"
test "$step_code" -eq 3
grep -F -x -q 'stop=compat-refused pc=0x00128d14 instructions=932183288 virtual_time_ns=14976272753 detail=2.39 GPS startup lifecycle or hit budget refused' "$run_dir/refused.log"
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
