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
provenance=${snapshot}.provenance
checkpoint=${3:-${SEMU_SAPPORO_UI_CHECKPOINT:-middle-language}}
refresh=${SEMU_SAPPORO_UI_REFRESH:-0}
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

case "$refresh" in
    0|1) ;;
    *)
        echo "SEMU_SAPPORO_UI_REFRESH must be 0 or 1" >&2
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

library="$build_root/libsemu.a"
helper="$root_dir/tools/run_sapporo_ui.sh"
if command -v shasum >/dev/null 2>&1; then
    hash_tool=shasum
elif command -v sha256sum >/dev/null 2>&1; then
    hash_tool=sha256sum
else
    echo "a SHA-256 utility (shasum or sha256sum) is required" >&2
    exit 2
fi

hash_file()
{
    case "$hash_tool" in
        shasum) shasum -a 256 "$1" | awk '{print $1}' ;;
        sha256sum) sha256sum "$1" | awk '{print $1}' ;;
    esac
}

provenance_value()
{
    if [ -r "$provenance" ]; then
        sed -n "s/^$1=//p" "$provenance" | head -n 1
    fi
}

load_provenance_inputs()
{
    manifest_hash=$(hash_file "$manifest")
    sdl_hash=$(hash_file "$sdl")
    helper_hash=$(hash_file "$helper")
    stored_headless_hash=$(provenance_value headless_sha256)
    stored_library_hash=$(provenance_value libsemu_sha256)
    if [ -x "$headless" ]; then
        headless_hash=$(hash_file "$headless")
    elif [ -n "$stored_headless_hash" ]; then
        headless_hash=$stored_headless_hash
    else
        headless_hash=unavailable
    fi
    if [ -r "$library" ]; then
        library_hash=$(hash_file "$library")
    elif [ -n "$stored_library_hash" ]; then
        library_hash=$stored_library_hash
    else
        library_hash=unavailable
    fi
}

provenance_contents()
{
    printf '%s\n' \
        "version=1" \
        "manifest_sha256=$manifest_hash" \
        "headless_sha256=$headless_hash" \
        "sdl_sha256=$sdl_hash" \
        "libsemu_sha256=$library_hash" \
        "helper_sha256=$helper_hash" \
        "profile=sapporo-2.22.60" \
        "layer=sapporo-2.22-no-device" \
        "capture_instructions=450800000" \
        "capture_time_ns=22000000000" \
        "snapshot_sha256=$snapshot_hash"
}

rebuild_snapshot=0
snapshot_reason=""
if [ "$refresh" -eq 1 ]; then
    rebuild_snapshot=1
    snapshot_reason="explicit refresh"
elif [ ! -s "$snapshot" ]; then
    rebuild_snapshot=1
    snapshot_reason="missing snapshot"
elif [ -x "$headless" ] && [ "$headless" -nt "$snapshot" ]; then
    rebuild_snapshot=1
    snapshot_reason="headless binary is newer"
elif [ "$sdl" -nt "$snapshot" ]; then
    rebuild_snapshot=1
    snapshot_reason="SDL binary is newer"
fi

if [ "$rebuild_snapshot" -eq 0 ]; then
    load_provenance_inputs
    snapshot_hash=$(hash_file "$snapshot")
    expected_provenance=$(provenance_contents)
    actual_provenance=$(cat "$provenance" 2>/dev/null || true)
    if [ "$actual_provenance" != "$expected_provenance" ]; then
        rebuild_snapshot=1
        snapshot_reason="provenance mismatch"
    fi
fi

if [ "$rebuild_snapshot" -eq 1 ]; then
    if [ ! -x "$headless" ]; then
        echo "headless binary is required to create: $snapshot" >&2
        echo "build both binaries first with: make" >&2
        exit 2
    fi
    load_provenance_inputs
    snapshot_tmp=$(mktemp "${snapshot}.tmp.XXXXXX")
    log_tmp=$(mktemp "${snapshot}.log.XXXXXX")
    provenance_tmp=""
    cleanup_snapshot_files()
    {
        rm -f "$snapshot_tmp" "$log_tmp"
        if [ -n "$provenance_tmp" ]; then
            rm -f "$provenance_tmp"
        fi
    }
    trap cleanup_snapshot_files EXIT HUP INT TERM
    echo "creating Sapporo UI checkpoint: $snapshot ($snapshot_reason)" >&2
    set +e
    SEMU_FIRMWARE_MANIFEST="$manifest" "$headless" run \
        --profile sapporo-2.22.60 --firmware "$manifest" \
        --layer sapporo-2.22-no-device \
        --max-instructions 450800000 --max-time 22000000000 \
        --snapshot-save "$snapshot_tmp" >"$log_tmp" 2>&1
    status=$?
    set -e
    cat "$log_tmp" >&2
    if [ "$status" -ne 3 ] || [ ! -s "$snapshot_tmp" ] ||
       ! grep -q '^stop=budget ' "$log_tmp"; then
        echo "failed to create Sapporo UI checkpoint" >&2
        exit 2
    fi
    snapshot_hash=$(hash_file "$snapshot_tmp")
    if [ -z "$snapshot_hash" ]; then
        echo "failed to hash Sapporo UI checkpoint" >&2
        exit 2
    fi
    provenance_tmp=$(mktemp "${provenance}.tmp.XXXXXX")
    provenance_contents >"$provenance_tmp"
    mv -f "$snapshot_tmp" "$snapshot"
    mv -f "$provenance_tmp" "$provenance"
fi

exec env SEMU_FIRMWARE_MANIFEST="$manifest" "$sdl" run \
    --profile sapporo-2.22.60 --firmware "$manifest" \
    --layer sapporo-2.22-no-device --until "$checkpoint" \
    --snapshot-load "$snapshot" --wait-for-quit \
    --max-instructions 14000000000 --max-time 22000000000
