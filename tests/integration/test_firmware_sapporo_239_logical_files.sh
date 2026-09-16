#!/bin/sh
# TEST_TAGS: sapporo_239_logical_files
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 logical-files runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
expected_flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
# E-ULS-0047 (ticket 777) re-derivation of the artifact hashes only, from two
# byte-identical runs. The era drift recorded by E-ULS-0041 (accepted
# integration batch d311da0..6555d38) changed log bytes at unpinned per-event
# fields; the boundary stop line, 118-intervention census, trigger counts,
# retained-file events, and the snapshot-resume continuation are unchanged.
expected_log_hash=4b96c1ba019787054179ee691e5a2ac2535f6e18111111f432113f20d0338591
expected_snapshot_hash=c1ea5c288fa6be6f6e1adbb60376fb7a74cbf7f4bc2db50a795edd04363c26ba

if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 logical-files runner: set SEMU_SAPPORO_239_FULL_FLASH"
    exit 0
fi
if [ ! -f "$full_flash" ] || [ ! -r "$full_flash" ]; then
    echo "error: Sapporo 2.39 full flash is not readable: $full_flash" >&2
    exit 2
fi
flash_hash=$(shasum -a 256 "$full_flash" | awk '{print $1}')
if [ "$flash_hash" != "$expected_flash_hash" ]; then
    echo "error: Sapporo 2.39 full flash hash mismatch" >&2
    echo "expected: $expected_flash_hash" >&2
    echo "actual:   $flash_hash" >&2
    exit 2
fi

run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-files.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

run_once()
{
    output=$1
    snapshot=$2
    if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --until normal-frame --max-instructions 78868137 \
        --max-time 30000000000 --snapshot-save "$snapshot" \
        >"$output" 2>&1;
    then
        run_status=0
    else
        run_status=$?
    fi
    if [ "$run_status" -ne 3 ]; then
        echo "error: Sapporo 2.39 logical-files run returned $run_status" >&2
        cat "$output" >&2
        exit 1
    fi
}

if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --until normal-frame \
    --max-instructions 72774982 --max-time 30000000000 \
    >"$run_dir/layer-off.log" 2>&1;
then
    layer_off_status=0
else
    layer_off_status=$?
fi
if [ "$layer_off_status" -ne 3 ] || ! grep -F -x -q \
    'stop=budget pc=0x00079e1e instructions=72774982 virtual_time_ns=521257564' \
    "$run_dir/layer-off.log";
then
    echo "error: Sapporo 2.39 layer-off checkpoint changed" >&2
    cat "$run_dir/layer-off.log" >&2
    exit 1
fi

run_once "$run_dir/first.log" "$run_dir/first.sems"
run_once "$run_dir/second.log" "$run_dir/second.sems"
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log" ||
   ! cmp -s "$run_dir/first.sems" "$run_dir/second.sems"; then
    echo "error: Sapporo 2.39 logical-files runs differ" >&2
    diff -u "$run_dir/first.log" "$run_dir/second.log" >&2 || true
    exit 1
fi
if [ "$(shasum -a 256 "$run_dir/first.log" | awk '{print $1}')" != \
     "$expected_log_hash" ] ||
   [ "$(shasum -a 256 "$run_dir/first.sems" | awk '{print $1}')" != \
     "$expected_snapshot_hash" ]; then
    echo "error: Sapporo 2.39 logical-files artifact hash changed" >&2
    cat "$run_dir/first.log" >&2
    exit 1
fi
if [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 118 ]; then
    echo "error: Sapporo 2.39 logical-file intervention count changed" >&2
    cat "$run_dir/first.log" >&2
    exit 1
fi
for trigger in wbsto-session-cache wbsto-preload-result; do
    if [ "$(grep -c "trigger=$trigger ordinal=1" "$run_dir/first.log")" -ne 1 ]; then
        echo "error: Sapporo 2.39 intervention $trigger count changed" >&2
        cat "$run_dir/first.log" >&2
        exit 1
    fi
done
for event in \
    'operation=close path=settings/sync.txt result=1 size=0 cursor=0' \
    'operation=close path=settings/uiv2.txt result=1 size=235 cursor=235' \
    'operation=close path=settings/general result=1 size=1505 cursor=1505'; do
    if ! grep -F -q "$event" "$run_dir/first.log"; then
        echo "error: missing Sapporo 2.39 retained file event: $event" >&2
        cat "$run_dir/first.log" >&2
        exit 1
    fi
done
if grep -E -q '0x0f676e34|event=machine-reset-request|compat-refused|unknown Sapporo' \
    "$run_dir/first.log" || ! grep -F -x -q \
    'stop=budget pc=0x000be522 instructions=78868137 virtual_time_ns=520697206' \
    "$run_dir/first.log";
then
    echo "error: Sapporo 2.39 logical-files checkpoint changed" >&2
    cat "$run_dir/first.log" >&2
    exit 1
fi

if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
    --until normal-frame --max-instructions 78868138 \
    --max-time 30000000000 --snapshot-load "$run_dir/first.sems" \
    >"$run_dir/resume.log" 2>&1;
then
    resume_status=0
else
    resume_status=$?
fi
if [ "$resume_status" -ne 3 ] || grep -F -q \
    'event=machine-reset-request pc=0x000d2f6e' "$run_dir/resume.log" ||
   ! grep -F -x -q \
    'stop=budget pc=0x000be524 instructions=78868138 virtual_time_ns=520697207' \
    "$run_dir/resume.log";
then
    echo "error: Sapporo 2.39 logical-files snapshot resume changed" >&2
    cat "$run_dir/resume.log" >&2
    exit 1
fi
if [ "$(shasum -a 256 "$full_flash" | awk '{print $1}')" != \
     "$expected_flash_hash" ]; then
    echo "error: Sapporo 2.39 source flash was modified" >&2
    exit 1
fi

echo "PASS sapporo-2.39.20 logical writable-files checkpoint"
