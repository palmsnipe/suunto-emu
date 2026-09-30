#!/bin/sh
# TEST_TAGS: sapporo_235_compressed
# Ticket 793 / E-RE-SAP235-TSC6A-001: TSC6A compressed-texture runtime gate.
# Section 1 derives the 2700-byte refused-draw fixture from the hash-pinned
# resource partition with dd (offset 0x9db613 = 10335763), pins its SHA-256,
# and runs the golden expansion case of the unit binary against the
# twice-reproduced reference window when that volatile private artifact is
# present (it is never stored in Git; absence skips, mismatch fails).
# Section 2 reproduces the five-layer bounded window with the gps-awake
# invocation and pins the derived checkpoint.  Derived 2026-09-23 from
# paired identical runs: this headless window ends at the 70 s virtual-time
# budget at 1860847385 instructions and eleven awake polls and does NOT
# reach the 2.35 main-entry compressed draw (which is only observed in the
# setup-walk trajectory of E-EMU-SAP235-MAIN-TSC6A-001, past this window).
# The window therefore pins the honest boundary: no compressed-source
# refusal, no machine-reset-request, and a byte-identical transcript.
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.35.34 ]; then
    echo "SKIP Sapporo 2.35 compressed-texture runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi
emulator=${SEMU_EMULATOR:?}
manifest=${SEMU_FIRMWARE_MANIFEST:?}
"$emulator" validate --profile sapporo-2.35.34 --firmware "$manifest"

run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-235-compressed.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

# --- Section 1: dd-derived fixture, pinned hash, golden expansion case ---
resources="${manifest%/*}/resources.raw"
[ -f "$resources" ] || { echo "missing $resources"; exit 1; }
# The refused crosshair asset payload: PXB2 header at 0x9db600, data at
# 0x9db613, exactly ceil(60/4)*ceil(60/4)*12 = 2700 bytes.
dd if="$resources" bs=1 skip=10335763 count=2700 of="$run_dir/fixture.bin" 2>/dev/null
[ "$(shasum -a 256 "$run_dir/fixture.bin" | awk '{print $1}')" = \
    f311e1ef2267f07528ce18403f1601ce4b51e7a897b516bcf540b773af8ab371 ]

expand_test="$(dirname "$emulator")/tests/test_nema_tsc6a_expand"
if [ ! -x "$expand_test" ]; then
    echo "SKIP compressed golden: $expand_test not built"
else
    reference=${SEMU_TSC6A_REFERENCE:-/tmp/sap235-tex17/decode-1.bin}
    if [ -f "$reference" ]; then
        [ "$(shasum -a 256 "$reference" | awk '{print $1}')" = \
            2f30fe186ba7edd5ef39498f4ad69736684a1611ee1f5d581f05b2afba756b2f ]
        SEMU_TSC6A_FIXTURE="$run_dir/fixture.bin" \
        SEMU_TSC6A_REFERENCE="$reference" \
            "$expand_test" > "$run_dir/golden.log" 2>&1
        # Ticket 788 added three synthetic admission cases to the expand
        # suite (7 -> 10); the ticket-710 vertical-scroll instance added
        # two more (10 -> 12); the '0 failed' semantics are unchanged.
        tail -1 "$run_dir/golden.log" | grep -qx '12 tests, 0 failed'
        echo 'PASS compressed golden expansion matches decode-1.bin'
    else
        "$expand_test" > "$run_dir/golden.log" 2>&1
        # Same additive-count re-pin as the reference branch above.
        tail -1 "$run_dir/golden.log" | grep -qx '12 tests, 0 failed'
        echo 'SKIP compressed golden reference absent (synthetic cases ran)'
    fi
fi

