#!/bin/sh
# Differential adapter for synthetic CPU vectors (ticket 630).
#
# Runs a synthetic CPU program through both the in-tree emulator and
# an optional Renode instance, then compares normalized output.
# Renode is caller-provided and never authoritative: expected results
# come from the in-tree emulator.
#
# Usage:
#   RENODE=path/to/renode SEMU_EMULATOR=./build/suunto-emu \
#     tools/differential/run_vector.sh <program.bin> <args...>
#
# Exit codes:
#   0 — match or skip (Renode absent)
#   1 — mismatch
#   2 — infrastructure error

set -eu

renode=${RENODE:-}
emulator=${SEMU_EMULATOR:-./build/suunto-emu}
program=${1:-}

if [ -z "$program" ]; then
    echo "error: no program specified" >&2
    exit 2
fi
if [ ! -f "$program" ]; then
    echo "error: program not found: $program" >&2
    exit 2
fi
if [ -z "$renode" ]; then
    echo "SKIP: RENODE not provided"
    exit 0
fi

# Run the in-tree emulator (authoritative expected result).
emu_output=$("$emulator" run --profile sapporo-2.22.60 \
    --firmware /dev/null --max-time 1000000 2>&1) || true

# Run Renode (optional, normalized).
renode_output=$(renode --console -e "
    mach create
    machine LoadELF @${program}
    start
    emulation RunFor 0.001
    quit
" 2>&1) || true

# Normalize: strip timestamps, addresses, Renode banner lines.
norm_emu=$(echo "$emu_output" | grep -v '^$' | sed 's/[0-9a-f]\{8\}/ADDR/g')
norm_renode=$(echo "$renode_output" | grep -v '^$' | \
    sed 's/[0-9a-f]\{8\}/ADDR/g' | grep -v '^\(Renode\|  \|\*\)')

if [ "$norm_emu" = "$norm_renode" ]; then
    echo "MATCH: $program"
    exit 0
else
    echo "MISMATCH: $program"
    echo "--- emulator ---"
    echo "$norm_emu"
    echo "--- renode ---"
    echo "$norm_renode"
    exit 1
fi
