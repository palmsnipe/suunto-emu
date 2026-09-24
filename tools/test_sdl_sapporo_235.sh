#!/bin/sh
# Optional SDL3/private-firmware gate; E-SAP-0043/0044, ticket 782.
# Ticket 789 / E-EMU-SAPPORO-BRANCH-GATES-001: paired F57F correction audit.
set -eu
emulator=${SEMU_SDL_EMULATOR:-build/suunto-emu-sdl}
manifest=${SEMU_FIRMWARE_MANIFEST:-tests/private/sapporo-2.35.34.18929/firmware.semu}
if [ ! -f "$manifest" ]; then
    if [ -n "${SEMU_FIRMWARE_MANIFEST-}" ]; then
        echo "error: configured Sapporo 2.35 manifest is unavailable: $manifest" >&2
        exit 2
    fi
    echo 'SKIP Sapporo 2.35 SDL gate: no private firmware manifest'
    exit 0
fi
if [ ! -x "$emulator" ]; then
    echo "error: SDL emulator unavailable: $emulator (run make sdl)" >&2
    exit 2
fi
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sdl-sapporo-235.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
for pass in 1 2; do
    # CLI validates every component before guest execution. The built-in
    # input driver queues real SDL keys/clicks and quits after three steps.
    if ! SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=middle-language \
        "$emulator" run --profile sapporo-2.35.34 --firmware "$manifest" \
        --layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup \
        --until middle-language --max-instructions 2000000000 \
        --max-time 11000000000 > "$run_dir/run-$pass.log" 2>&1; then
        cat "$run_dir/run-$pass.log" >&2
        exit 1
    fi
done
cmp "$run_dir/run-1.log" "$run_dir/run-2.log"
[ "$(shasum -a 256 "$run_dir/run-1.log" | awk '{print $1}')" = \
    eb9d4183cf9b824bdcc7102f3bf145eee6e15f7f22dcf83a2da8326083ad2bbb ]
grep -Fqx 'SDL live test settled step=1 generation=3 crc32=4979f432' "$run_dir/run-1.log"
grep -Fqx 'SDL live test settled step=2 generation=6 crc32=3bd12ac8' "$run_dir/run-1.log"
grep -Fqx 'SDL live test settled step=3 generation=79 crc32=405422e1' "$run_dir/run-1.log"
grep -Fqx 'SDL live test completed setup-navigation' "$run_dir/run-1.log"
grep -Fqx 'stop=user pc=0x080000a0 instructions=1262036864 virtual_time_ns=7966140811' "$run_dir/run-1.log"
if grep -Eq 'machine-reset-request|status=refuse' "$run_dir/run-1.log"; then
    echo 'error: 2.35 SDL prefix refused or reset' >&2
    exit 1
fi
echo 'PASS Sapporo 2.35 SDL: repeated startup and middle-button language menu'
