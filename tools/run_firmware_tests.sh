#!/bin/sh
set -eu

emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
firmware_root=${FIRMWARE_ROOT-}
profile=${TEST_PROFILE:-sapporo-2.22.60}
filter=${TEST_FILTER-}

case "$profile" in
    ''|*[!A-Za-z0-9_.-]*)
        echo "error: TEST_PROFILE must be a profile id, not a path" >&2
        exit 2
        ;;
esac
case "$filter" in
    *[!A-Za-z0-9_.-]*)
        echo "error: TEST_FILTER must contain only letters, digits, '.', '_' or '-'" >&2
        exit 2
        ;;
esac

if [ -z "$emulator" ] || [ ! -x "$emulator" ]; then
    echo "error: SEMU_EMULATOR is not an executable" >&2
    exit 2
fi
if [ -n "$manifest" ] && [ -n "$firmware_root" ]; then
    echo "error: set only SEMU_FIRMWARE_MANIFEST or FIRMWARE_ROOT, not both" >&2
    exit 2
fi
if [ -z "$manifest" ] && [ -z "$firmware_root" ]; then
    echo "SKIP firmware tests: set SEMU_FIRMWARE_MANIFEST or FIRMWARE_ROOT" >&2
    exit 0
fi

if [ -n "$firmware_root" ]; then
    if [ ! -d "$firmware_root" ]; then
        echo "error: FIRMWARE_ROOT is not a directory: $firmware_root" >&2
        exit 2
    fi
    flat_manifest=$firmware_root/$profile.semu
    nested_manifest=$firmware_root/$profile/firmware.semu
    if [ -f "$flat_manifest" ] && [ -f "$nested_manifest" ]; then
        echo "error: two manifests found for TEST_PROFILE=$profile" >&2
        echo "  $flat_manifest" >&2
        echo "  $nested_manifest" >&2
        exit 2
    elif [ -f "$flat_manifest" ]; then
        manifest=$flat_manifest
    elif [ -f "$nested_manifest" ]; then
        manifest=$nested_manifest
    else
        echo "error: no manifest for TEST_PROFILE=$profile under $firmware_root" >&2
        echo "expected: $flat_manifest or $nested_manifest" >&2
        exit 2
    fi
fi

if [ ! -f "$manifest" ] || [ ! -r "$manifest" ]; then
    echo "error: firmware manifest is not a readable file: $manifest" >&2
    exit 2
fi

echo "FIRMWARE VALIDATE profile=$profile manifest=$manifest"
"$emulator" validate --profile "$profile" --firmware "$manifest"

matched=0
for runner in tests/integration/test_firmware_*.sh; do
    if [ ! -f "$runner" ]; then
        continue
    fi
    if [ -n "$filter" ]; then
        runner_name=$(basename "$runner" .sh)
        if ! echo "$runner_name" | grep -F -q -e "$filter" &&
           ! grep '^# TEST_TAGS:' "$runner" | grep -F -q -e "$filter"; then
            continue
        fi
    fi
    matched=$((matched + 1))
    echo "FIRMWARE TEST $runner"
    SEMU_FIRMWARE_MANIFEST=$manifest TEST_PROFILE=$profile \
        SEMU_EMULATOR=$emulator sh "$runner"
done

if [ -n "$filter" ] && [ "$matched" -eq 0 ]; then
    echo "error: TEST_FILTER='$filter' matched no firmware integration test" >&2
    exit 2
fi
