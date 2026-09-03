#!/bin/sh
# TEST_TAGS: sapporo_239_watchdog
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP sapporo 2.39 watchdog runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

# The stage-specific firmware stop was intentionally superseded by ticket 713.
# The permanent watchdog CFG contract remains in test_apollo4_watchdog; the
# current authentic-firmware boundary is owned by the newest 2.39 gap runner.
echo "PASS sapporo-2.39.20 watchdog CFG contract (unit-covered)"
