#!/bin/sh
# TEST_TAGS: sapporo_233_files sapporo_233_pressure sapporo_233_ohr sapporo_233_guard
# E-SAP-0072: the logical-file READ/WRITE ABI clears the settings assertion.
# E-SAP-0074: negative pressure probes complete; the later OHR/reset refuses.
# E-SAP-0075: the OHR startup fixture admits the staged exchange; the next
# stop is the pois/poi.bin logical-file capacity boundary.
# E-SAP-0119: the opt-in sentinel-insert guard prevents the E-SAP-0092
# self-ring; the guarded boot advances past the former 37.82 s spin stop
# (pc=0x000bb6b0) to a healthy poll-loop stop at the 2B-instruction bound.
# E-SAP-0123: the logical-files layer seeds the observed 772-byte
# storage/7ab0dd65/data.jsn content; the pre-write load at vt 278 ms is
# admitted and read (772 bytes) instead of the native size-0 open, shifting
# the stage stops/instruction counts and the guarded log hash (re-pinned
# below, both passes byte-identical).
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.33.16 ]; then
    echo "SKIP Sapporo 2.33 files runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi
emulator=${SEMU_EMULATOR:?}
manifest=${SEMU_FIRMWARE_MANIFEST:?}
"$emulator" validate --profile sapporo-2.33.16 --firmware "$manifest"
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-233-files.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
run_once()
{
    output=$1
    instructions=$2
    time_ns=$3
    shift 3
    if "$emulator" run --profile sapporo-2.33.16 --firmware "$manifest" \
        --max-instructions "$instructions" --max-time "$time_ns" \
        "$@" > "$output" 2>&1; then
        code=0
    else
        code=$?
    fi
    if [ "$code" -ne 3 ]; then
        cat "$output" >&2
        exit 1
    fi
}
staged_once()
{
    run_once "$1" "$2" "$3" \
        --layer sapporo-2.33-production-data \
        --layer sapporo-2.33-logical-files \
        --layer sapporo-2.33-firstboot \
        --layer sapporo-2.33-gps-startup
}
staged_ohr_once()
{
    run_once "$1" "$2" "$3" \
        --layer sapporo-2.33-production-data \
        --layer sapporo-2.33-logical-files \
        --layer sapporo-2.33-firstboot \
        --layer sapporo-2.33-gps-startup \
        --layer sapporo-2.33-ohr-startup
}
guarded_once()
{
    run_once "$1" "$2" "$3" \
        --layer sapporo-2.33-production-data \
        --layer sapporo-2.33-logical-files \
        --layer sapporo-2.33-firstboot \
        --layer sapporo-2.33-gps-startup \
        --layer sapporo-2.33-ohr-startup \
        --layer sapporo-2.33-sentinel-guard
}
for pass in 1 2; do
    run_once "$run_dir/control-$pass.log" 200000000 200000000
    staged_once "$run_dir/settings-$pass.log" 70000000 1000000000
    staged_once "$run_dir/pressure-$pass.log" 200000000 1000000000
    staged_once "$run_dir/next-$pass.log" 300000000 30000000000
    staged_ohr_once "$run_dir/ohr-$pass.log" 2000000000 60000000000
    guarded_once "$run_dir/guarded-$pass.log" 2000000000 60000000000
done
for stage in control settings pressure next ohr guarded; do
    cmp "$run_dir/$stage-1.log" "$run_dir/$stage-2.log"
done
[ "$(shasum -a 256 "$run_dir/control-1.log" | awk '{print $1}')" = \
    74c858187b0fcc1c4b33729b9df7194d57e851edf13c079391308c7169e67a33 ]
[ "$(shasum -a 256 "$run_dir/settings-1.log" | awk '{print $1}')" = \
    6736ff1a1c8999d8f7b9e634c899754e460a076854f11ac8074aab9e70af3296 ]
# E-SAP-0074 retires E-SAP-0072's pressure-fault suffix (8a5e06b0...).
# Preserve both earlier prefix pins and pin success before the new OHR limit.
[ "$(shasum -a 256 "$run_dir/pressure-1.log" | awk '{print $1}')" = \
    6c3e71a8f6706f9e7a6e5f7e4543f6e21ea810fb9449bb6d2749765b112f63b6 ]
# E-SAP-0078 re-pin: the stop tuple is unchanged; only the fixture's
# diagnostic budget text moved (maximum=6 with the re-arm admissions).
[ "$(shasum -a 256 "$run_dir/next-1.log" | awk '{print $1}')" = \
    5408ccff23457661e6825a93d5a050514ba79812439a07e6f71226695809d6e1 ]
