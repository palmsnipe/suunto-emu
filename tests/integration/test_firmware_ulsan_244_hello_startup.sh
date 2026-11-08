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
expect_refusal 'run: Ulsan GPS requires the production-data and eMMC layers' \
    --layer ulsan-2.44-gps-startup
printf x > "$work/short"
expect_refusal 'run: Ulsan eMMC fixture size mismatch' \
    --layer ulsan-2.44-emmc --ulsan-emmc "$work/short"

run_boot()
{
    log=$1
    instructions=$2
    time_ns=$3
    rc=0
    run --layer ulsan-2.44-production-data --layer ulsan-2.44-emmc \
        --ulsan-emmc "$image" --max-instructions "$instructions" \
        --max-time "$time_ns" > "$log" 2>&1 || rc=$?
    if [ "$rc" -ne 3 ]; then
        cat "$log" >&2
        return 1
    fi
}

check_boots()
{
    run_boot "$work/first" "$1" "$2"
    run_boot "$work/second" "$1" "$2"
    cmp "$work/first" "$work/second"
    test "$(wc -l < "$work/first" | tr -d ' ')" -eq 3
    grep -Fq 'time_ns=0 level=warning subsystem=compat event=layer-hit layer=ulsan-2.44-production-data hit=1 maximum=1 ' "$work/first"
    grep -Fq 'time_ns=0 level=warning subsystem=compat event=layer-hit layer=ulsan-2.44-emmc hit=1 maximum=1 ' "$work/first"
    test "$(tail -1 "$work/first")" = "$3"
    cat "$work/first"
}

# E-ULS-0082. This bounded checkpoint is before the first NemaGPU access.
check_boots 85000000 2000000000 \
    'stop=budget pc=0x000b22cc instructions=85000000 virtual_time_ns=558718255'
# E-ULS-0087. GPU initialization and the IOM startup have completed here.
check_boots 420000000 16000000000 \
    'stop=budget pc=0x000d47f0 instructions=327430360 virtual_time_ns=16014847988'
echo 'PASS ulsan-2.44.52 Hello startup through GPU initialization and sensor startup'
