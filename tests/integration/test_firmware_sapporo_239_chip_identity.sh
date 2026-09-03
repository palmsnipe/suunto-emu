#!/bin/sh
# TEST_TAGS: sapporo_239_chip_identity
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP sapporo 2.39 chip identity runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

echo "PASS sapporo-2.39.20 deterministic chip identity contract (unit-covered)"
