#!/bin/sh

# Build or reuse the validated pre-frame checkpoint, then launch the live SDL
# setup checkpoint.  The checkpoint contains machine state only; firmware and
# resources remain external and are identity-checked by the emulator.
set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 3 ]; then
    echo "usage: sh tools/run_sapporo_ui.sh MANIFEST [SNAPSHOT] [CHECKPOINT]" >&2
    exit 2
fi

manifest=$1
snapshot=${2:-${SEMU_SAPPORO_UI_SNAPSHOT:-/tmp/suunto-ui-preframe.sems}}
checkpoint=${3:-${SEMU_SAPPORO_UI_CHECKPOINT:-middle-language}}
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${SEMU_SAPPORO_UI_BUILD_DIR:-build}
case "$build_dir" in
    /*) build_root=$build_dir ;;
    *) build_root="$root_dir/$build_dir" ;;
esac
headless="$build_root/suunto-emu"
sdl="$build_root/suunto-emu-sdl"

case "$checkpoint" in
    middle-language|lower-transition) ;;
    *)
        echo "unsupported Sapporo UI checkpoint: $checkpoint" >&2
        exit 2
        ;;
esac

if [ ! -r "$manifest" ]; then
    echo "manifest is not readable: $manifest" >&2
    exit 2
fi
if [ ! -x "$sdl" ]; then
    echo "SDL binary is missing; build it first with: make sdl" >&2
    exit 2
fi

if [ ! -s "$snapshot" ]; then
    if [ ! -x "$headless" ]; then
        echo "headless binary is required to create: $snapshot" >&2
        echo "build both binaries first with: make" >&2
        exit 2
    fi
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
    --layer sapporo-2.22-no-device --until "$checkpoint" \
    --snapshot-load "$snapshot" --wait-for-quit \
    --max-instructions 14000000000 --max-time 22000000000
