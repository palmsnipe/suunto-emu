#!/bin/sh
set -eu

# Ticket 789, E-EMU-SAPPORO-BRANCH-GATES-001: manual time entry reaches
# the native main menu, LOWER changes its selection, and an idle continuation
# survives beyond the former script allocation panic. A fatal halt is failure.
# Parser-refusal checks run without firmware; private walks auto-detect the
# conventional bundle unless SEMU_SDL_SKIP_FIRMWARE_WALKS is set.
# The disabled manual-time control retains its bounded GPS fixture refusal.

emulator=${SEMU_SDL_EMULATOR:-build/suunto-emu-sdl}
manifest=${SEMU_FIRMWARE_MANIFEST:-}
if [ -z "$manifest" ] && [ -z "${SEMU_SDL_SKIP_FIRMWARE_WALKS:-}" ] &&
   [ -f "$(dirname "$0")/../tests/private/sapporo-2.22.60/firmware.semu" ];
then
    manifest="$(dirname "$0")/../tests/private/sapporo-2.22.60/firmware.semu"
fi
layer=sapporo-2.22-no-device
temporary_root=${TMPDIR:-/tmp}
log=$(mktemp "$temporary_root/suunto-emu-sdl-onboard.XXXXXX")
ppmdir=$(mktemp -d "$temporary_root/suunto-emu-sdl-onboard-ppm.XXXXXX")

cleanup()
{
    rm -f "$log"
    rm -rf "$ppmdir"
}
trap cleanup EXIT HUP INT TERM

if [ ! -x "$emulator" ]; then
    echo "error: onboarding completion emulator is not executable: $emulator" >&2
    exit 2
fi

# --- Parser refusal cases (no firmware required) --------------------------------
set +e
SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk \
    SEMU_SDL_SETUP_WALK_TIMELINE='30000:x' \
    "$emulator" list >"$log" 2>&1
status=$?
set -e
if [ "$status" -ne 2 ] ||
   ! grep -q '^SDL setup-walk: SDL live test timeline button letter must be u, m, or l$' "$log"; then
    cat "$log" >&2
    echo "error: onboarding timeline letter refusal failed" >&2
    exit 1
fi

set +e
SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk \
    SEMU_SDL_SETUP_WALK_TIMELINE='30000:l,32000' \
    "$emulator" list >"$log" 2>&1
status=$?
set -e
if [ "$status" -ne 2 ] ||
   ! grep -q '^SDL setup-walk: SDL live test timeline entry must be ms:letter$' "$log"; then
    cat "$log" >&2
    echo "error: onboarding timeline malformed-entry refusal failed" >&2
    exit 1
fi

# --- Firmware-gated completion walk -------------------------------------------
if [ -z "$manifest" ]; then
    echo "SKIP onboarding completion firmware check: no manifest (set SEMU_FIRMWARE_MANIFEST; the conventional tests/private/sapporo-2.22.60/firmware.semu is auto-detected)"
    exit 0
fi
if [ ! -f "$manifest" ]; then
    echo "error: onboarding completion firmware manifest is unavailable: $manifest" >&2
    exit 2
fi

expected_press='SDL live test timeline press index=0 virtual_ns=30010093175'
expected_step_22='SDL live test settled step=22 generation=3991 crc32=5321867e'
expected_step_23='SDL live test settled step=23 generation=4073 crc32=8b6879f9'
expected_step_24='SDL live test settled step=24 generation=4136 crc32=cd1b0979'
expected_step_25='SDL live test settled step=25 generation=4193 crc32=455b603a'
expected_step_26='SDL live test settled step=26 generation=4256 crc32=53d3f0c1'
expected_step_27='SDL live test settled step=27 generation=4259 crc32=17e1772c'
expected_step_28='SDL live test settled step=28 generation=4311 crc32=578e2601'
expected_step_29='SDL live test settled step=29 generation=4319 crc32=2a01c517'
expected_step_30='SDL live test settled step=30 generation=4406 crc32=73d569a5'
expected_step_31='SDL live test settled step=31 generation=4510 crc32=040ebb03'
expected_main_frame='suunto-frame-73d569a5.ppm'
expected_selected_frame='suunto-frame-040ebb03.ppm'
expected_walk_hash=2c6910c2d53d0dfd3fa9c00aa046615a3075a725ce33e67876a2706c8a68e0b8
snapshot="$ppmdir/after-setup.sems"
expected_stop='stop=user pc=0x0800009e instructions=8491624576 virtual_time_ns=38819797929'

