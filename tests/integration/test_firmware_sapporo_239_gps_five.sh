#!/bin/sh
# TEST_TAGS: sapporo_239_gps_five
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo five-pulse fixture: different profile"; exit 0
fi
manifest=${SEMU_FIRMWARE_MANIFEST-}
flash=${SEMU_SAPPORO_239_FULL_FLASH-}
emulator=${SEMU_EMULATOR-}
if [ -z "$manifest" ] || [ -z "$flash" ]; then
    echo "SKIP Sapporo five-pulse fixture: manifest and full flash required"; exit 0
fi
hash() { shasum -a 256 "$1" | awk '{print $1}'; }
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
test -r "$flash" && test "$(hash "$flash")" = "$flash_hash"
"$emulator" validate --profile sapporo-2.39.20 --firmware "$manifest"
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-five.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
${CC:-cc} -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices \
    tests/integration/sapporo_239_five_probe.c "$(dirname "$emulator")/libsemu.a" -o "$run_dir/probe"
run()
{
    gate_name=$1; gate_start=$2; gate_pin=$3; gate_branch=$4
    if ! "$run_dir/probe" "$manifest" "$flash" "$gate_start" "$gate_pin" \
        "$run_dir/$gate_name" "$gate_branch" \
        >"$run_dir/$gate_name.trace" 2>"$run_dir/$gate_name.log"; then
        cat "$run_dir/$gate_name.trace" "$run_dir/$gate_name.log" >&2; exit 1
    fi
}
cli()
{
    cli_name=$1; cli_limit=$2; shift 2
    cli_code=0
    "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$flash" --layer sapporo-2.39-synthetic-wbsto \
        --layer sapporo-2.39-gps-startup --layer sapporo-2.39-gps-reopen \
        --layer sapporo-2.39-gps-awake-five --max-instructions "$cli_limit" \
        --max-time 45000000000 --snapshot-save "$run_dir/$cli_name.sems" "$@" \
        >"$run_dir/$cli_name.log" 2>&1 || cli_code=$?
    if [ "$cli_code" -ne 3 ]; then
        cat "$run_dir/$cli_name.log" >&2
        echo "error: five-pulse checkpoint $cli_name returned $cli_code" >&2
        exit 1
    fi
}
# RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): the five-pulse lifecycle with zero
# input reaches the layer hit budget (maximum_hits=5, trigger
# gps-awake-pulse): five interventions are accepted at the awake hook
# pc 0x001291cc — hook instructions 846889602, 960806493, 1073726377,
# 1184805197, 1296811148 — and the sixth arrival at the same hook refuses
# at instruction 1409719577 / virtual time 38250007180 with
# '2.39 GPS awake lifecycle or hit budget refused'. The hook runs BEFORE
# the step retires, so the refused stop keeps the pc. The old cold prefix
# 6e670940… (cap 3960123530), the idle/middle branch expectations, the
# pending/irq/high/fallen phase loop and the eighteen middle-button edges
# all sat behind the refusal and are retired: nothing at or above
# instruction 1409719577 is reachable at any cap. The CXD5610 rise-latch
# law the five layer shares with the awake lane (fall allocated AT the
# rising deadline, 0 ns pin-high window, stage never 3) makes GPIO24=0 and
# driver mirror 0x100588a2=1 the only observable pulse states — no
# snapshot in the flow shows the pin high or fallen.
#
# The durable resume anchor is the refusal-state budget snapshot: the CLI
# cap ON the refusing instruction stops under the budget (pc 0x001291cc,
# 1409719577 retired, twice-identical) and serializes as 908714165f….
# Loading it and running re-fires the refusal at the same triple with no
# advance, zero frames and zero events (the hit counter is already at the
# budget); the terminal image is the refusal image 33e1dbdf…, identical to
# the image the full cold CLI run saves. The old middle-branch 137-line
# census is retired; the deep-run census below replaces it.
prefix_hash=908714165f3e73c84281ac5931fd0353942fd4584a488252b69004d4c8c5d56c
# The refusal image every terminal path saves: the refused stop at the
# same triple as the budget stop above.
refusal_hash=33e1dbdf858c2161382d9158db2d0519ec6af02f364ccb7abc4f87eeeb764b87
final_stop='stop=compat-refused pc=0x001291cc instructions=1409719577 virtual_time_ns=38250007180'
# RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): the probe cold prefix is the
# clean boot to the 700000000-instruction budget stop — the same era cap
# the general/personal gates re-pinned — saved at the exact count/time
# arrival (pc 0x000a81b0, virtual time 4823758494, observer 3 frames,
# crc 4979f432), snapshot 27625f12…, twice. The settings/personal save
# lifecycle is fully inside the pinned prefix (six logical opens/closes of
# the 1727-byte payload, 396 write chunks); no awake-five intervention
# line exists inside it: the earliest pulse is logged at time
# 10876190851, long after the prefix stop. The old idle '1 line' and
# middle '137 lines' expectations are retired.
for attempt in first second; do
    run "prefix-$attempt" cold - prefix
    test "$(hash "$run_dir/prefix-$attempt.prefix.sems")" = 27625f12a47b31bf2b7d63eef28cc91386da6845b0617238e9f5ccccce13e0fb
    test "$(grep -c 'operation=open path=settings/personal mode=2 ' "$run_dir/prefix-$attempt.log")" -eq 6
    test "$(grep -c 'operation=write path=settings/personal ' "$run_dir/prefix-$attempt.log")" -eq 396
    test "$(grep -c 'operation=close path=settings/personal result=1 size=1727 cursor=1727$' "$run_dir/prefix-$attempt.log")" -eq 6
    test "$(grep -c 'layer=sapporo-2.39-gps-awake-five' "$run_dir/prefix-$attempt.log")" -eq 0
    test "$(wc -l < "$run_dir/prefix-$attempt.log")" -eq 152668
