#!/bin/sh
# Optional private-firmware regression: ticket 710 slice 4 (E-SAP-0041-EXT3
# era) — Sapporo 2.35 main-screen BUTTON NAVIGATION acceptance windows.
#
# Golden policy (offline-RE button census, volatile artifact
# /tmp/sap235-btn/notes.md sha256 8f1fd1391233b082311bacf3c8173f8498f2ea39bb294b3b18200f76992d6bf0):
# the firmware-BACKED chain is click -> 30 ms poll -> button codes -> UI
# dispatch -> widget handler -> carousel step (5000 ms settle timer) -> list
# move -> invalidate -> repaint. The per-key direction labels below are
# EMULATOR REGRESSION GOLDENS ONLY (observation-only): every transition table
# row was captured from this emulator twice byte-identically; the lane never
# observed a post-Done click. Do not read per-key visible-region correctness
# into these pins beyond the backed chain.
#
# Observation-only transition table (zero-hold click on the settling main
# screen, five-layer setup-walk, 40 s budget window; re-derived post-EXT4
# and post-788 — the ticket-794 accent raster changes the settling-frame
# crc space from step 25, and the ticket-788 codec admission rasters the
# compressed-source bounce repaints so settled generations drift further
# (host-side only); every guest stop line and the navigation frame crc
# stay byte-identical):
# Ticket 801 / E-EMU-SAP235-NAVIGATION-002 corrects the old interpretation:
# these are multi-click windows, not isolated buttons from the watchface.
# MIDDLE alternates the watchface and the static widget-pinning prompt.
# LOWER sends Middle at step 25 before Lower at step 26. CRC 7ef957e9 is
# the pinning prompt; no repaint is expected from the observed lower press.
# The frame-gated script then waits and supplies no further input. A later
# Middle press returns to the watchface in the independent restored test.
# UPPER reaches the Exercise menu (9b554fd9), then subsequent input reaches
# the exercise-entry sequence. Ticket 803 extends the former OHR refusal.
# Baseline, LOWER and the old UPPER prefix/frame assertions remain unchanged.
# Re-derived 2026-09-30 after the ticket-710 tsc6a frame-lifecycle fix
# (E-EMU-SAP235-TICKTRAIL-001): the seconds-sweep frames re-derive clean;
# the guest stop lines, generations and refusal guards are unchanged.
# Ticket 710 instance (E-SAP-0041-EXT6 named residual, closed): the
# upper window's 4 vertically-scrolling draws (full-width 60px rects at
# x 171..231, heights 11/49/48/20, clip-cut ends, mm12 = -dstY
# -151/-151eps/-192/-220, mm02 -171 exact; census /tmp/sap235-vscroll
# twice byte-identical, pre-admission log be0e2e1c…) are admitted through
# the composer band law (RE 0xc1b5e set-matrix API, MM12 = -dstY with
# the quad as the strip/clip cut). Post-admission: zero draw-refused,
# every guest stop byte and the navigation frame crc unchanged, only
# step 28's host-side generation drifts 4345 -> 4349; zero resets.
set -eu
emulator=${SEMU_SDL_EMULATOR:-build/suunto-emu-sdl}
manifest=${SEMU_FIRMWARE_MANIFEST:-tests/private/sapporo-2.35.34.18929/firmware.semu}
if [ ! -f "$manifest" ]; then
    if [ -n "${SEMU_FIRMWARE_MANIFEST-}" ]; then
        echo "error: configured Sapporo 2.35 manifest is unavailable: $manifest" >&2
        exit 2
    fi
    echo 'SKIP Sapporo 2.35 navigation gate: no private firmware manifest'
    exit 0
fi
if [ ! -x "$emulator" ]; then
    echo "error: SDL emulator unavailable: $emulator (run make sdl)" >&2
    exit 2
fi
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sdl-sapporo-235-nav.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

# $1 = pass, $2 = window name, $3 = POST script, $4 = expected exit status.
run_window() {
    SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk \
        SEMU_SDL_SETUP_WALK_POST="$3" \
        "$emulator" run --profile sapporo-2.35.34 --firmware "$manifest" \
        --layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup \
        --layer sapporo-2.35-gps-startup --layer sapporo-2.35-gps-reopen \
        --layer sapporo-2.35-gps-awake --until setup-next \
        --max-instructions 10000000000 --max-time 40000000000 \
        > "$run_dir/$2-$1.log" 2>&1 || rc=$?
    if [ "${rc:-0}" -ne "$4" ]; then
        cat "$run_dir/$2-$1.log" >&2
        echo "error: window $2 pass $1 exited ${rc:-0}, expected $4" >&2
        exit 1
    fi
    rc=0
}

