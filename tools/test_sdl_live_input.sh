#!/bin/sh
set -eu

emulator=${SEMU_SDL_EMULATOR:-build/suunto-emu-sdl}
manifest=${SEMU_FIRMWARE_MANIFEST:-}
if [ -z "$manifest" ] && [ -z "${SEMU_SDL_SKIP_FIRMWARE_WALKS:-}" ] &&
   [ -f "$(dirname "$0")/../tests/private/sapporo-2.22.60/firmware.semu" ];
then
    manifest="$(dirname "$0")/../tests/private/sapporo-2.22.60/firmware.semu"
fi
snapshot=${SEMU_SDL_TEST_SNAPSHOT:-}
expected_first='SDL first-frame width=240 height=240 generation=1 crc32=2a01c517'
expected_step_one='SDL live test settled step=1 generation=3 crc32=4979f432'
expected_step_two='SDL live test settled step=2 generation=5 crc32=629da47e'
expected_step_three='SDL live test settled step=3 generation=63 crc32=d4ed66c7'
# E-EMU-SAPPORO-BRANCH-GATES-001 / ticket 789: architectural branch fix.
# Frame CRCs are unchanged; the stop tuple/transcript are re-derived.
# E-EMU-SAP233-GAUGE-FIXTURE-001: gauge AvgVCell 0x19 aligned to the current
# lane fixture (pair 9119ef13…); the guest no longer issues the resource-
# status compat probe at t~1.228 s, shifting timing only. All frame CRCs and
# the completed-navigation checkpoint are unchanged; stop tuple and cold
# transcript re-derived twice (control-build attribution in the ledger).
expected_stop='stop=user pc=0x0800009e instructions=770457344 virtual_time_ns=6521343631'
expected_cold_hash=ccc4ea6008192eb3981695c208004a8f530ea6152cfc8d3053770de354f96e93
temporary_root=${TMPDIR:-/tmp}
log=$(mktemp "$temporary_root/suunto-emu-sdl-live.XXXXXX")

cleanup()
{
    rm -f "$log"
}
trap cleanup EXIT HUP INT TERM

if [ ! -x "$emulator" ]; then
    echo "error: SDL live input test emulator is not executable: $emulator" >&2
    exit 2
fi

set +e
SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=invalid \
    "$emulator" list >"$log" 2>&1
status=$?
set -e
if [ "$status" -ne 2 ] ||
   ! grep -q '^SDL live test: SEMU_SDL_LIVE_TEST accepts only ' "$log"; then
    cat "$log" >&2
    echo "error: SDL live input test configuration refusal failed" >&2
    exit 1
fi

set +e
SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=middle-language \
    "$emulator" list >"$log" 2>&1
status=$?
set -e
if [ "$status" -ne 2 ] ||
   ! grep -q '^SDL live test requires --until middle-language ' "$log"; then
    cat "$log" >&2
    echo "error: SDL live input test argument refusal failed" >&2
    exit 1
fi

if [ -z "$manifest" ]; then
    echo "SKIP SDL live input firmware check: no manifest (set SEMU_FIRMWARE_MANIFEST; the conventional tests/private/sapporo-2.22.60/firmware.semu is auto-detected)"
    exit 0
fi
if [ ! -f "$manifest" ]; then
    echo "error: SDL live input firmware manifest is unavailable: $manifest" >&2
    exit 2
fi
if [ -n "$snapshot" ] && [ ! -f "$snapshot" ]; then
    echo "error: SDL live input snapshot is unavailable: $snapshot" >&2
    exit 2
fi

set +e
if [ -n "$snapshot" ]; then
    SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=middle-language \
        "$emulator" run --profile sapporo-2.22.60 \
        --firmware "$manifest" --layer sapporo-2.22-no-device \
        --until middle-language --snapshot-load "$snapshot" \
        --max-instructions 14000000000 --max-time 22000000000 \
        >"$log" 2>&1
else
    SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=middle-language \
        "$emulator" run --profile sapporo-2.22.60 \
        --firmware "$manifest" --layer sapporo-2.22-no-device \
        --until middle-language --max-instructions 14000000000 \
        --max-time 22000000000 >"$log" 2>&1
fi
status=$?
set -e

if [ "$status" -ne 0 ] ||
   ! grep -qx "$expected_first" "$log" ||
   ! grep -q '^SDL live test injected Return/Enter step=1$' "$log" ||
   ! grep -qx "$expected_step_one" "$log" ||
   ! grep -q '^SDL live test injected middle click step=2$' "$log" ||
   ! grep -qx "$expected_step_two" "$log" ||
   ! grep -q '^SDL live test injected middle click step=3$' "$log" ||
   ! grep -qx "$expected_step_three" "$log" ||
   ! grep -q '^SDL live test completed setup-navigation$' "$log" ||
   ! grep -qx "$expected_stop" "$log"; then
    cat "$log" >&2
    echo "error: SDL live input firmware check failed" >&2
    exit 1
fi

if [ -z "$snapshot" ] &&
   [ "$(shasum -a 256 "$log" | awk '{print $1}')" != "$expected_cold_hash" ]; then
    cat "$log" >&2
    echo "error: SDL live input cold-start transcript mismatch" >&2
    exit 1
fi

echo "$expected_stop"
echo "SDL live input firmware check: passed"
