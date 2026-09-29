#!/bin/sh
# TEST_TAGS: sapporo_239_history_budget
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 history budget: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
log_hash=6e39f50a92c4ab8b069983de8dc24e31fe0ab192a6cd84617803d8f983449948
snapshot_hash=ab7c3b6b6e2288eb2c75a761e4321ceaf5a46176c415d4228a33936da12ceec8
if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 history budget: set SEMU_SAPPORO_239_FULL_FLASH"
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
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-history.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

run()
{
    name=$1
    limit=$2
    shift 2
    code=0
    if [ "$name" != refusal ]; then
        set -- --snapshot-save "$run_dir/$name.sems" "$@"
    fi
    "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --until normal-frame --max-instructions "$limit" \
        --max-time 30000000000 \
        "$@" >"$run_dir/$name.log" 2>&1 || code=$?
    if [ "$code" -ne 3 ]; then
        echo "error: history checkpoint returned $code" >&2
        cat "$run_dir/$name.log" >&2
        exit 1
    fi
}
run first 439081594
run second 439081594
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log" ||
   ! cmp -s "$run_dir/first.sems" "$run_dir/second.sems" ||
   [ "$(hash "$run_dir/first.log")" != "$log_hash" ] ||
   [ "$(hash "$run_dir/first.sems")" != "$snapshot_hash" ]; then
    echo "error: history checkpoint artifacts differ" >&2
    exit 1
fi
if ! grep -F -x -q \
    'stop=budget pc=0x000d160e instructions=439081594 virtual_time_ns=2173196670' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|status=refuse' \
    "$run_dir/first.log" ||
   [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 865 ] ||
   [ "$(grep -c 'operation=read path=sleepln/sleep.bin result=72 ' "$run_dir/first.log")" -ne 0 ] ||
   [ "$(grep -c 'operation=seek path=sleepln/sleep.bin ' "$run_dir/first.log")" -ne 2 ]; then
    echo "error: native history scans or boundary changed" >&2
    exit 1
fi

# Total sleep seeks include two earlier header rewinds plus 35,712 record seeks.

# Preserve the actual ticket-746 prefix, not its superseded next-step refusal.
run prefix 435333559
if [ "$(hash "$run_dir/prefix.log")" != 5d6daaadbe1b446e583f4570c5d0ebde8b25dda94a3b8a4d0f37aedeaa7e50e5 ] ||
   [ "$(hash "$run_dir/prefix.sems")" != 661b65fc11797324d444bb77409cce4c09bab4491d0855b00ff4272aea211cfa ]; then
    echo "error: historical pre-refusal checkpoint changed" >&2
    exit 1
fi
run resumed 439081594 --snapshot-load "$run_dir/prefix.sems"
if ! cmp -s "$run_dir/first.sems" "$run_dir/resumed.sems"; then
    echo "error: resumed history checkpoint differs" >&2
    exit 1
fi

# RE-SCOPED under E-SAP239-DEEPCLEAN-001 (ticket 777): the old
# unknown-writable-path refusal at 439081595 was the pre-capacity-law
# wall; the guest now continues through 4e9 instructions with zero
# refusals (twice-verified at 439081595, 442856246, and 4e9 caps).
# The next instruction is pinned as a clean budget continuation.
run refusal 439081595 --snapshot-load "$run_dir/first.sems"
if ! grep -F -x -q \
    'stop=budget pc=0x000d1612 instructions=439081595 virtual_time_ns=2173196671' \
    "$run_dir/refusal.log" || grep -E -q 'compat-refused|status=refuse' \
    "$run_dir/refusal.log"; then
    echo "error: unknown writable-path refusal changed" >&2
    cat "$run_dir/refusal.log" >&2
    exit 1
fi
if [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: source flash was modified" >&2
    exit 1
fi
echo "PASS sapporo-2.39.20 native history scans, old prefix, resume and unknown-path refusal"