# --- Section 2: paired five-layer runs and derived checkpoint ---
for pass in 1 2; do
    rc=0
    "$emulator" run --profile sapporo-2.35.34 --firmware "$manifest" \
        --layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup \
        --layer sapporo-2.35-gps-startup --layer sapporo-2.35-gps-reopen \
        --layer sapporo-2.35-gps-awake \
        --max-instructions 3000000000 --max-time 70000000000 \
        > "$run_dir/compressed-$pass.log" 2>&1 || rc=$?
    if [ "$rc" -ne 3 ]; then cat "$run_dir/compressed-$pass.log"; exit 1; fi
done
cmp "$run_dir/compressed-1.log" "$run_dir/compressed-2.log"
[ "$(grep -c 'event=layer-hit layer=sapporo-2.35-gps-awake' "$run_dir/compressed-1.log")" -eq 11 ]
grep -Fqx 'stop=budget pc=0x000e1862 instructions=1860847385 virtual_time_ns=70000000000' "$run_dir/compressed-1.log"
[ "$(shasum -a 256 "$run_dir/compressed-1.log" | awk '{print $1}')" = \
    ee99bc5e034f2e7d0ef94999551b5815ca2eabba891d6bfb0a9d7d32dd7e448f ]
# Honest boundary inside this window: nothing refuses and nothing resets.
# The compressed draw itself lives past this window (setup-walk trajectory);
# its runtime rendering gate is Section 3.
! grep -q 'status=refuse' "$run_dir/compressed-1.log"
! grep -q 'machine-reset-request' "$run_dir/compressed-1.log"
! grep -q 'compressed source' "$run_dir/compressed-1.log"
! grep -q 'nema_tsc6a' "$run_dir/compressed-1.log"
echo 'PASS sapporo-2.35.34 compressed-texture window at derived boundary'

# --- Section 3: main-entry setup-walk trajectory (positive render gate) ---
# The setup-walk trajectory reaches private 2.35 main entry, where the
# 60x60 crosshair draw is accepted by ticket 793.  E-EMU-SAP235-RINGKICK-
# CPU-INVISIBLE-001 (ticket 794) retired the post-Done BusFault-on-kick,
# and the ticket-794 resolve-law extension (E-SAP-0041-EXT4 census:
# accent 0xff55aaff firmware-native in the pinned theme table, all 124
# resolve refusals carry it, every other tuple predicate already passed)
# admits it to the tsc6a ACCENT predicate: the 124 unsupported-resolve
# refusals are gone and their draws RASTER (host-side only — guest
# instructions and virtual time are unchanged).  The ticket-788 codec
# (E-SAP-0041-EXT6, offline RE per the 2026-09-23 owner decision) then
# admits the 103 draw-refused bounce quads: the eased horizontal bounce
# RASTERS host-side and the window ends with ZERO GPU refusals and zero
# resets.  Residual census: a twice-reproduced intermediate walk pair
# showed exactly 9 census rows still refusing on one-ULP binary32
# roundings of the pinned matrix constants (mm11 0x3f7fffff on the
# width-43 wall draws, mm12 0xc2b40001 on width-1/2/4/8 draws; mm02
# keeps obeying 60-rect_x1 exactly); those two observed bit values are
# admitted alternates in the shape predicate, named
# TSC6A_MATRIX_ONE_ALT / TSC6A_MATRIX_TY0_ALT.  With the E-SAP-0041-EXT3
# lane-law poll tail (ticket 710 slice 4) the walk still terminates
# NATURALLY: stop=user at the scripted quit, instructions and virtual
# time byte-identical to the pre-788 pin, steps 24-31 settling on the
# main screen (step 24 3991/1c1f9064 and step 25 4000/74e8d4f5 are
# unchanged from the pre-788 window; from step 26 the bounce contributes:
# the 7ef957e9 alternates recur at 26/28/30 with generations 4131/4389/
# 4646, and the tick members move to 4258/aec1d3a0, 4515/cf8a4285,
# 4770/0a576ff1; settled generations drift as the bounce consumes GPU
# frames, host-side only), zero resolve refusals, zero resets.  Re-derived
# Re-derived 2026-09-30 after the ticket-710 tsc6a frame-lifecycle fix
# (E-EMU-SAP235-TICKTRAIL-001): the seconds-sweep frames re-derive clean
# (steps 25/27/29/31; the 7ef957e9 tick base, step 24, the stop line and
# the zero-refusal guards are byte-identical), twice byte-identically.
# 2026-09-27 from paired byte-identical runs on the final binary
# (transcript sha
# c8b69fce5b29136b752c9da76667e5fe17942aa875709edfcc69dff21538967e),
# integrator-reproduced on a fresh build.
sdl_emulator="$(dirname "$emulator")/suunto-emu-sdl"
if [ ! -x "$sdl_emulator" ]; then
    echo 'SKIP main-entry compressed gate: suunto-emu-sdl not built'
    exit 0
