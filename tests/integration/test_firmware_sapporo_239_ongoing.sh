#!/bin/sh
# TEST_TAGS: sapporo_239_ongoing
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 ongoing file: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
log_hash=164a4f2d881fca6fc33adcabd307e26cb5264e80f1629d2b735c5b81525ddac2
snapshot_hash=ec57b83b28ac2683eff39d4bf4624c04b6ed375bf3aa814a36a000ee951991d9
if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 ongoing file: set SEMU_SAPPORO_239_FULL_FLASH"
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
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-ongoing.XXXXXX")
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
        --max-time 30000000000 "$@" >"$run_dir/$name.log" 2>&1 || code=$?
    if [ "$code" -ne 3 ]; then
        echo "error: ongoing-file checkpoint returned $code" >&2
        cat "$run_dir/$name.log" >&2
        exit 1
    fi
}
run first 453000000
run second 453000000
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log" ||
   ! cmp -s "$run_dir/first.sems" "$run_dir/second.sems" ||
   [ "$(hash "$run_dir/first.log")" != "$log_hash" ] ||
   [ "$(hash "$run_dir/first.sems")" != "$snapshot_hash" ]; then
    echo "error: ongoing-file checkpoint artifacts differ" >&2
    exit 1
fi
if ! grep -F -x -q \
    'stop=budget pc=0x000d20aa instructions=453000000 virtual_time_ns=2187115076' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|status=refuse' \
    "$run_dir/first.log" ||
   [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 75792 ] ||
   [ "$(grep -c 'event=logical-file .*path=actitmln/ongoing.bin ' "$run_dir/first.log")" -ne 20 ] ||
   [ "$(grep -c 'operation=write path=actitmln/ongoing.bin ' "$run_dir/first.log")" -ne 5 ] ||
   ! grep -F -q 'operation=size path=actitmln/ongoing.bin result=152 size=152' "$run_dir/first.log" ||
   ! grep -F -q 'operation=read path=actitmln/ongoing.bin result=40 size=152 cursor=152' "$run_dir/first.log"; then
    echo "error: native ongoing-file lifecycle or boundary changed" >&2
    exit 1
fi

# Preserve ticket 747's actual prefix, not its superseded unknown-path refusal.
run prefix 439081594
if [ "$(hash "$run_dir/prefix.log")" != 6e39f50a92c4ab8b069983de8dc24e31fe0ab192a6cd84617803d8f983449948 ] ||
   [ "$(hash "$run_dir/prefix.sems")" != 293f9e87f9cdf386ee7ba7f4c694f356cac24c659487d09e5ac4994432401cbe ]; then
    echo "error: historical eleven-file checkpoint changed" >&2
    exit 1
fi
run resumed 453000000 --snapshot-load "$run_dir/prefix.sems"
if ! cmp -s "$run_dir/first.sems" "$run_dir/resumed.sems"; then
    echo "error: resumed ongoing-file checkpoint differs" >&2
    exit 1
fi
# The unknown-open-mode refusal is gone (E-SAP239-CAPLAW-001/E-SAP239-DEEPCLEAN-001):
# the guest uses mode=3 (read/write) at this site, admitted by the file law, so the
# next instruction runs as recorded. The refusal run now pins clean continuation.
run refusal 453000001 --snapshot-load "$run_dir/first.sems"
if ! grep -F -x -q \
    'stop=budget pc=0x000a7f22 instructions=453000001 virtual_time_ns=2187115077' \
    "$run_dir/refusal.log" ||
   grep -E -q 'compat-refused|status=refuse' "$run_dir/refusal.log"; then
    echo "error: next-instruction continuation changed" >&2
    cat "$run_dir/refusal.log" >&2
    exit 1
fi
if [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: source flash was modified" >&2
    exit 1
fi
echo "PASS sapporo-2.39.20 native ongoing file, legacy prefix, resume and mode refusal"