done
cmp "$run_dir/prefix-first.trace" "$run_dir/prefix-second.trace"
cmp "$run_dir/prefix-first.log" "$run_dir/prefix-second.log"
# The durable resume anchor, twice, via the CLI refusal cap.
for attempt in first second; do
    cli refusal-state-$attempt 1409719577
    grep -F -x -q 'stop=budget pc=0x001291cc instructions=1409719577 virtual_time_ns=38250007180' "$run_dir/refusal-state-$attempt.log"
    test "$(hash "$run_dir/refusal-state-$attempt.sems")" = "$prefix_hash"
done
cmp "$run_dir/refusal-state-first.log" "$run_dir/refusal-state-second.log"
cmp "$run_dir/refusal-state-first.sems" "$run_dir/refusal-state-second.sems"
# The refusal state re-fires in-process: idle proves the identity (empty
# log, zero frames), refusal pins the exact terminal with the refusal
# detail; both save the loaded budget image under the prefix name, the
# refused stop's image under final/refused, unchanged across re-saves.
for branch in idle refusal; do
    for attempt in first second; do
        run "$branch-$attempt" "$run_dir/refusal-state-first.sems" "$prefix_hash" "$branch"
        test "$(hash "$run_dir/$branch-$attempt.prefix.sems")" = "$prefix_hash"
        test "$(hash "$run_dir/$branch-$attempt.final.sems")" = "$refusal_hash"
        cmp "$run_dir/$branch-$attempt.final.sems" "$run_dir/$branch-$attempt.refused.sems"
        test "$(wc -l < "$run_dir/$branch-$attempt.log")" -eq 0
    done
    cmp "$run_dir/$branch-first.trace" "$run_dir/$branch-second.trace"
    cmp "$run_dir/$branch-first.log" "$run_dir/$branch-second.log"
done
# CLI resume of the refusal state: the refusal re-fires as a stop line
# with zero events after it (the +1 law: the chunked CLI shows the
# refusal at the same triple), saving the refusal image.
for attempt in first second; do
    cli refusal-resume-$attempt 2000000000 --snapshot-load "$run_dir/refusal-state-first.sems"
    grep -F -x -q "$final_stop" "$run_dir/refusal-resume-$attempt.log"
    test "$(hash "$run_dir/refusal-resume-$attempt.sems")" = "$refusal_hash"
    test "$(wc -l < "$run_dir/refusal-resume-$attempt.log")" -eq 1
done
cmp "$run_dir/refusal-resume-first.log" "$run_dir/refusal-resume-second.log"
# Full cold run to refusal: the golden stop line, the five-pulse census,
# the settings/personal lifecycle inside the deep run, and the refusal
# image; twice-identical logs and snapshots.
for attempt in first second; do
    cli full-cold-$attempt 2000000000
    grep -F -x -q "$final_stop" "$run_dir/full-cold-$attempt.log"
    test "$(hash "$run_dir/full-cold-$attempt.sems")" = "$refusal_hash"
    test "$(grep -c 'trigger=gps-awake-pulse' "$run_dir/full-cold-$attempt.log")" -eq 5
    grep -q 'trigger=gps-awake-pulse ordinal=5 ' "$run_dir/full-cold-$attempt.log"
    test "$(grep -c 'layer=sapporo-2.39-gps-awake-five .*provenance=E-SAP-GPS-FIFTH-239-002$' "$run_dir/full-cold-$attempt.log")" -eq 5
    test "$(grep -c 'layer=sapporo-2.39-gps-startup trigger=' "$run_dir/full-cold-$attempt.log")" -eq 2
    test "$(grep -c 'layer=sapporo-2.39-gps-reopen trigger=' "$run_dir/full-cold-$attempt.log")" -eq 2
    test "$(grep -c 'path=settings/personal' "$run_dir/full-cold-$attempt.log")" -eq 408
    grep -q 'operation=close path=settings/personal result=1 size=1727 cursor=1727$' "$run_dir/full-cold-$attempt.log"
    test "$(wc -l < "$run_dir/full-cold-$attempt.log")" -eq 152674
