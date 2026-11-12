#!/bin/sh
# TEST_TAGS: wismar_246_reset
set -eu

if [ "${TEST_PROFILE-}" != wismar-2.46.14 ]; then
    echo "SKIP wismar 2.46.14 reset runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

emulator=${SEMU_EMULATOR:?}
manifest=${SEMU_FIRMWARE_MANIFEST:?}
work=$(mktemp -d "${TMPDIR:-/tmp}/semu-wismar-246-reset.XXXXXX")
trap 'rm -rf "$work"' EXIT

"$emulator" list | grep -Fxq 'wismar-2.46.14  Wismar  2.46.14.26212-U  reset-bring-up'

# Manifest identity must refuse before resolving component files or booting.
sed 's/^version=.*/version=2.35.36.10731-R/' "$manifest" >"$work/wrong.semu"
rc=0
result=$("$emulator" run --profile wismar-2.46.14 --firmware "$work/wrong.semu" \
    --max-instructions 1 --max-time 1 2>&1) || rc=$?
test "$rc" -eq 2
test "$result" = 'run: firmware does not match profile'

run_boot()
{
    rc=0
    "$emulator" run --profile wismar-2.46.14 --firmware "$manifest" \
        --max-instructions 80000000 --max-time 8000000000 >"$1" 2>&1 || rc=$?
    test "$rc" -eq 3 || { cat "$1" >&2; return 1; }
}

run_boot "$work/first"
run_boot "$work/second"
cmp "$work/first" "$work/second"
stop='stop=budget pc=0x0051bc82 instructions=15128956 virtual_time_ns=8000000000'
tail -1 "$work/first" >"$work/stop"
printf '%s\n' "$stop" | cmp - "$work/stop"
# Fault-free window: no refusals, no guest AIRCR resets, no warnings —
# matching the lane 1 s BKPT probe (E-WIS-0001, terminal pc 0x51bc82,
# cfsr/hfsr 0).
test "$(grep -c 'machine-reset-request' "$work/first")" -eq 0
test "$(grep -c 'level=warning' "$work/first")" -eq 0
cat "$work/first"
echo 'PASS wismar-2.46.14 standalone reset and refusal checkpoint'
