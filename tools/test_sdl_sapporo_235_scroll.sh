#!/bin/sh
# Optional private-firmware regression: ticket787, E-NEMA-RGBA4444-001.
# Drives native button input past the old phone-instructions GPU fault.
# Ticket 789 / E-EMU-SAPPORO-BRANCH-GATES-001: frame CRC is unchanged.
set -eu
emulator=${SEMU_SDL_EMULATOR:-build/suunto-emu-sdl}
manifest=${SEMU_FIRMWARE_MANIFEST:-tests/private/sapporo-2.35.34.18929/firmware.semu}
if [ ! -f "$manifest" ]; then
    if [ -n "${SEMU_FIRMWARE_MANIFEST-}" ]; then
        echo "error: configured Sapporo 2.35 manifest is unavailable: $manifest" >&2
        exit 2
    fi
    echo 'SKIP Sapporo 2.35 scroll gate: no private firmware manifest'
    exit 0
fi
if [ ! -x "$emulator" ]; then
    echo "error: SDL emulator unavailable: $emulator (run make sdl)" >&2
    exit 2
fi
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sdl-sapporo-235-scroll.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
for pass in 1 2; do
    rc=0
    SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk \
        SEMU_SDL_SETUP_WALK_POST=mmlmmmmmmmmmmmmmmmmmmmmmmmmmmmmm \
        "$emulator" run --profile sapporo-2.35.34 --firmware "$manifest" \
        --layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup \
        --layer sapporo-2.35-gps-startup --layer sapporo-2.35-gps-reopen \
        --layer sapporo-2.35-gps-awake --until setup-next \
        --max-instructions 5000000000 --max-time 22000000000 \
        > "$run_dir/run-$pass.log" 2>&1 || rc=$?
    # The explicit time budget is the successful endpoint, not a user quit.
    if [ "$rc" -ne 3 ]; then cat "$run_dir/run-$pass.log" >&2; exit 1; fi
done
cmp "$run_dir/run-1.log" "$run_dir/run-2.log"
[ "$(shasum -a 256 "$run_dir/run-1.log" | awk '{print $1}')" = \
    c41627380a3a87e4f0b3af2edef017ff8d63ab37b7c6c8c582574e4545d584d8 ]
grep -Fqx 'SDL live test settled step=15 generation=1756 crc32=f0ff828c' "$run_dir/run-1.log"
grep -Fqx 'stop=budget pc=0x000e1862 instructions=4961334596 virtual_time_ns=22000000000' "$run_dir/run-1.log"
if grep -Eq 'machine-reset-request|status=refuse|stop=compat-refused' "$run_dir/run-1.log"; then
    echo 'error: phone-instructions scroll refused or reset' >&2
    exit 1
fi
echo 'PASS Sapporo 2.35 SDL: repeated phone-instructions scroll, bounded at 22s'
