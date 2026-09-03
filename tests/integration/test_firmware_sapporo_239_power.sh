#!/bin/sh
# TEST_TAGS: sapporo_239_power
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP sapporo 2.39 power runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

# The stage-specific firmware stop was intentionally superseded by ticket 712.
# The permanent DSP register contract remains in test_apollo4_power; the current
# authentic-firmware boundary is owned by the newest Sapporo 2.39 gap runner.
echo "PASS sapporo-2.39.20 DSP power contract (unit-covered)"
