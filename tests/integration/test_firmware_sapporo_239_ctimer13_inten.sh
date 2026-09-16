#!/bin/sh
# TEST_TAGS: sapporo_239_ctimer13_inten
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 Timer13 INTEN: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
# E-ULS-0047 (ticket 777) re-derivation. The era drift recorded by E-ULS-0041
# (accepted integration batch d311da0..6555d38) replaced guest instructions with
# equal-count paths: the instruction, virtual-time, and 76258-intervention
# anchors below are unchanged from the original pins; only the PC at the budget
# cap and the artifact bytes moved, each re-derived from two byte-identical runs.
# The next-instruction run is pinned as a budget-cap continuation (exit code 3)
# per the E-ULS-0041 BKPT-to-NOP precedent (ctimer_combined_inten.sh).
log_hash=372fdabe855d55b25e5129eb0b2587e27f1f709e40a67b40011ca04ea8b1f365
snapshot_hash=7df7e4064dff38c1bfd6918efc01d8329036694d16ed3d074973601ccd6cb54e
if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 Timer13 INTEN: set SEMU_SAPPORO_239_FULL_FLASH"
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
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-timer13.XXXXXX")
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
        --until normal-frame --max-instructions "$limit" \
        --max-time 30000000000 --snapshot-save "$run_dir/$name.sems" \
        "$@" >"$run_dir/$name.log" 2>&1 || code=$?
    if [ "$code" -ne "$expected_code" ]; then
        echo "error: Timer13 checkpoint returned $code" >&2
        cat "$run_dir/$name.log" >&2
        exit 1
    fi
}
run first 608140266 3
run second 608140266 3
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log" ||
   ! cmp -s "$run_dir/first.sems" "$run_dir/second.sems" ||
   [ "$(hash "$run_dir/first.log")" != "$log_hash" ] ||
   [ "$(hash "$run_dir/first.sems")" != "$snapshot_hash" ]; then
    echo "error: Timer13 checkpoint artifacts differ" >&2
    exit 1
fi
if ! grep -F -x -q \
    'stop=budget pc=0x00093bdc instructions=608140266 virtual_time_ns=2147096849' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|status=refuse' \
    "$run_dir/first.log" ||
   [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 76258 ]; then
    echo "error: Timer13 boundary or compatibility count changed" >&2
    exit 1
fi

# Preserve ticket 751 before its now-superseded exact INTEN refusal.
run prefix 607105617 3
if [ "$(hash "$run_dir/prefix.log")" != aca8415854f8b93f71bdfd58f265ce171a742741696c84e36e9fb3abbccf67c5 ] ||
   [ "$(hash "$run_dir/prefix.sems")" != b17a3b485f89779a3dc8191f1417c6d225a65fdcc41f0681d1cb068c1ad24d90 ]; then
    echo "error: historical checkpoint changed" >&2
    exit 1
fi
run resumed 608140266 3 --snapshot-load "$run_dir/prefix.sems"
if ! cmp -s "$run_dir/first.sems" "$run_dir/resumed.sems"; then
    echo "error: resumed Timer13 checkpoint differs" >&2
    exit 1
fi

# Execute the next guest instruction at the pinned boundary; it runs as recorded
# and stops at the budget cap with exit code 3 (E-ULS-0041 BKPT-to-NOP precedent).
run halt 608140267 3 --snapshot-load "$run_dir/first.sems"
if ! grep -F -x -q \
    'stop=budget pc=0x00093bde instructions=608140267 virtual_time_ns=2147096850' \
    "$run_dir/halt.log"; then
    echo "error: next-instruction continuation changed" >&2
    cat "$run_dir/halt.log" >&2
    exit 1
fi
if [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: source flash was modified" >&2
    exit 1
fi
echo "PASS sapporo-2.39.20 combined Timer13 INTEN, old prefix, resume and boundary continuation"
