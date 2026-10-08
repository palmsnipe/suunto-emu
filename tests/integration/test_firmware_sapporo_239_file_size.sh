#!/bin/sh
# TEST_TAGS: sapporo_239_file_size
set -eu

if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 logical file size: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
# Ticket 777 B2 cap re-derivation (technique per E-SAP239-OHR2-REPIN-001).
# The 797-era relocation moved the tssln/tss.bin size transaction (the newest
# pinned transcript event) from instruction 405895301 to 414252829, still far
# below the E-SAP239-ERA-CLASSIFY-001 B1 wall at 442856246. The cap advanced
# by +/-1 bisection to the transaction's first-appearance instruction (absent
# at 414252828, present at 414252829; new cold stop vt equals the event time
# 2038505656 plus 1 ns). Both size transcripts occur verbatim; the intervention
# census at the cap is now 506; stop lines and hashes re-pinned verbatim from
# two byte-identical runs. Guard greps unchanged and byte-identical.
log_hash=70c922e831714b1fdc97d2e6063b086dc16368fcec8c54a3fdc249f9f02218c2
snapshot_hash=a236b6452532a9d7f7a8170a905c1fa47867615fb840c237fae63feab84c7cbb
if [ -z "$full_flash" ]; then
    echo "SKIP Sapporo 2.39 logical file size: set SEMU_SAPPORO_239_FULL_FLASH"
    exit 0
fi
hash()
{
    shasum -a 256 "$1" | awk '{print $1}'
}
if [ ! -r "$full_flash" ] || [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: Sapporo 2.39 full flash unavailable or hash mismatch" >&2
    exit 2
fi
# Check every manifest component before either authentic execution.
"$emulator" validate --profile sapporo-2.39.20 --firmware "$manifest"
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-file-size.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM

run_once()
{
    if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
        --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
        --until normal-frame --max-instructions 414252829 \
        --max-time 30000000000 --snapshot-save "$run_dir/$1.sems" \
        >"$run_dir/$1.log" 2>&1;
    then
        code=0
    else
        code=$?
    fi
    if [ "$code" -ne 3 ]; then
        echo "error: logical file size checkpoint returned $code" >&2
        cat "$run_dir/$1.log" >&2
        exit 1
    fi
}
run_once first
run_once second
if ! cmp -s "$run_dir/first.log" "$run_dir/second.log" ||
   ! cmp -s "$run_dir/first.sems" "$run_dir/second.sems" ||
   [ "$(hash "$run_dir/first.log")" != "$log_hash" ] ||
   [ "$(hash "$run_dir/first.sems")" != "$snapshot_hash" ]; then
    echo "error: logical file size checkpoint artifacts differ" >&2
    exit 1
fi
for expected in \
    'operation=size path=sleepln/sleep.bin result=17888 size=17888 cursor=24' \
    'operation=size path=tssln/tss.bin result=2384 size=2384 cursor=24'
do
    if ! grep -F -q "$expected" "$run_dir/first.log"; then
        echo "error: missing logical file size transcript: $expected" >&2
        exit 1
    fi
done
if ! grep -F -x -q \
    'stop=budget pc=0x0016f862 instructions=414252829 virtual_time_ns=2038505657' \
    "$run_dir/first.log" ||
   grep -E -q 'event=machine-reset-request|compat-refused|status=refuse' \
    "$run_dir/first.log" ||
   [ "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/first.log")" -ne 506 ]; then
    echo "error: logical file size execution boundary changed" >&2
    exit 1
fi

# Resume from the pinned boundary; the next guest instruction executes exactly
# as recorded, stopping at the budget cap with exit code 3 (E-ULS-0041 precedent).
if "$emulator" run --profile sapporo-2.39.20 --firmware "$manifest" \
    --full-flash "$full_flash" --layer sapporo-2.39-synthetic-wbsto \
    --until normal-frame --max-instructions 414252830 \
    --max-time 30000000000 --snapshot-load "$run_dir/first.sems" \
    >"$run_dir/resume.log" 2>&1;
then
    code=0
else
    code=$?
fi
if [ "$code" -ne 3 ] || ! grep -F -x -q \
    'stop=budget pc=0x0016f864 instructions=414252830 virtual_time_ns=2038505658' \
    "$run_dir/resume.log"; then
    echo "error: post-file-size boundary continuation changed" >&2
    cat "$run_dir/resume.log" >&2
    exit 1
fi
if [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: source flash was modified" >&2
    exit 1
fi
echo "PASS sapporo-2.39.20 logical file size checkpoint and boundary continuation"
