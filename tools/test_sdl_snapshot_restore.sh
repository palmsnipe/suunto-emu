#!/bin/sh
set -eu
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
        7186e3eb3f16474294628d6753932f9635c8a3ce2a7fc8cb66138cabf831eafa ]
fi
[ "$(shasum -a 256 "$snapshot" | awk '{print $1}')" = \
    f829b2fa514c65b0e10d1f7faa20f4564ba95a442ffd8d217fa21b28b711e592 ]
cat >"$work/lower.replay" <<'EOF'
39000000000 button lower press
39100000000 button lower release
EOF
for pass in 1 2; do
    set +e
    bounded "$emulator" run --profile sapporo-2.22.60 --firmware "$manifest" \
        --layer sapporo-2.22-no-device --snapshot-load "$snapshot" \
        --max-instructions 8500057344 --max-time 38818426902 \
        >"$work/initial-$pass.log" 2>&1
    status=$?
    set -e
    [ "$status" -eq 3 ]
    grep -Fqx 'SDL first-frame width=240 height=240 generation=4510 crc32=040ebb03' "$work/initial-$pass.log"
    grep -Fqx 'stop=budget pc=0x0800009e instructions=8500057344 virtual_time_ns=38818426902' "$work/initial-$pass.log"
    set +e
    bounded "$emulator" run --profile sapporo-2.22.60 --firmware "$manifest" \
        --layer sapporo-2.22-no-device --snapshot-load "$snapshot" \
        --input-replay "$work/lower.replay" --max-instructions 10000000000 \
        --max-time 41000000000 --snapshot-save "$work/next-$pass.sems" \
        >"$work/next-$pass.log" 2>&1
    status=$?
    set -e
    [ "$status" -eq 3 ]
    grep -Fqx 'stop=budget pc=0x000d4a8c instructions=8744080727 virtual_time_ns=41088770901' "$work/next-$pass.log"
    # Full machine state includes generation 4610 and Media controls CRC 0cb272ba.
    [ "$(shasum -a 256 "$work/next-$pass.sems" | awk '{print $1}')" = \
        e5c5dd57e7ad7b7ba1941f2b1bdd8123d42e6ab286ed152d55ecd38f885a9ab6 ]
done
cmp "$work/initial-1.log" "$work/initial-2.log"
cmp "$work/next-1.log" "$work/next-2.log"
cmp "$work/next-1.sems" "$work/next-2.sems"
echo 'renderer restore: immediate saved frame; paired native LOWER continuation passed'