fi
for pass in 1 2; do
    rc=0
    SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk \
    SEMU_SDL_SETUP_WALK_POST=mmlllmlllmmmmmmmmmmmmmmmmmmmmmm \
        "$sdl_emulator" run --profile sapporo-2.35.34 \
            --firmware "$manifest" \
            --layer sapporo-2.35-production-data \
            --layer sapporo-2.35-ohr-startup \
            --layer sapporo-2.35-gps-startup \
            --layer sapporo-2.35-gps-reopen \
            --layer sapporo-2.35-gps-awake \
            --until setup-next --max-instructions 10000000000 \
            --max-time 40000000000 \
        > "$run_dir/main-$pass.log" 2>&1 || rc=$?
    if [ "$rc" -ne 0 ]; then cat "$run_dir/main-$pass.log"; exit 1; fi
done
cmp "$run_dir/main-1.log" "$run_dir/main-2.log"
grep -Fqx 'SDL live test settled step=24 generation=3991 crc32=1c1f9064' \
    "$run_dir/main-1.log"
grep -Fqx 'SDL live test settled step=25 generation=4000 crc32=0e077730' \
    "$run_dir/main-1.log"
grep -Fqx 'SDL live test settled step=26 generation=4131 crc32=7ef957e9' \
    "$run_dir/main-1.log"
grep -Fqx 'SDL live test settled step=27 generation=4258 crc32=0b78f6cf' \
    "$run_dir/main-1.log"
grep -Fqx 'SDL live test settled step=28 generation=4389 crc32=7ef957e9' \
    "$run_dir/main-1.log"
grep -Fqx 'SDL live test settled step=29 generation=4515 crc32=d8e8bc60' \
    "$run_dir/main-1.log"
grep -Fqx 'SDL live test settled step=30 generation=4646 crc32=7ef957e9' \
    "$run_dir/main-1.log"
grep -Fqx 'SDL live test settled step=31 generation=4770 crc32=500b350f' \
    "$run_dir/main-1.log"
grep -Fqx 'stop=user pc=0x0800009e instructions=9487528672 virtual_time_ns=37414100700' \
    "$run_dir/main-1.log"
# GPU refusals in this window are gone: the ticket-788 codec family
# (E-SAP-0041-EXT6, twice-reproduced census and residual one-ULP law)
# admits every bounce quad, so the main screen settles with ZERO
# draw-refused events and zero faults/resets; the terminal is the
# scripted user quit.
if grep -q 'nema_tsc6a: unsupported resolve state' "$run_dir/main-1.log"; then
    echo 'FAIL: resolve-state refusals returned after the EXT4 extension'
    exit 1
fi
[ "$(grep -c 'compressed source 60x60 stride 180 is unsupported' "$run_dir/main-1.log")" -eq 0 ]
[ "$(grep -c 'subsystem=gpu event=draw-refused' "$run_dir/main-1.log")" -eq 0 ]
[ "$(grep -c 'event=machine-reset-request' "$run_dir/main-1.log")" -eq 0 ]
[ "$(shasum -a 256 "$run_dir/main-1.log" | awk '{print $1}')" = \
    14ffff66146dfce6fabbb57ee2f96917431c02061d6c172ffac731b9adaa9aae ]
echo 'PASS sapporo-2.35.34 main-entry compressed render at derived boundary'
