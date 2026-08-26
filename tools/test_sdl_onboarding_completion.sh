#!/bin/sh
set -eu

# Regression for ticket 670: the 2.22.60 onboarding completes standalone to
# the main face through the firmware's own manual time-entry chain, driven by
# the setup-walk plus the opt-in SEMU_SDL_SETUP_WALK_TIMELINE press schedule.
#
# Parser-refusal cases run unconditionally and need no firmware. The full
# completion walk and its byte-identical disabled-case baseline are gated on
# SEMU_FIRMWARE_MANIFEST (skipped when unset), matching test_sdl_live_input.sh.

emulator=${SEMU_SDL_EMULATOR:-build/suunto-emu-sdl}
manifest=${SEMU_FIRMWARE_MANIFEST:-}
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
    echo "SKIP onboarding completion firmware check: SEMU_FIRMWARE_MANIFEST is unset"
    exit 0
fi
if [ ! -f "$manifest" ]; then
    echo "error: onboarding completion firmware manifest is unavailable: $manifest" >&2
    exit 2
fi

expected_press='SDL live test timeline press index=0 virtual_ns=30009231542'
expected_step_22='SDL live test settled step=22 generation=3180 crc32=5321867e'
expected_step_23='SDL live test settled step=23 generation=3262 crc32=c683e828'
expected_step_24='SDL live test settled step=24 generation=3325 crc32=cd1b0979'
expected_step_25='SDL live test settled step=25 generation=3382 crc32=455b603a'
expected_step_26='SDL live test settled step=26 generation=3445 crc32=53d3f0c1'
expected_step_27='SDL live test settled step=27 generation=3448 crc32=17e1772c'
expected_step_28='SDL live test settled step=28 generation=3500 crc32=578e2601'
expected_step_29='SDL live test settled step=29 generation=3506 crc32=1c62ab1a'
expected_main_frame='suunto-frame-fb8e0155.ppm'
expected_stop='stop=halt pc=0x000727ca instructions=13719427895 virtual_time_ns=43807257472'

# (1) Completion walk: manual time-entry chain reaches the main face.
set +e
SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk \
    SEMU_SDL_PPM_DIR="$ppmdir" \
    SEMU_SDL_SETUP_WALK_POST=mlllmlllmmlmmmmm \
    SEMU_SDL_SETUP_WALK_TIMELINE='30000:l' \
    "$emulator" run --profile sapporo-2.22.60 \
    --firmware "$manifest" --layer "$layer" \
    --until setup-next --max-instructions 40000000000 \
    --max-time 300000000000 >"$log" 2>&1
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
   ! grep -qx "$expected_stop" "$log" ||
   [ ! -f "$ppmdir/$expected_main_frame" ]; then
    cat "$log" >&2
    echo "error: onboarding completion firmware check failed" >&2
    exit 1
fi
echo "onboarding completion walk: reached main face ($expected_main_frame), stop=halt"

# (2) Byte-identical disabled case: default walk must remain byte-stable to the
#     E-SAP-ONBOARD-EMU-009 baseline (last settled step 21 + GPS-cap abort).
set +e
SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk \
    SEMU_SDL_SETUP_WALK_POST=mlllmlllmm \
    "$emulator" run --profile sapporo-2.22.60 \
    --firmware "$manifest" --layer "$layer" \
    --until setup-next --max-instructions 40000000000 \
    --max-time 300000000000 >"$log" 2>&1
status=$?
set -e

expected_baseline_step_21='SDL live test settled step=21 generation=1924 crc32=8362b9bc'
expected_baseline_stop='stop=compat-refused pc=0x0010fbde instructions=13277642122 virtual_time_ns=71118716877'

# The GPS-cap abort is a compatibility refusal; the CLI reports it with exit
# status 3 (only halt/user/wfi-deadlock stop reasons exit 0).
if [ "$status" -ne 3 ] ||
   ! grep -qx "$expected_baseline_step_21" "$log" ||
   ! grep -q "^$expected_baseline_stop " "$log"; then
    cat "$log" >&2
    echo "error: onboarding disabled-case byte-identical check failed" >&2
    exit 1
fi
echo "onboarding disabled-case baseline: byte-identical to E-SAP-ONBOARD-EMU-009"

echo "onboarding completion check: passed"
