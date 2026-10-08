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
# RE-PINNED (777/E-SAP239-DEEPCLEAN-001): the storage-law corrections (concurrent
# 64-handle recycling and fixed-table runtime capacity 1037) removed the old
# wall, so every instruction, virtual-time and PC anchor below the deep-boot
# wall moved. The native state-12 monitor still arms at the same guest place,
# but the observable stop lands at pc=0x000a7aa2 / virtual_time_ns=9904990615.
# r2 is no longer the state constant 12: it now holds the driver pointer
# 268573820 (0x100368BC), so it is pinned as the observed pointer value. The
# startup and reopen trigger counts stay 2 and 2.
for attempt in first second; do
    run "$attempt" 825147087
    grep -F -x -q 'stop=budget pc=0x000a7aa2 instructions=825147087 virtual_time_ns=9904990615' "$run_dir/$attempt.log"
    grep -F -x -q 'pc=000a7aa2 instructions=825147087 time=9904990615 callback=12 pending=10 retry=0 r2=268573820 hits=2 startup=1 reply=1 reopen=1 gsr=1 rx_events=0' "$run_dir/$attempt.state"
    test "$(grep -c 'layer=sapporo-2.39-gps-startup trigger=' "$run_dir/$attempt.log")" -eq 2
    test "$(grep -c 'layer=sapporo-2.39-gps-reopen trigger=' "$run_dir/$attempt.log")" -eq 2
    test "$(hash "$run_dir/$attempt.log")" = d24282dacdfe4bebade0faa569e3a7dd6ef98292a0fcc2f86eca107772255c46
    test "$(hash "$run_dir/$attempt.sems")" = 3ffb9eefefe78b5ad5f165f90c837464b8c57d4b1890e313fadc7e812cbeab7a
done
cmp "$run_dir/first.log" "$run_dir/second.log"
cmp "$run_dir/first.sems" "$run_dir/second.sems"
awk '!/^stop=/' "$run_dir/first.log" >"$run_dir/full.events"

# Snapshot before reopen, each pending reply, and completed native state 10.
# Each resumes to the identical native state-12 image and full event log.
# RE-PINNED (777/E-SAP239-DEEPCLEAN-001): all four caps re-derived by bisection.
# The trigger line logs at t=T while the cap T+1 snapshot is the first image
# that shows the new state, so each cap is the FIRST cap where its predicate
# holds and the "before" cap is the one immediately below that boundary.
# The non-anchored suffix greps below are now cap-anchored: layers[1].hits is a
# layer TOTAL that saturates at 2 long before the reopen boundary, so the old
# unanchored 'hits=2 startup=1 ...' patterns had become vacuously true.
previous=
for spec in before:693761394 startup:693761395 gsr:699391326 state10:704931712; do
    name=${spec%:*}; limit=${spec#*:}
    if [ -z "$previous" ]; then
        run "$name" "$limit"
        awk '!/^stop=/' "$run_dir/$name.log" >"$run_dir/$name.prefix"
    else
        run "$name" "$limit" --snapshot-load "$run_dir/$previous.sems"
        awk '!/^stop=/' "$run_dir/$previous.prefix" "$run_dir/$name.log" >"$run_dir/$name.prefix"
    fi
    case "$name" in
        before) grep -q 'instructions=693761394 time=4758447100 callback=4 pending=7 retry=0 r2=14841 hits=2 startup=1 reply=1 reopen=0 gsr=0 rx_events=0$' "$run_dir/$name.state" ;;
        startup) grep -q 'instructions=693761395 time=4758447101 callback=4 pending=7 retry=0 r2=14841 hits=2 startup=1 reply=1 reopen=1 gsr=0 rx_events=1$' "$run_dir/$name.state" ;;
        gsr) grep -q 'instructions=699391326 time=4797221169 callback=9 pending=10 retry=0 r2=6 hits=2 startup=1 reply=1 reopen=1 gsr=1 rx_events=1$' "$run_dir/$name.state" ;;
        # RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): the old predicate demanded
        # r2=10. r2 is now a pointer register in this handler, so r2=10 never
        # holds at any cap. The pin below is the ACTUAL observed state at the
        # first cap where callback=10 pending=10 retry=0 (native state ten,
        # retry zero) with the reopen/GSR exchanges already retired.
        state10) grep -q 'instructions=704931712 time=5395186630 callback=10 pending=10 retry=0 r2=200 hits=2 startup=1 reply=1 reopen=1 gsr=1 rx_events=0$' "$run_dir/$name.state" ;;
    esac
    run "${name}_resumed" 825147087 --snapshot-load "$run_dir/$name.sems"
    cmp "$run_dir/first.sems" "$run_dir/${name}_resumed.sems"
    awk '!/^stop=/' "$run_dir/$name.prefix" "$run_dir/${name}_resumed.log" >"$run_dir/resumed.events"
    cmp "$run_dir/full.events" "$run_dir/resumed.events"
    previous=$name
