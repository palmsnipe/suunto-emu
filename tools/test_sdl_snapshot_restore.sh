#!/bin/sh
set -eu
# Ticket 803 / E-EMU-SAP235-EXERCISE-003: Apollo4 codec 1 snapshot pins.
# Paired attribution changes only section 5 (+17 bytes); old logs are unchanged.
# Ticket 791: restore the displayed menu before any guest instruction, then
# continue with native LOWER input. Private artifacts remain in a temp directory.
emulator=${SEMU_SDL_EMULATOR:-build/suunto-emu-sdl}
manifest=${SEMU_FIRMWARE_MANIFEST:-tests/private/sapporo-2.22.60/firmware.semu}
if [ ! -f "$manifest" ]; then
    if [ -n "${SEMU_FIRMWARE_MANIFEST:-}" ]; then
        echo "error: requested firmware manifest is missing" >&2; exit 2
    fi
    echo "SKIP renderer restore: private Sapporo 2.22 firmware unavailable"; exit 0
fi
work=$(mktemp -d "${TMPDIR:-/tmp}/suunto-renderer-restore.XXXXXX")
cleanup()
{
    status=$?
    if [ "$status" -ne 0 ]; then
        echo "renderer restore failed (status $status)" >&2
        for log in "$work"/*.log; do
            [ ! -f "$log" ] || cat "$log" >&2
        done
    fi
    rm -rf "$work"
}
trap cleanup EXIT HUP INT TERM
bounded()
{
    python3 - "$@" <<'PY'
import subprocess, sys
try:
    sys.exit(subprocess.run(sys.argv[1:], timeout=900).returncode)
except subprocess.TimeoutExpired:
    sys.exit(124)
PY
}
export SDL_VIDEODRIVER=dummy
snapshot=${SEMU_SDL_TEST_SNAPSHOT:-$work/menu.sems}
if [ -z "${SEMU_SDL_TEST_SNAPSHOT:-}" ]; then
    bounded env SEMU_SDL_LIVE_TEST=setup-walk \
        SEMU_SDL_SETUP_WALK_POST=mlllmlllmmlmmmmm \
        SEMU_SDL_SETUP_WALK_TIMELINE=30000:l "$emulator" run \
        --profile sapporo-2.22.60 --firmware "$manifest" \
        --layer sapporo-2.22-no-device --until setup-next \
        --max-instructions 18000000000 --max-time 60000000000 \
        --snapshot-save "$snapshot" >"$work/cold.log" 2>&1
    [ "$(shasum -a 256 "$work/cold.log" | awk '{print $1}')" = \
        2c6910c2d53d0dfd3fa9c00aa046615a3075a725ce33e67876a2706c8a68e0b8 ]
fi
# E-EMU-RENDERER-SNAPSHOT-002 (2026-10-02): paired renderer-codec-2 pins.
# The historical codec-1 mismatch was attributed to cb6298b with a paired
# 92b8ac4 control. Guest execution and published frame pins are unchanged.
[ "$(shasum -a 256 "$snapshot" | awk '{print $1}')" = \
    5efa72283e10e649be9da883c0add66c895602cda9d871a8d2fb37ff26fcc886 ]
cat >"$work/lower.replay" <<'EOF'
39000000000 button lower press
39100000000 button lower release
EOF
for pass in 1 2; do
    set +e
    bounded "$emulator" run --profile sapporo-2.22.60 --firmware "$manifest" \
        --layer sapporo-2.22-no-device --snapshot-load "$snapshot" \
        --max-instructions 8491624576 --max-time 38819797929 \
        >"$work/initial-$pass.log" 2>&1
    status=$?
    set -e
    [ "$status" -eq 3 ]
    grep -Fqx 'SDL first-frame width=240 height=240 generation=4510 crc32=040ebb03' "$work/initial-$pass.log"
    grep -Fqx 'stop=budget pc=0x0800009e instructions=8491624576 virtual_time_ns=38819797929' "$work/initial-$pass.log"
    set +e
    bounded "$emulator" run --profile sapporo-2.22.60 --firmware "$manifest" \
        --layer sapporo-2.22-no-device --snapshot-load "$snapshot" \
        --input-replay "$work/lower.replay" --max-instructions 10000000000 \
        --max-time 41000000000 --snapshot-save "$work/next-$pass.sems" \
        >"$work/next-$pass.log" 2>&1
    status=$?
    set -e
    [ "$status" -eq 3 ]
    grep -Fqx 'stop=budget pc=0x000d4a8c instructions=8734743608 virtual_time_ns=41090034111' "$work/next-$pass.log"
    # Full machine state includes generation 4610 and Media controls CRC 0cb272ba.
    [ "$(shasum -a 256 "$work/next-$pass.sems" | awk '{print $1}')" = \
        1782aff8b36575c0bdadbf18fa5834ea7866977e89a5c5cb2feb1405b4dd3c46 ]
done
cmp "$work/initial-1.log" "$work/initial-2.log"
cmp "$work/next-1.log" "$work/next-2.log"
cmp "$work/next-1.sems" "$work/next-2.sems"
echo 'renderer restore: immediate saved frame; paired native LOWER continuation passed'