grep -Fqx 'stop=budget pc=0x000dbc0a instructions=48412879 virtual_time_ns=339421948' "$run_dir/control-1.log"
grep -Fqx 'stop=budget pc=0x000c892c instructions=70000000 virtual_time_ns=405355276' "$run_dir/settings-1.log"
grep -Fq 'operation=close path=settings/uiv2.txt result=1 size=256 cursor=0' "$run_dir/settings-1.log"
grep -Fq 'operation=close path=settings/general result=1 size=1487 cursor=0' "$run_dir/settings-1.log"
grep -Fqx 'stop=budget pc=0x000dbc0a instructions=93149170 virtual_time_ns=1000000000' "$run_dir/pressure-1.log"
for stage in settings pressure; do
    if grep -Eq 'wui_dump.bin|machine-reset-request' "$run_dir/$stage-1.log"; then
        echo "error: 2.33 $stage assertion/reset regressed" >&2
        exit 1
    fi
done
grep -Fq 'kind=request command=0x0010 sequence=0 state=BSL status=refuse ready=0' "$run_dir/next-1.log"
grep -Fq 'fault_address=0x40052120' "$run_dir/next-1.log"
grep -Fqx 'stop=device-refused pc=0x000c97f2 instructions=525319890 virtual_time_ns=1800546345 detail=2.33 ui.txt seed already populated' "$run_dir/next-1.log"
# E-SAP-0076: the OHR layer admits all seven staged exchanges in the lane
# order (hit 7 pinned); the former OHR refusal stays retired. With the
# E-SAP-0075 capacity correction the boot continues past the pois write
# stream to the GPS fixture's third-arm refusal.
grep -Fq 'layer-hit layer=sapporo-2.33-ohr-startup hit=7 maximum=7 effect=trigger=ohr-startup ordinal=7 command=0006 sequence=6 synthetic-body evidence=E-SAP-0075' "$run_dir/ohr-1.log"
if grep -q 'ui.txt seed already populated' "$run_dir/ohr-1.log"; then
    echo "error: 2.33 OHR round regressed to the seed refusal" >&2
    exit 1
fi
if grep -q 'logical-file size overflow: pois/poi.bin' "$run_dir/ohr-1.log"; then
    echo "error: 2.33 capacity regression" >&2
    exit 1
fi
grep -Fq 'layer-hit layer=sapporo-2.33-gps-startup hit=6 maximum=6 effect=trigger=gps-rearm-status ordinal=3 synthetic-status delay-ns=10000000 evidence=E-SAP-0071' "$run_dir/ohr-1.log"
grep -Fqx 'stop=budget pc=0x000bb6b0 instructions=2000000000 virtual_time_ns=37821258942' "$run_dir/ohr-1.log"
# E-SAP-0119: the guard's first skip lands on the witnessed corrupting
# insert at 37.108419034 s; the 1,000,000th milestone follows at
# 37.517418625 s (~2.44M skips/s, inside the 3M/s rate cap). The former
# spin stop (pc=0x000bb6b0) is retired and the poll loop stays healthy.
[ "$(shasum -a 256 "$run_dir/guarded-1.log" | awk '{print $1}')" = \
    27c1fec53024a3cc3b13554ba2e246724ace0b2a446dfec2abb667fae3b71530 ]
grep -Fq 'layer-hit layer=sapporo-2.33-sentinel-guard hit=1 maximum=360000000 effect=trigger=sentinel-insert ordinal=1 skip pc=0x000a4e8a virtual_time_ns=37108419034 evidence=E-SAP-0092..E-SAP-0107' "$run_dir/guarded-1.log"
grep -Fq 'layer-hit layer=sapporo-2.33-sentinel-guard hit=1000000 maximum=360000000 effect=trigger=sentinel-insert ordinal=1000000 skip pc=0x000a4e8a virtual_time_ns=37517418625 evidence=E-SAP-0092..E-SAP-0107' "$run_dir/guarded-1.log"
grep -Fqx 'stop=budget pc=0x000925b4 instructions=2000000000 virtual_time_ns=37821258942' "$run_dir/guarded-1.log"
if grep -q 'pc=0x000bb6b0' "$run_dir/guarded-1.log"; then
    echo "error: 2.33 guarded boot regressed to the E-SAP-0092 spin" >&2
    exit 1
fi
echo 'PASS sapporo-2.33.16 native settings, negative pressure and OHR startup probes; bounded logical-file refusal; sentinel-insert guard advances past the spin'
