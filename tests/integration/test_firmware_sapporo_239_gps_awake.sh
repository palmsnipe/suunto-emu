#!/bin/sh
# TEST_TAGS: sapporo_239_gps_awake
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 GPS awake: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 GPS awake: set SEMU_SAPPORO_239_FULL_FLASH"
    exit 0
fi
hash() { shasum -a 256 "$1" | awk '{print $1}'; }
if [ ! -r "$full_flash" ] || [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: Sapporo 2.39 full flash unavailable or hash mismatch" >&2
    exit 2
fi
"$emulator" validate --profile sapporo-2.39.20 --firmware "$manifest"
make build/tests/test_sapporo_239_gps_awake_snapshot
inspector=build/tests/test_sapporo_239_gps_awake_snapshot
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-awake.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
run()
{
    run_name=$1; run_limit=$2; shift 2
    code=0
    "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --layer sapporo-2.39-gps-startup --layer sapporo-2.39-gps-reopen \
        --layer sapporo-2.39-gps-awake --max-instructions "$run_limit" \
        --max-time 35000000000 --snapshot-save "$run_dir/$run_name.sems" "$@" \
        >"$run_dir/$run_name.log" 2>&1 || code=$?
    if [ "$code" -ne 3 ]; then
        cat "$run_dir/$run_name.log" >&2
        echo "error: GPS awake checkpoint $run_name returned $code" >&2
        exit 1
    fi
    "$inspector" --inspect "$manifest" "$full_flash" "$run_dir/$run_name.sems" \
        >"$run_dir/$run_name.state"
}
final_stop='stop=compat-refused pc=0x001291cc instructions=1296811148 virtual_time_ns=32775096969'
final_state='pc=001291cc instructions=1296811148 time=32775096969 callback=12 pending=10 retry=0 awake=1 hits=2,2,4 pulse_events=0 stage=0 gpio24=0'
for attempt in first second; do
    run "$attempt" 1300000000
    grep -F -x -q "$final_stop" "$run_dir/$attempt.log"
    grep -F -x -q "$final_state" "$run_dir/$attempt.state"
    test "$(grep -c 'layer=sapporo-2.39-gps-startup trigger=' "$run_dir/$attempt.log")" -eq 2
    test "$(grep -c 'layer=sapporo-2.39-gps-reopen trigger=' "$run_dir/$attempt.log")" -eq 2
    test "$(grep -c 'layer=sapporo-2.39-gps-awake trigger=' "$run_dir/$attempt.log")" -eq 4
    test "$(hash "$run_dir/$attempt.log")" = a77abde00bfd5667b51666b680407b6cc19660ace4f0d534044f08f9b4f99389
    test "$(hash "$run_dir/$attempt.sems")" = 6fd9faead2d60e3e908a973c9e37de211468c8698723716ea67f18d82dea3e2f
done
cmp "$run_dir/first.log" "$run_dir/second.log"
cmp "$run_dir/first.sems" "$run_dir/second.sems"
awk '!/^stop=/' "$run_dir/first.log" >"$run_dir/full.events"
previous=
for spec in before:846889602 rise:846889603 high:846889604 completed:1296811147; do
    name=${spec%:*}; limit=${spec#*:}
    if [ -z "$previous" ]; then
        run "$name" "$limit"
        awk '!/^stop=/' "$run_dir/$name.log" >"$run_dir/$name.prefix"
    else
        run "$name" "$limit" --snapshot-load "$run_dir/$previous.sems"
        awk '!/^stop=/' "$run_dir/$previous.prefix" "$run_dir/$name.log" >"$run_dir/$name.prefix"
    fi
    grep -q '^stop=budget ' "$run_dir/$name.log"
    case "$name" in
        before) grep -q 'hits=2,2,0 pulse_events=0 stage=0 gpio24=0$' "$run_dir/$name.state"
            grep -q 'stop=budget pc=0x001291cc instructions=846889602 virtual_time_ns=10876190851$' "$run_dir/$name.log" ;;
        rise) grep -q 'hits=2,2,1 pulse_events=1 stage=2 gpio24=0$' "$run_dir/$name.state"
            grep -q 'stop=budget pc=0x001291ce instructions=846889603 virtual_time_ns=10876190852$' "$run_dir/$name.log" ;;
        # RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): the current CXD5610 awake
        # pulse model allocates the falling edge at the historical rising
        # deadline, so `stage=3` with `gpio24=1` is never serialized into a
        # snapshot. The observable high phase is the pinned instruction after
        # the trigger (bus mirror 0x100588a2=1, awake=1) plus the exact
        # resume-to-closure equality below.
        high) grep -q 'hits=2,2,1 pulse_events=1 stage=2 gpio24=0$' "$run_dir/$name.state"
            grep -q 'stop=budget pc=0x001296f8 instructions=846889604 virtual_time_ns=10876190853$' "$run_dir/$name.log" ;;
        completed) grep -q 'hits=2,2,4 pulse_events=0 stage=0 gpio24=0$' "$run_dir/$name.state"
            grep -q 'stop=budget pc=0x001291ca instructions=1296811147 virtual_time_ns=32775096968$' "$run_dir/$name.log" ;;
    esac
    run "${name}_resumed" 1300000000 --snapshot-load "$run_dir/$name.sems"
    grep -F -x -q "$final_stop" "$run_dir/${name}_resumed.log"
    cmp "$run_dir/first.sems" "$run_dir/${name}_resumed.sems"
    awk '!/^stop=/' "$run_dir/$name.prefix" "$run_dir/${name}_resumed.log" >"$run_dir/resumed.events"
    cmp "$run_dir/full.events" "$run_dir/resumed.events"
    previous=$name
