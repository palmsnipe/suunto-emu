#!/bin/sh
# TEST_TAGS: sapporo_239_activity_budget
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 activity budget: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 activity budget: set SEMU_SAPPORO_239_FULL_FLASH"
    exit 0
fi
hash()
{
    shasum -a 256 "$1" | awk '{print $1}'
}
if [ ! -r "$full_flash" ] || [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: Sapporo 2.39 full flash unavailable or hash mismatch" >&2
    exit 2
fi
"$emulator" validate --profile sapporo-2.39.20 --firmware "$manifest"
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-activity.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
run()
{
    name=$1
    limit=$2
    expected_code=$3
    shift 3
    code=0
    "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --max-instructions "$limit" --max-time 30000000000 \
        --snapshot-save "$run_dir/$name.sems" "$@" \
        >"$run_dir/$name.log" 2>&1 || code=$?
    if [ "$code" -ne "$expected_code" ]; then
        echo "error: activity checkpoint returned $code" >&2
        cat "$run_dir/$name.log" >&2
        exit 1
    fi
}
# Two independent cold starts preserve the exact, rendered ticket-753 logo.
for attempt in first second; do
    run "logo_$attempt" 1000000000 0 --until normal-frame
    if [ "$(hash "$run_dir/logo_$attempt.log")" != 1ae47f317dffb829a058f8de8f49db0fb0a2797d1b580c036f5c5b343a2595e8 ] ||
       [ "$(hash "$run_dir/logo_$attempt.sems")" != ffbabbf614344ee4657f7efd22d2a14364e55d493e1880616d1f1d3371286bfd ]; then
        echo "error: historical boot-logo checkpoint changed" >&2
        exit 1
    fi
    # RE-SCOPED (ticket 777, E-SAP239-DEEPCLEAN-001): the cold logo now
    # stops at the first rendered frame (stop=user 640300000, exit 0) and
    # post-frame guest time idles, so the actitmln burst is reached via a
    # lifecycle boundary prefix (460M, before the actitmln burst) loaded
    # under --until at the continuation ceiling.
    run "pre_$attempt" 460000000 3
    run "$attempt" 932397949 0 --until normal-frame --snapshot-load "$run_dir/pre_$attempt.sems"
done
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log" ||
   ! cmp -s "$run_dir/first.sems" "$run_dir/second.sems" ||
   [ "$(hash "$run_dir/first.log")" != 9c6691f9dec27ed1fc0125f957b42033b7b8f77c4a12100f0876ba79915c6cd0 ] ||
   [ "$(hash "$run_dir/first.sems")" != ffbabbf614344ee4657f7efd22d2a14364e55d493e1880616d1f1d3371286bfd ]; then
    echo "error: activity continuation artifacts differ" >&2
    exit 1
fi
if ! grep -F -x -q \
    'stop=user pc=0x000a7ac4 instructions=640300000 virtual_time_ns=2487119465' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|status=refuse' "$run_dir/first.log" ||
   [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 111 ] ||
   [ "$(grep -c 'event=logical-file .*path=actitmln/247.bin' "$run_dir/first.log")" -ne 9 ] ||
   [ "$(grep -c 'event=logical-file .*path=actitmln/ongoing.bin' "$run_dir/first.log")" -ne 12 ] ||
   [ "$(grep -c 'operation=write path=actitmln/247.bin' "$run_dir/first.log")" -ne 3 ] ||
   [ "$(grep -c 'operation=write path=actitmln/ongoing.bin' "$run_dir/first.log")" -ne 5 ]; then
    echo "error: native activity update count or boundary changed" >&2
    exit 1
fi
grep -F -q 'trigger=logical-file ordinal=76279 ' "$run_dir/first.log"
grep -F -q 'operation=close path=actitmln/247.bin result=1 size=46112 cursor=24' "$run_dir/first.log"
grep -F -q 'operation=close path=actitmln/ongoing.bin result=1 size=152 cursor=24' "$run_dir/first.log"

# Snapshot after both native updates: no file/cache/scheduler state is repaired.
run updated 610700000 3 --snapshot-load "$run_dir/pre_first.sems"
run resumed 932397949 0 --until normal-frame --snapshot-load "$run_dir/updated.sems"
cmp "$run_dir/first.sems" "$run_dir/resumed.sems"
run halted 640300001 0 --until normal-frame --snapshot-load "$run_dir/first.sems"
if ! grep -F -x -q \
    'stop=user pc=0x000a7ac6 instructions=640300001 virtual_time_ns=2487119466' \
    "$run_dir/halted.log"; then
    echo "error: native GPS assertion no-op stop changed" >&2
    cat "$run_dir/halted.log" >&2
    exit 1
fi
if [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: source flash was modified" >&2
    exit 1
fi
echo "PASS sapporo-2.39.20 post-logo activity, deterministic resume and native GPS assertion no-op"
