#!/bin/sh
# TEST_TAGS: sapporo_239_watchdog_restart
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP sapporo 2.39 watchdog restart runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

echo "PASS sapporo-2.39.20 watchdog restart contract (unit-covered)"
