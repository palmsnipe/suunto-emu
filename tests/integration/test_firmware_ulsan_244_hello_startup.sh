#!/bin/sh
# TEST_TAGS: ulsan_244_hello_startup
set -eu

if [ "${TEST_PROFILE-}" != ulsan-2.44.52 ]; then
    echo "SKIP ulsan 2.44.52 Hello startup runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

image=${SEMU_ULSAN_244_EMMC_IMAGE-}
if [ -z "$image" ]; then
    echo 'SKIP ulsan 2.44.52 Hello startup: set SEMU_ULSAN_244_EMMC_IMAGE'
    exit 0
fi

emulator=${SEMU_EMULATOR:?}
manifest=${SEMU_FIRMWARE_MANIFEST:?}
work=$(mktemp -d "${TMPDIR:-/tmp}/semu-ulsan-244-hello.XXXXXX")
trap 'rm -f "$work/first" "$work/second" "$work/short"; rmdir "$work"' EXIT

run()
{
    "$emulator" run --profile ulsan-2.44.52 --firmware "$manifest" "$@"
}

expect_refusal()
{
    expected=$1
    shift
    rc=0
    result=$(run --max-instructions 1 --max-time 1 "$@" 2>&1) || rc=$?
    if [ "$rc" -ne 2 ] || [ "$result" != "$expected" ]; then
        printf '%s\n' "$result" >&2
        return 1
    fi
}

missing='run: Ulsan eMMC requires both the named layer and --ulsan-emmc'
expect_refusal "$missing" --layer ulsan-2.44-emmc
expect_refusal "$missing" --ulsan-emmc "$image"
printf x > "$work/short"
expect_refusal 'run: Ulsan eMMC fixture size mismatch' \
    --layer ulsan-2.44-emmc --ulsan-emmc "$work/short"

run_boot()
{
    rc=0
    run --layer ulsan-2.44-production-data --layer ulsan-2.44-emmc \
        --ulsan-emmc "$image" --max-instructions 85000000 \
        --max-time 2000000000 > "$1" 2>&1 || rc=$?
    if [ "$rc" -ne 3 ]; then
        cat "$1" >&2
        return 1
    fi
}

run_boot "$work/first"
run_boot "$work/second"
cmp "$work/first" "$work/second"
# E-ULS-0082. This bounded checkpoint is before the first NemaGPU access.
test "$(wc -l < "$work/first" | tr -d ' ')" -eq 3
grep -Fq 'time_ns=0 level=warning subsystem=compat event=layer-hit layer=ulsan-2.44-production-data hit=1 maximum=1 ' "$work/first"
grep -Fq 'time_ns=0 level=warning subsystem=compat event=layer-hit layer=ulsan-2.44-emmc hit=1 maximum=1 ' "$work/first"
test "$(tail -1 "$work/first")" = \
    'stop=budget pc=0x000b22cc instructions=85000000 virtual_time_ns=558718255'
cat "$work/first"
echo 'PASS ulsan-2.44.52 Hello startup through SDIO and TIMER7'
