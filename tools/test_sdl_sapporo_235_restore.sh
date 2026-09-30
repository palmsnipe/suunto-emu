#!/bin/sh
set -eu
# Ticket 792 continuation: Sapporo 2.35 interactive snapshot restore.
# A five-layer setup-walk session saved at its natural quit (settled
# main screen, stop=user) restores in SDL: the held frame presents
# before any guest instruction, and the restored session continues
# natively to the WFI park with zero resets, draw refusals, or compat
# hits. The GPS-layer bind confirms the restored owners on load
# (state-identity rules; see tests/unit/test_sapporo_235_gps_reopen.c).
# Re-derived 2026-09-30 after the ticket-710 tsc6a frame-lifecycle fix
# (E-EMU-SAP235-TICKTRAIL-001), twice byte-identically: cold transcript
# 14ffff66… (the re-pinned nav baseline walk), snapshot 5f21f7d1…,
# first frame generation 4770 crc 500b350f, continuation stop pc
# 0x000e1862 at 9578131227 / 42000000000 (the guest stop line is
# unchanged; the sweep frames are clean).
emulator=${SEMU_SDL_EMULATOR:-build/suunto-emu-sdl}
manifest=${SEMU_FIRMWARE_MANIFEST:-tests/private/sapporo-2.35.34.18929/firmware.semu}
if [ ! -f "$manifest" ]; then
    if [ -n "${SEMU_FIRMWARE_MANIFEST:-}" ]; then
        echo "error: requested firmware manifest is missing" >&2; exit 2
    fi
    echo "SKIP Sapporo 2.35 interactive restore: private firmware unavailable"; exit 0
fi
if [ ! -x "$emulator" ]; then
    echo "error: SDL emulator unavailable: $emulator (run make sdl)" >&2; exit 2
fi
work=$(mktemp -d "${TMPDIR:-/tmp}/semu-sap235-restore.XXXXXX")
cleanup()
{
    status=$?
    if [ "$status" -ne 0 ]; then
        echo "Sapporo 2.35 interactive restore failed (status $status)" >&2
        for log in "$work"/*.log; do
            [ ! -f "$log" ] || cat "$log" >&2
        done
    fi
    rm -rf "$work"
}
trap cleanup EXIT HUP INT TERM
export SDL_VIDEODRIVER=dummy
layers="--layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup --layer sapporo-2.35-gps-startup --layer sapporo-2.35-gps-reopen --layer sapporo-2.35-gps-awake"
snapshot=${SEMU_SDL_TEST_SNAPSHOT:-$work/main.sems}
if [ -z "${SEMU_SDL_TEST_SNAPSHOT:-}" ]; then
    set +e
    SEMU_SDL_LIVE_TEST=setup-walk \
        SEMU_SDL_SETUP_WALK_POST=mmlllmlllmmmmmmmmmmmmmmmmmmmmmm \
        "$emulator" run --profile sapporo-2.35.34 --firmware "$manifest" \
        $layers --until setup-next --max-instructions 10000000000 \
        --max-time 40000000000 --snapshot-save "$snapshot" \
        >"$work/cold.log" 2>&1
    status=$?
    set -e
    [ "$status" -eq 0 ]
    [ "$(shasum -a 256 "$work/cold.log" | awk '{print $1}')" = \
        14ffff66146dfce6fabbb57ee2f96917431c02061d6c172ffac731b9adaa9aae ]
fi
[ "$(shasum -a 256 "$snapshot" | awk '{print $1}')" = \
    5f21f7d11a6de4c18e17e61a7256e217508526ea6bcfd09ae46d5e954ca14a5d ]
for pass in 1 2; do
    set +e
    "$emulator" run --profile sapporo-2.35.34 --firmware "$manifest" \
        $layers --snapshot-load "$snapshot" --max-instructions 10500000000 \
        --max-time 42000000000 >"$work/restore-$pass.log" 2>&1
    status=$?
    set -e
    [ "$status" -eq 3 ]
    grep -Fqx 'SDL first-frame width=240 height=240 generation=4770 crc32=500b350f' \
        "$work/restore-$pass.log"
    grep -Fqx 'stop=budget pc=0x000e1862 instructions=9578131227 virtual_time_ns=42000000000' \
        "$work/restore-$pass.log"
    if grep -Eq 'machine-reset-request|draw-refused|compat-refused' \
        "$work/restore-$pass.log"; then
        echo "error: restore pass $pass shows a reset/refusal" >&2; exit 1
    fi
done
cmp "$work/restore-1.log" "$work/restore-2.log"
echo 'PASS Sapporo 2.35 interactive restore: held main screen presents, session continues natively'
