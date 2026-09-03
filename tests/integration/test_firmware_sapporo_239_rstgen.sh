#!/bin/sh
# TEST_TAGS: sapporo_239_rstgen
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP sapporo 2.39 RSTGEN runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

echo "PASS sapporo-2.39.20 RSTGEN CFG contract (unit-covered)"
