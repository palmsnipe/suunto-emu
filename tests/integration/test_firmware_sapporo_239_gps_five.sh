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
    name=$1; start=$2; pin=$3; branch=$4
    if ! "$run_dir/probe" "$manifest" "$flash" "$start" "$pin" "$run_dir/$name" "$branch" \
        >"$run_dir/$name.trace" 2>"$run_dir/$name.log"; then
        cat "$run_dir/$name.trace" "$run_dir/$name.log" >&2; exit 1
    fi
}
run cold cold - prefix
prefix_hash=6e67094057d9510874a3766ced6372066fc134c1014840a0908a990266afa6de
test "$(hash "$run_dir/cold.prefix.sems")" = "$prefix_hash"
for branch in idle middle; do
    expected=d5244833987e6801192af15f0c57077eba2d23329fbaa4e09a6ccc4ec5b455c1
    lines=1
    if [ "$branch" = middle ]; then
        expected=41c65d4626481ddd0da99babfd48aff7c2d930e1864180b2eb64364f060043fa
        lines=137
    fi
    for attempt in first second; do
        run "$branch-$attempt" "$run_dir/cold.prefix.sems" "$prefix_hash" "$branch"
        test "$(hash "$run_dir/$branch-$attempt.final.sems")" = "$expected"
        cmp "$run_dir/$branch-$attempt.final.sems" "$run_dir/$branch-$attempt.refused.sems"
        test "$(grep -c 'trigger=gps-awake-pulse ordinal=5 ' "$run_dir/$branch-$attempt.log")" -eq 1
        test "$(wc -l < "$run_dir/$branch-$attempt.log")" -eq "$lines"
        grep -q 'layer=sapporo-2.39-gps-awake-five .*provenance=E-SAP-GPS-FIFTH-239-002$' "$run_dir/$branch-$attempt.log"
        if [ "$branch" = middle ]; then
            test "$(grep -c 'operation=open path=settings/personal mode=2 ' "$run_dir/$branch-$attempt.log")" -eq 1
            test "$(grep -c 'operation=write path=settings/personal ' "$run_dir/$branch-$attempt.log")" -eq 66
            grep -q 'operation=close path=settings/personal result=1 size=1727 cursor=1727$' "$run_dir/$branch-$attempt.log"
            grep -q 'trigger=logical-file ordinal=76667 ' "$run_dir/$branch-$attempt.log"
        fi
    done
    cmp "$run_dir/$branch-first.trace" "$run_dir/$branch-second.trace"
    cmp "$run_dir/$branch-first.log" "$run_dir/$branch-second.log"
done
for phase in pending irq high fallen; do
    start="$run_dir/idle-first.$phase.sems"
    run "resume-$phase" "$start" "$(hash "$start")" idle
    cmp "$run_dir/idle-first.final.sems" "$run_dir/resume-$phase.final.sems"
    cmp "$run_dir/resume-$phase.final.sems" "$run_dir/resume-$phase.refused.sems"
    test ! -s "$run_dir/resume-$phase.log"
done
# Independently validate wrong inputs before execution.
if "$run_dir/probe" "$manifest" "$manifest" cold - "$run_dir/bad-flash" prefix \
    >"$run_dir/bad-flash.trace" 2>"$run_dir/bad-flash.log"; then exit 1; fi
grep -q 'private input hash mismatch' "$run_dir/bad-flash.log"
if "$run_dir/probe" "$manifest" "$flash" "$run_dir/cold.prefix.sems" wrong \
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
echo "PASS Sapporo five-pulse fixture: cold startup, HEIGHT, repeat/resume, exact diagnostic state and atomic refusals"
