#!/bin/sh
# TEST_TAGS: ulsan_244_reset
set -eu

if [ "${TEST_PROFILE-}" != ulsan-2.44.52 ]; then
    echo "SKIP ulsan 2.44.52 reset runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

emulator=${SEMU_EMULATOR:?}
manifest=${SEMU_FIRMWARE_MANIFEST:?}
work=$(mktemp -d "${TMPDIR:-/tmp}/semu-ulsan-244-reset.XXXXXX")
trap 'rm -f "$work/first" "$work/second" "$work/wrong.semu"; rmdir "$work"' EXIT

"$emulator" list | grep -Fxq 'ulsan-2.44.52  Ulsan  2.44.52.20521-R  reset-bring-up'

# Manifest identity must refuse before resolving component files or booting.
sed 's/^version=.*/version=2.35.36.10731-R/' "$manifest" >"$work/wrong.semu"
rc=0
result=$("$emulator" run --profile ulsan-2.44.52 --firmware "$work/wrong.semu" \
    --max-instructions 1 --max-time 1 2>&1) || rc=$?
test "$rc" -eq 2
test "$result" = 'run: firmware does not match profile'

run_boot()
{
    rc=0
    "$emulator" run --profile ulsan-2.44.52 --firmware "$manifest" \
        --max-instructions 160000000 --max-time 8000000000 >"$1" 2>&1 || rc=$?
    test "$rc" -eq 3 || { cat "$1" >&2; return 1; }
}

run_boot "$work/first"
run_boot "$work/second"
cmp "$work/first" "$work/second"
stop='stop=budget pc=0x000d47f0 instructions=98756036 virtual_time_ns=8014847988'
printf '%s\n' "$stop" | cmp - "$work/first"
cat "$work/first"
echo 'PASS ulsan-2.44.52 standalone reset and refusal checkpoint'