# E-EMU-SAP233-GAUGE-FIXTURE-001: pins re-derived twice under the lane-
# consistent gauge AvgVCell fixture (pair 9119ef13...); all frame CRCs, the
# main/selected PPM frames, and the GPS-cap line count (11) are unchanged;
# only walk/idle/baseline transcripts and timing tuples shifted (control-
# build attribution recorded in the ledger entry).
# (1) E-EMU-SAPPORO-BRANCH-GATES-001 / ticket 789: manual setup reaches
# Navigation, then a native LOWER press selects Logbook. Fatal halt is failure.
set +e
SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk \
    SEMU_SDL_PPM_DIR="$ppmdir" \
    SEMU_SDL_SETUP_WALK_POST=mlllmlllmmlmmmmm \
    SEMU_SDL_SETUP_WALK_TIMELINE='30000:l' \
    "$emulator" run --profile sapporo-2.22.60 \
    --firmware "$manifest" --layer "$layer" \
    --until setup-next --max-instructions 18000000000 \
    --max-time 60000000000 --snapshot-save "$snapshot" >"$log" 2>&1
status=$?
set -e

if [ "$status" -ne 0 ] ||
   ! grep -qx "$expected_press" "$log" ||
   ! grep -qx "$expected_step_22" "$log" ||
   ! grep -qx "$expected_step_23" "$log" ||
   ! grep -qx "$expected_step_24" "$log" ||
   ! grep -qx "$expected_step_25" "$log" ||
   ! grep -qx "$expected_step_26" "$log" ||
   ! grep -qx "$expected_step_27" "$log" ||
   ! grep -qx "$expected_step_28" "$log" ||
   ! grep -qx "$expected_step_29" "$log" ||
   ! grep -qx "$expected_step_30" "$log" ||
   ! grep -qx "$expected_step_31" "$log" ||
   ! grep -qx 'SDL live test completed setup-navigation last-step=31' "$log" ||
   ! grep -qx "$expected_stop" "$log" ||
   [ "$(shasum -a 256 "$log" | awk '{print $1}')" != "$expected_walk_hash" ] ||
   [ ! -f "$ppmdir/$expected_main_frame" ] ||
   [ ! -f "$ppmdir/$expected_selected_frame" ] ||
   grep -Eq 'machine-reset-request|status=refuse|stop=compat-refused' "$log"; then
    cat "$log" >&2
    echo "error: onboarding completion firmware check failed" >&2
    exit 1
fi
echo "onboarding completion walk: main menu and Logbook selection, stop=user"

# (2) Continue without input beyond the former allocation panic. This checks
# guest execution and the renderer restored at its original generation.
set +e
SDL_VIDEODRIVER=dummy "$emulator" run --profile sapporo-2.22.60 \
    --firmware "$manifest" --layer "$layer" --snapshot-load "$snapshot" \
    --max-instructions 18000000000 --max-time 60000000000 >"$log" 2>&1
status=$?
set -e
expected_idle='stop=budget pc=0x000d4a8c instructions=8798037004 virtual_time_ns=60043686593'
expected_idle_hash=91cc708109a2e0405d385fc894674832c58c5457a2845d36c97571d9bd4ab1bf
if [ "$status" -ne 3 ] || ! grep -Fqx "$expected_idle" "$log" ||
   [ "$(shasum -a 256 "$log" | awk '{print $1}')" != "$expected_idle_hash" ] ||
   grep -Eq 'machine-reset-request|status=refuse|stop=compat-refused' "$log"; then
    cat "$log" >&2
    echo "error: post-setup idle continuation failed" >&2
    exit 1
fi
echo "onboarding idle continuation: active through 60 virtual seconds"

# (3) Disabled manual-time control retains its finite GPS-cap refusal.
# Pins are re-derived after the architectural branch correction (ticket 789).
set +e
SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk \
    SEMU_SDL_SETUP_WALK_POST=mlllmlllmm \
    "$emulator" run --profile sapporo-2.22.60 \
    --firmware "$manifest" --layer "$layer" \
    --until setup-next --max-instructions 40000000000 \
    --max-time 300000000000 >"$log" 2>&1
status=$?
set -e

expected_baseline_step_21='SDL live test settled step=21 generation=1927 crc32=8362b9bc'
expected_baseline_stop='stop=compat-refused pc=0x0010fbde instructions=13176760848 virtual_time_ns=68149538779 detail=layer sapporo-2.22-no-device trigger gps-awake-pulse exceeded budget'
expected_baseline_hash=9fe0c259b48a59ada20064d68f7be32834dc463df3958b23e8d3f2072efa8a5b

# The GPS-cap abort is a compatibility refusal; the CLI reports it with exit
# status 3 (only halt/user/wfi-deadlock stop reasons exit 0).
if [ "$status" -ne 3 ] ||
   ! grep -qx "$expected_baseline_step_21" "$log" ||
   ! grep -Fqx "$expected_baseline_stop" "$log" ||
   [ "$(shasum -a 256 "$log" | awk '{print $1}')" != "$expected_baseline_hash" ] ||
   [ "$(grep -c 'trigger=gps-awake-pulse ordinal=' "$log")" -ne 11 ]; then
    cat "$log" >&2
    echo "error: onboarding disabled-case byte-identical check failed" >&2
    exit 1
fi
echo "onboarding disabled manual-time control: expected GPS-cap refusal"

echo "onboarding completion check: passed"