done
# RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): the receiver-liveness GSTP boundary
# is unreachable at any cap the reopened run can now reach. Under the old law
# the deep boot stopped before the liveness branch, so the pinned observation
# was the refused one-use @GSTP exchange: budget stop at the fault vector
# pc=0x001c0db4 with the inspector line 'fault_address=4001d000
# stacked_pc=00171798'. With the wall removed the boot continues clean: the
# 0x4001d000 UART-DR fault is still taken, but the guest now escalates it to
# the SYSRESETREQ chain at pc=0x000d2f6e (lr=0xffffffed), which E-SAP239-
# DEEPCLEAN-001 (3) models as the logged machine-reset-request that now carries
# fault_address=. Two boundaries were bisected: instruction 959951554 is the
# last clean cap (pc=0x000d2f6c, no reset line); instruction 959951555 is the
# FIRST cap whose log carries the reset-request line, the same law as the
# trigger-line/state-1 boundary. Because pc is an exactly-equal gate, a budget
# stop can only ever show the PC at that one instruction, and a sweep of every
# cap from 825147087 to 959951554 (68 probes at 2M spacing plus 1M/0.5M/0.2M
# spot checks) plus a 100K resume scan over the old 940946122 window shows no
# cap with pc=0x00128c34 or pc=0x001c0db4 and zero refusals. The tx=GSTP and
# fault_address=/stacked_pc= inspector lines are therefore dead at every cap
# below the reset, and demanding them here would require an engine change.
run clean_pre_reset 959951554
grep -F -x -q 'stop=budget pc=0x000d2f6c instructions=959951554 virtual_time_ns=16350798184' "$run_dir/clean_pre_reset.log"
grep -F -x -q 'pc=000d2f6c instructions=959951554 time=16350798184 callback=12 pending=10 retry=0 r2=100270084 hits=2 startup=1 reply=1 reopen=1 gsr=1 rx_events=0' "$run_dir/clean_pre_reset.state"
# Receiver liveness is still unsupported: the reopened run adds no GSTP reply
# and no awake signal, so the boot ends in the reset rather than in recovery.
# Across the whole clean deep run both GPS layers retire exactly two triggers
# each, and the event stream the cold run and this deeper run share is
# byte-identical (only the stop line differs).
test "$(grep -c 'layer=sapporo-2.39-gps-startup trigger=' "$run_dir/first.log")" -eq 2
test "$(grep -c 'layer=sapporo-2.39-gps-reopen trigger=' "$run_dir/first.log")" -eq 2
# The deep run's own log is the same cold stream plus the deeper tail, so it
# still carries exactly the same two triggers per layer and nothing new.
test "$(grep -c 'layer=sapporo-2.39-gps-startup trigger=' "$run_dir/clean_pre_reset.log")" -eq 2
test "$(grep -c 'layer=sapporo-2.39-gps-reopen trigger=' "$run_dir/clean_pre_reset.log")" -eq 2
awk '!/^stop=/' "$run_dir/first.log" >"$run_dir/deep.prefix"
cmp "$run_dir/full.events" "$run_dir/deep.prefix"
if grep -F -q 'tx=GSTP' "$run_dir/clean_pre_reset.state"; then exit 1; fi
if grep -F -q 'fault_address=' "$run_dir/clean_pre_reset.state"; then exit 1; fi
if grep -q 'machine-reset-request' "$run_dir/clean_pre_reset.log"; then exit 1; fi
# The machine-reset-request boundary: the first cap whose log carries the line,
# with the same 0x4001d000 UART DR fault_address the old fault block pinned.
run machine_reset 959951555
grep -F -x -q 'stop=budget pc=0x001c4fb6 instructions=959951555 virtual_time_ns=16350798185' "$run_dir/machine_reset.log"
test "$(grep -c 'machine-reset-request' "$run_dir/machine_reset.log")" -eq 1
grep -F -x -q 'time_ns=16350798185 level=warning subsystem=cpu event=machine-reset-request pc=0x000d2f6e lr=0xffffffed sp=0x1005ff58 r0=0x05fa0004 r1=0xe000ed0c r2=0x05fa0004 r3=0x69000200 xpsr=0x29000003 reset_count=1 compat_hits=76316 instructions=959951555 virtual_time_ns=16350798185 fault_address=0x4001d000' "$run_dir/machine_reset.log"
# Post-reset image: the machine restart clears the layer hit counters to zero,
# so hits/startup/reply/reopen/gsr read 0 while the guest callbacks are still
# in the re-entered boot path.
grep -F -x -q 'pc=001c4fb6 instructions=959951555 time=16350798185 callback=12 pending=10 retry=0 r2=0 hits=0 startup=0 reply=0 reopen=0 gsr=0 rx_events=0' "$run_dir/machine_reset.state"
if grep -q 'stop=compat-refused' "$run_dir/machine_reset.log"; then exit 1; fi
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
echo "PASS Sapporo 2.39 GPS reopen/GSR, four snapshot phases and the deep UART fault escalated to the machine reset"