done
cmp "$run_dir/full-cold-first.log" "$run_dir/full-cold-second.log"
cmp "$run_dir/full-cold-first.sems" "$run_dir/full-cold-second.sems"
# The five hook sites, CLI budget stops one instruction past each hook:
# the hook-return observable pc 0x001291ce (the refusal hook's sibling
# return), the twice-verified stop triple, the intervention census equal
# to the spec ordinal and the full-file twice-identity. Measured on the
# plain no-edge era (fresh cold, twice byte-identical): cap hook+1 shows
# exactly N awake-five lines with the newest ordinal N — the CLI chunk
# steps through the hook instruction and the intervention-hit line is
# logged before the budget stop is reported.
for spec in 1:846889603:10876190852 2:960806494:16350459382 \
    3:1073726378:21825199679 4:1184805198:27300294167 \
    5:1296811149:32775096970; do
    expected=$(echo "$spec" | cut -d: -f1); limit=$(echo "$spec" | cut -d: -f2)
    vtime=$(echo "$spec" | cut -d: -f3)
    pulse_name="pulse$expected"
    for attempt in first second; do
        cli "$pulse_name-$attempt" "$limit"
        grep -F -x -q "stop=budget pc=0x001291ce instructions=$limit virtual_time_ns=$vtime" "$run_dir/$pulse_name-$attempt.log"
        test "$(grep -c 'trigger=gps-awake-pulse' "$run_dir/$pulse_name-$attempt.log")" -eq "$expected"
        grep -q "trigger=gps-awake-pulse ordinal=$expected " "$run_dir/$pulse_name-$attempt.log"
    done
    cmp "$run_dir/$pulse_name-first.log" "$run_dir/$pulse_name-second.log"
    cmp "$run_dir/$pulse_name-first.sems" "$run_dir/$pulse_name-second.sems"
done
# Independently validate wrong inputs before execution.
if "$run_dir/probe" "$manifest" "$manifest" cold - "$run_dir/bad-flash" prefix \
    >"$run_dir/bad-flash.trace" 2>"$run_dir/bad-flash.log"; then exit 1; fi
grep -q 'private input hash mismatch' "$run_dir/bad-flash.log"
if "$run_dir/probe" "$manifest" "$flash" "$run_dir/refusal-state-first.sems" wrong \
    "$run_dir/bad-hash" idle >"$run_dir/bad-hash.trace" 2>"$run_dir/bad-hash.log"; then exit 1; fi
grep -q 'private input hash mismatch' "$run_dir/bad-hash.log"
manifest_dir=$(cd "$(dirname "$manifest")" && pwd)
ln -s "$manifest_dir" "$run_dir/components"
awk '
    /^\[component application\]$/ {application=1}
    /^\[component / && $0!="[component application]" {application=0}
    /^path=/ {$0="path=components/" substr($0,6)}
    application && /^sha256=/ {$0="sha256=0000000000000000000000000000000000000000000000000000000000000000"}
    {print}
' "$manifest" >"$run_dir/wrong.semu"
if "$run_dir/probe" "$run_dir/wrong.semu" "$flash" cold - "$run_dir/bad-component" prefix \
    >"$run_dir/bad-component.trace" 2>"$run_dir/bad-component.log"; then exit 1; fi
grep -q 'component application does not match profile' "$run_dir/bad-component.log"
# Registry and machine binding reject simultaneous variants before running.
if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --layer sapporo-2.39-gps-startup --layer sapporo-2.39-gps-reopen \
    --layer sapporo-2.39-gps-awake --layer sapporo-2.39-gps-awake-five \
    --max-instructions 1 --max-time 1 >"$run_dir/duplicate.log" 2>&1; then exit 1; fi
grep -q 'GPS layer dependency or ownership conflict' "$run_dir/duplicate.log"
test "$(hash "$flash")" = "$flash_hash"
echo "PASS Sapporo five-pulse fixture: cold prefix, refusal-state anchor, zero-event re-fire, five-pulse census and atomic refusals"
