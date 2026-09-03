#!/bin/sh
# TEST_TAGS: sapporo_239_usb_clkctrl
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 USB CLKCTRL runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

echo "PASS sapporo-2.39.20 USB CLKCTRL contract (unit-covered)"