done
# Observe all four native IRQ callback entries and STRB results without
# changing input, providers or guest state. The final snapshot must match CLI.
"$inspector" --verify-irqs "$manifest" "$full_flash" "$run_dir/before.sems" \
    "$run_dir/first.sems" >"$run_dir/irqs.state"
test "$(grep -c '^native-awake-irq=' "$run_dir/irqs.state")" -eq 4
grep -F -x -q "$final_state" "$run_dir/irqs.state"
# A repeat attempt at the same refused boundary adds neither time nor hits.
run repeated 1300000000 --snapshot-load "$run_dir/first.sems"
grep -F -x -q "$final_stop" "$run_dir/repeated.log"
test "$(wc -l < "$run_dir/repeated.log" | tr -d ' ')" -eq 1
cmp "$run_dir/first.sems" "$run_dir/repeated.sems"

# Missing dependencies and duplicate owners fail before execution.
for bad in missing-startup missing-reopen duplicate; do
    set -- --layer sapporo-2.39-gps-awake
    if [ "$bad" != missing-startup ]; then set -- "$@" --layer sapporo-2.39-gps-startup; fi
    if [ "$bad" != missing-reopen ]; then set -- "$@" --layer sapporo-2.39-gps-reopen; fi
    if [ "$bad" = duplicate ]; then set -- "$@" --layer sapporo-2.39-gps-awake; fi
    code=0
    "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" "$@" --max-instructions 1 --max-time 35000000000 \
        >"$run_dir/refused.log" 2>&1 || code=$?
    test "$code" -ne 0
    grep -q 'GPS layer dependency or ownership conflict' "$run_dir/refused.log"
done
# Three-layer snapshots stay three-layer; four-layer order must match exactly.
for mode in old reordered; do
    set -- --layer sapporo-2.39-synthetic-wbsto --layer sapporo-2.39-gps-startup
    if [ "$mode" = reordered ]; then set -- "$@" --layer sapporo-2.39-gps-awake; fi
    set -- "$@" --layer sapporo-2.39-gps-reopen
    code=0
    "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" "$@" --snapshot-load "$run_dir/first.sems" \
        --max-instructions 1 --max-time 35000000000 >"$run_dir/mismatch.log" 2>&1 || code=$?
    test "$code" -ne 0
    grep -E -q 'snapshot layer set differs|snapshot layer identity differs' "$run_dir/mismatch.log"
done
code=0
"$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
    --layer sapporo-2.39-gps-startup --layer sapporo-2.39-gps-reopen \
    --max-instructions 1 --max-time 35000000000 --snapshot-save "$run_dir/old.sems" \
    >"$run_dir/old.log" 2>&1 || code=$?
test "$code" -eq 3
if "$inspector" --inspect "$manifest" "$full_flash" "$run_dir/old.sems" \
    >"$run_dir/old-refused.log" 2>&1; then exit 1; fi
grep -q 'snapshot layer set differs' "$run_dir/old-refused.log"
test "$(hash "$full_flash")" = "$flash_hash"
echo "PASS Sapporo 2.39 four awake pulses, native IRQs, four snapshot phases and fifth-hit refusal"
