#!/bin/sh
set -eu

renode=${RENODE-}
runner_dir=${DIFFERENTIAL_RUNNER_DIR:-tests/differential}
emulator=${SEMU_EMULATOR-}
filter=${TEST_FILTER-}

case "$filter" in
    *[!A-Za-z0-9_.-]*)
        echo "error: TEST_FILTER must contain only letters, digits, '.', '_' or '-'" >&2
        exit 2
        ;;
esac

if [ -z "$renode" ]; then
    echo "SKIP differential tests: set RENODE to a Renode executable" >&2
    exit 0
fi
case "$renode" in
    */*)
        if [ ! -x "$renode" ]; then
            echo "error: RENODE is not executable: $renode" >&2
            exit 2
        fi
        ;;
    *)
        if ! command -v "$renode" >/dev/null 2>&1; then
            echo "error: RENODE command was not found: $renode" >&2
            exit 2
        fi
        ;;
esac

matched=0
for runner in "$runner_dir"/test_*.sh; do
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
    echo "DIFFERENTIAL TEST $runner"
    RENODE=$renode SEMU_EMULATOR=$emulator sh "$runner"
done

if [ "$matched" -eq 0 ]; then
    if [ -n "$filter" ]; then
        echo "SKIP differential tests: no runner matched TEST_FILTER='$filter'" >&2
    else
        echo "SKIP differential tests: no runners in $runner_dir" >&2
    fi
fi