for pass in 1 2; do
    run_window "$pass" baseline 'mmlllmlllmmmmmmmmmmmmmmmmmmmmmm' 0
    run_window "$pass" lower   'mmlllmlllmmmmmlmlmmu' 3
    run_window "$pass" upper   'mmlllmlllmmmmmmummmmm' 0
done
for window in baseline lower upper; do
    cmp "$run_dir/$window-1.log" "$run_dir/$window-2.log"
done
[ "$(shasum -a 256 "$run_dir/baseline-1.log" | awk '{print $1}')" = \
    14ffff66146dfce6fabbb57ee2f96917431c02061d6c172ffac731b9adaa9aae ]
[ "$(shasum -a 256 "$run_dir/lower-1.log" | awk '{print $1}')" = \
    61cbd3e887d533dc8c657fd75066aec2aaaee34b05b3f0a8e117afebc447578c ]
[ "$(shasum -a 256 "$run_dir/upper-1.log" | awk '{print $1}')" = \
    8176cd579aa3718d6597647f1d31d6942affd76d1b9ab734f2ffa95ac97bb906 ]

# Repeated-Middle baseline: the recorded screen pattern carries the walk to its
# natural QUIT (E-SAP-0041-EXT3 natural terminal + EXT4 accent raster +
# EXT6 compressed-bounce admission; matches the era-pinned window).
grep -Fqx 'SDL live test settled step=25 generation=4000 crc32=0e077730' \
    "$run_dir/baseline-1.log"
grep -Fqx 'SDL live test settled step=31 generation=4770 crc32=500b350f' \
    "$run_dir/baseline-1.log"
grep -Fqx 'stop=user pc=0x0800009e instructions=9487528672 virtual_time_ns=37414100700' \
    "$run_dir/baseline-1.log"

# LOWER window: settled steps identical through step 26 (the press letters are
# derived from the POST script: step N presses POST letter N-12), then the
# static pinning prompt — no step 27 or GPU draw was observed after 35 s.
# Preserve this historical control; it does not establish a renderer freeze.
grep -Fqx 'SDL live test settled step=26 generation=4131 crc32=7ef957e9' \
    "$run_dir/lower-1.log"
if grep -Fq 'SDL live test settled step=27' "$run_dir/lower-1.log"; then
    echo 'error: lower-click window changed the historical static-prompt checkpoint' >&2
    exit 1
fi
if grep -Eq 'time_ns=3[5-9][0-9]{9} .*subsystem=gpu' "$run_dir/lower-1.log"; then
    echo 'error: lower-click window changed historical static-prompt GPU activity' >&2
    exit 1
fi
grep -Fqx 'stop=budget pc=0x000e1862 instructions=8000564488 virtual_time_ns=40000000000' \
    "$run_dir/lower-1.log"

# UPPER window: pair holds through step 27, the off-pair navigation frame
# settles at step 28, then exercise selection completes the scripted window.
# Ticket 803 / E-EMU-SAP235-EXERCISE-003 attributes the old refusal suffix;
# every old prefix line, baseline and LOWER log remains unchanged.
grep -Fqx 'SDL live test settled step=27 generation=4258 crc32=0b78f6cf' \
    "$run_dir/upper-1.log"
grep -Fqx 'SDL live test settled step=28 generation=4349 crc32=9b554fd9' \
    "$run_dir/upper-1.log"
grep -Fqx 'stop=user pc=0x000cd0ca instructions=8965104096 virtual_time_ns=36965203686' \
    "$run_dir/upper-1.log"

# Hard constraints over every window: zero machine resets and the OHR tail
# cap of 16 shared poll/exercise hits. No compatibility budget was increased.
for window in baseline lower upper; do
    if grep -q 'machine-reset-request' "$run_dir/$window-1.log"; then
        echo "error: window $window requested a machine reset" >&2
        exit 1
    fi
    hits=$(grep -Ec 'effect=trigger=ohr-(poll|exercise)' "$run_dir/$window-1.log" || true)
    [ "$hits" -le 16 ] || {
        echo "error: window $window used $hits OHR tail polls (cap 16)" >&2
        exit 1
    }
done
echo 'PASS Sapporo 2.35 SDL: main-screen button navigation goldens (repeated-middle, static pin prompt, exercise-entry)'
