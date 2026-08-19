#!/bin/sh

# Build or reuse the validated pre-frame checkpoint, then launch the live SDL
# setup checkpoint.  The checkpoint contains machine state only; firmware and
# resources remain external and are identity-checked by the emulator.
set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "usage: sh tools/run_sapporo_ui.sh MANIFEST [SNAPSHOT]" >&2
    exit 2
fi

manifest=$1
snapshot=${2:-${SEMU_SAPPORO_UI_SNAPSHOT:-/tmp/suunto-sapporo-ui-preframe.sems}}
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
headless="$root_dir/build/suunto-emu"
sdl="$root_dir/build/suunto-emu-sdl"

if [ ! -r "$manifest" ]; then
    echo "manifest is not readable: $manifest" >&2
    exit 2
fi
if [ ! -x "$headless" ] || [ ! -x "$sdl" ]; then
    echo "build both binaries first with: make sdl" >&2
    exit 2
fi

if [ ! -s "$snapshot" ]; then
    echo "creating Sapporo UI checkpoint: $snapshot" >&2
    set +e
    SEMU_FIRMWARE_MANIFEST="$manifest" "$headless" run \
        --profile sapporo-2.22.60 --firmware "$manifest" \
        --layer sapporo-2.22-no-device \
        --max-instructions 450800000 \
        --snapshot-save "$snapshot"
    status=$?
    set -e
    if [ "$status" -ne 3 ] || [ ! -s "$snapshot" ]; then
        echo "failed to create Sapporo UI checkpoint" >&2
        exit "$status"
    fi
fi

exec env SEMU_FIRMWARE_MANIFEST="$manifest" "$sdl" run \
    --profile sapporo-2.22.60 --firmware "$manifest" \
    --layer sapporo-2.22-no-device --until middle-language \
    --snapshot-load "$snapshot" --wait-for-quit \
    --max-instructions 14000000000 --max-time 22000000000
