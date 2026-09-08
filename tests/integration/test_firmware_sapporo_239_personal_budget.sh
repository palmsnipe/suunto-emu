#!/bin/sh
# TEST_TAGS: sapporo_239_personal_budget
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 personal budget: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
if [ -z "$full_flash" ] || [ -z "$manifest" ]; then
    echo "SKIP Sapporo 2.39 personal budget: set manifest and SEMU_SAPPORO_239_FULL_FLASH"
    exit 0
fi
hash() { shasum -a 256 "$1" | awk '{print $1}'; }
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
if [ ! -r "$full_flash" ] || [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: Sapporo 2.39 full flash unavailable or hash mismatch" >&2
    exit 2
fi
"$emulator" validate --profile sapporo-2.39.20 --firmware "$manifest"
library=$(dirname "$emulator")/libsemu.a
test -r "$library"
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-personal.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
${CC:-cc} -std=c99 -Wall -Wextra -Werror -pedantic -O2 \
    -Iinclude -Isrc -Isrc/devices tests/integration/sapporo_239_personal_probe.c \
    "$library" -o "$run_dir/probe"
run()
{
    name=$1; start=$2; branch=${3:-full}
    if ! "$run_dir/probe" "$manifest" "$full_flash" "$start" "$run_dir/$name" "$branch" \
        >"$run_dir/$name.out" 2>"$run_dir/$name.log"; then
        cat "$run_dir/$name.out" "$run_dir/$name.log" >&2
        echo "error: personal-settings native gate failed: $name" >&2
        exit 1
    fi
}
# Generate the exact general-save midpoint from cold native execution.
run cold cold
test "$(hash "$run_dir/cold.prefix.sems")" = 76a7af5385eb2ddf5dfe94f6607db34e6e820edb06f5054a31bce7a5ef4ada66
for attempt in first second; do
    run "$attempt" "$run_dir/cold.prefix.sems"
    test "$(hash "$run_dir/$attempt.mid.sems")" = 68ab2fa1fda8596e1b51a6b3d83950363effb0598f1835506492d20636fad7d8
    test "$(hash "$run_dir/$attempt.final.sems")" = 3d19f80df2c47a1e98f6bfa8dd0640f0d6cbb38a4d06f933d16f09c2c54245c6
    test "$(hash "$run_dir/$attempt.log")" = 21a566167b8e34a1bf36e25feca4f1e337e8ca1f4d858822fe8e24661d3de139
    cmp "$run_dir/$attempt.final.sems" "$run_dir/$attempt.refused.sems"
    # 51 remaining prior general-save operations plus the new 228-operation suffix.
    test "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/$attempt.log")" -eq 279
    test "$(grep -c 'operation=write path=settings/personal ' "$run_dir/$attempt.log")" -eq 132
    test "$(grep -c 'operation=write path=settings/general ' "$run_dir/$attempt.log")" -eq 140
    grep -F -q 'trigger=logical-file ordinal=76599 ' "$run_dir/$attempt.log"
done
cmp "$run_dir/first.out" "$run_dir/second.out"
cmp "$run_dir/first.final.sems" "$run_dir/second.final.sems"
run resumed "$run_dir/first.mid.sems"
cmp "$run_dir/first.final.sems" "$run_dir/resumed.final.sems"
cmp "$run_dir/resumed.final.sems" "$run_dir/resumed.refused.sems"
test "$(hash "$run_dir/resumed.log")" = f287fc1e9276ea1540596234b8b65ae4d560fdda493ae7865bd9c3d928ae54bf
awk 'match($0,/time_ns=[0-9]+/) {t=substr($0,9,RLENGTH-8); if(t>=24380713255) print}' \
    "$run_dir/first.log" >"$run_dir/suffix.log"
cmp "$run_dir/suffix.log" "$run_dir/resumed.log"
for attempt in idle-first idle-second; do
    run "$attempt" "$run_dir/cold.prefix.sems" idle
    test "$(hash "$run_dir/$attempt.final.sems")" = 4faf5b8934c80cbadc33a7d6a389dd8f50a26bacdf2ed7208effd7a4abadb3e3
    test "$(hash "$run_dir/$attempt.log")" = 509437ffa701685958420794fdf70d24ef4704b2869c0f49fb3fc09f0130dfcf
    cmp "$run_dir/$attempt.final.sems" "$run_dir/$attempt.refused.sems"
done
cmp "$run_dir/idle-first.out" "$run_dir/idle-second.out"
run idle-resumed "$run_dir/first.mid.sems" idle
cmp "$run_dir/idle-first.final.sems" "$run_dir/idle-resumed.final.sems"
cmp "$run_dir/idle-resumed.final.sems" "$run_dir/idle-resumed.refused.sems"
test "$(hash "$run_dir/idle-resumed.log")" = f6d6c1bab95a4150129d65d917ddbeade37bd7b0647354ba6877d68ad2013bd3
# The helper independently checks firmware and full flash before execution.
if "$run_dir/probe" "$manifest" "$manifest" cold "$run_dir/bad-flash" full \
    >"$run_dir/bad-flash.out" 2>"$run_dir/bad-flash.log"; then exit 1; fi
grep -q 'private input hash mismatch' "$run_dir/bad-flash.log"
test ! -e "$run_dir/bad-flash.prefix.sems"
# Preserve component paths while corrupting only the application's hash pin.
manifest_dir=$(cd "$(dirname "$manifest")" && pwd)
ln -s "$manifest_dir" "$run_dir/components"
awk '
    /^\[component application\]$/ {application=1}
    /^\[component / && $0!="[component application]" {application=0}
    /^path=/ {$0="path=components/" substr($0,6)}
    application && /^sha256=/ {$0="sha256=0000000000000000000000000000000000000000000000000000000000000000"}
    {print}
' "$manifest" >"$run_dir/wrong.semu"
if "$run_dir/probe" "$run_dir/wrong.semu" "$full_flash" cold "$run_dir/bad-component" full \
    >"$run_dir/bad-component.out" 2>"$run_dir/bad-component.log"; then exit 1; fi
grep -F -q 'component application does not match profile' "$run_dir/bad-component.log"
test ! -e "$run_dir/bad-component.prefix.sems"
test "$(hash "$full_flash")" = "$flash_hash"
# A valid but wrong checkpoint must refuse before executing or saving.
if "$run_dir/probe" "$manifest" "$full_flash" "$run_dir/first.final.sems" \
    "$run_dir/bad-snapshot" full >"$run_dir/bad-snapshot.out" 2>"$run_dir/bad-snapshot.log"; then exit 1; fi
grep -F -q 'unexpected native start checkpoint' "$run_dir/bad-snapshot.log"
test ! -e "$run_dir/bad-snapshot.final.sems"
echo "PASS sapporo-2.39.20 personal saves: 228 operations, weight frame, repeat/resume, time-path and GPS refusals"
