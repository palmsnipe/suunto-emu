#!/bin/sh
# TEST_TAGS: sapporo_239_general_budget
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 general budget: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
if [ -z "$full_flash" ] || [ -z "$manifest" ]; then
    echo "SKIP Sapporo 2.39 general budget: set manifest and SEMU_SAPPORO_239_FULL_FLASH"
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
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-general.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
${CC:-cc} -std=c99 -Wall -Wextra -Werror -pedantic -O2 \
    -Iinclude -Isrc -Isrc/devices tests/integration/sapporo_239_general_probe.c \
    "$library" -o "$run_dir/probe"
run()
{
    name=$1; start=$2
    if ! "$run_dir/probe" "$manifest" "$full_flash" "$start" "$run_dir/$name" \
        >"$run_dir/$name.out" 2>"$run_dir/$name.log"; then
        cat "$run_dir/$name.out" "$run_dir/$name.log" >&2
        echo "error: general-settings native gate failed: $name" >&2
        exit 1
    fi
}
# Generate the private prefix with the production library, not a diagnostic build.
run cold cold
test "$(hash "$run_dir/cold.prefix.sems")" = 7650d82fe72e58d544dc0043df99ab756ece41092d39e94d7c0b2b7460a904d2
for attempt in first second; do
    run "$attempt" "$run_dir/cold.prefix.sems"
    test "$(hash "$run_dir/$attempt.mid.sems")" = 76a7af5385eb2ddf5dfe94f6607db34e6e820edb06f5054a31bce7a5ef4ada66
    test "$(hash "$run_dir/$attempt.final.sems")" = 613712b78fd1ba748517e710d70280300bee0a48ee43133c9fd9f8070f5442f8
    test "$(hash "$run_dir/$attempt.log")" = 47979b14ad148e366fce73b64a5589c7793ae58a58645f02f2bdaf761ae3458b
    test "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/$attempt.log")" -eq 92
    test "$(grep -c 'operation=write path=settings/general ' "$run_dir/$attempt.log")" -eq 90
    grep -F -q 'trigger=logical-file ordinal=76371 ' "$run_dir/$attempt.log"
    grep -F -q 'operation=close path=settings/general result=1 size=1505 cursor=1505' "$run_dir/$attempt.log"
done
cmp "$run_dir/first.out" "$run_dir/second.out"
cmp "$run_dir/first.final.sems" "$run_dir/second.final.sems"
run resumed "$run_dir/first.mid.sems"
cmp "$run_dir/first.final.sems" "$run_dir/resumed.final.sems"
test "$(hash "$run_dir/resumed.log")" = 9dc4cff27c7a2a3d8499af7460e830d2dc9cc8615c5d50290abc56422b95eb3f
awk 'match($0,/time_ns=[0-9]+/) {t=substr($0,9,RLENGTH-8); if(t>=14401774261) print}' \
    "$run_dir/first.log" >"$run_dir/suffix.log"
cmp "$run_dir/suffix.log" "$run_dir/resumed.log"
# The helper independently checks firmware and full flash before execution.
if "$run_dir/probe" "$manifest" "$manifest" cold "$run_dir/bad-flash" \
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
if "$run_dir/probe" "$run_dir/wrong.semu" "$full_flash" cold "$run_dir/bad-component" \
    >"$run_dir/bad-component.out" 2>"$run_dir/bad-component.log"; then exit 1; fi
grep -F -q 'component application does not match profile' "$run_dir/bad-component.log"
test ! -e "$run_dir/bad-component.prefix.sems"
test "$(hash "$full_flash")" = "$flash_hash"
echo "PASS sapporo-2.39.20 general save: 92 operations, native profile frame, repeat/resume, GPS refusal"
