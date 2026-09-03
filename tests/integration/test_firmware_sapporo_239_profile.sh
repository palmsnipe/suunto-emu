#!/bin/sh
# TEST_TAGS: sapporo_239_profile
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP sapporo 2.39 profile runner for TEST_PROFILE=${TEST_PROFILE-}"
    exit 0
fi

profile=$(${SEMU_EMULATOR-} show-profile sapporo-2.39.20)
printf '%s\n' "$profile" | grep -F -x -q 'id=sapporo-2.39.20'
printf '%s\n' "$profile" | grep -F -x -q 'version=2.39.20.22297-P'
printf '%s\n' "$profile" | grep -F -q \
    'sha256=85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89'
echo "PASS sapporo-2.39.20 built-in profile contract"
