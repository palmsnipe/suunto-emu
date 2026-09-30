#!/bin/sh
set -eu
# Launch the Sapporo 2.35.34 watch interactively at the settled main
# screen.  Reuses (or first builds) a snapshot saved at the natural walk
# quit, so the watch face is on screen immediately instead of after a
# ~3-minute cold boot.  Private firmware and the snapshot stay outside
# the repository.
#
#   usage: sh tools/run_sapporo_235_watch.sh MANIFEST [SNAPSHOT]
#
#   buttons: keyboard Up = upper, Return/Enter = middle, Down = lower;
#   or click the top/middle/bottom third of the watch face.  Close the
#   window to exit.
manifest=${1:?usage: sh tools/run_sapporo_235_watch.sh MANIFEST [SNAPSHOT]}
snapshot=${2:-/tmp/sapporo-235-main.sems}
emulator=${SEMU_SDL_EMULATOR:-build/suunto-emu-sdl}
if [ ! -f "$manifest" ]; then
    echo "error: firmware manifest is missing: $manifest" >&2
    exit 2
fi
if [ ! -x "$emulator" ]; then
    echo "error: SDL emulator unavailable: $emulator (run make sdl)" >&2
    exit 2
fi
layers="--layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup \
--layer sapporo-2.35-gps-startup --layer sapporo-2.35-gps-reopen \
--layer sapporo-2.35-gps-awake"
if [ ! -f "$snapshot" ]; then
    echo "building the main-screen snapshot (one cold walk, ~3 min)…" >&2
    SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk \
        SEMU_SDL_SETUP_WALK_POST=mmlllmlllmmmmmmmmmmmmmmmmmmmmmm \
        "$emulator" run --profile sapporo-2.35.34 --firmware "$manifest" \
        $layers --until setup-next --max-instructions 10000000000 \
        --max-time 40000000000 --snapshot-save "$snapshot" >/dev/null 2>&1
fi
echo "restoring $snapshot; press Up/Return/Down (or click the thirds); close the window to exit" >&2
exec "$emulator" run --profile sapporo-2.35.34 --firmware "$manifest" \
    $layers --snapshot-load "$snapshot" \
    --max-instructions 100000000000 --max-time 3600000000000 --wait-for-quit
