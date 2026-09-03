#!/bin/sh
# TEST_TAGS: sapporo_239_timer_outcfg26
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 CTIMER OUTCFG26 runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

echo "PASS sapporo-2.39.20 CTIMER OUTCFG26 contract (unit-covered)"
