#!/bin/sh
# TEST_TAGS: sapporo_239_personal_budget
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 personal budget: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
if [ -z "$full_flash" ] || [ -z "$manifest" ]; then
    echo "SKIP Sapporo 2.39 personal budget: set manifest and SEMU_SAPPORO_239_FULL_FLASH"
    exit 0
fi
hash() { shasum -a 256 "$1" | awk '{print $1}'; }
flash_hash=37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb
if [ ! -r "$full_flash" ] || [ "$(hash "$full_flash")" != "$flash_hash" ]; then
    echo "error: Sapporo 2.39 full flash unavailable or hash mismatch" >&2
    exit 2
fi
"$emulator" validate --profile sapporo-2.39.20 --firmware "$manifest"
library=$(dirname "$emulator")/libsemu.a
test -r "$library"
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-personal.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
${CC:-cc} -std=c99 -Wall -Wextra -Werror -pedantic -O2 \
    -Iinclude -Isrc -Isrc/devices tests/integration/sapporo_239_personal_probe.c \
    "$library" -o "$run_dir/probe"
run()
{
    name=$1; start=$2; branch=${3:-full}
    if ! "$run_dir/probe" "$manifest" "$full_flash" "$start" "$run_dir/$name" "$branch" \
        >"$run_dir/$name.out" 2>"$run_dir/$name.log"; then
        cat "$run_dir/$name.out" "$run_dir/$name.log" >&2
        echo "error: personal-settings native gate failed: $name" >&2
        exit 1
    fi
}
# Generate the cold prefix with the production library, not a diagnostic build.
# RE-PINNED (777/E-SAP239-DEEPCLEAN-001): the storage-law corrections removed
# the old deep-boot wall, so the cold prefix is a clean boot and its cap moved
# 1376525552 -> 700000000. The prefix image is the general-budget cold prefix
# (76a7af53… -> 7dddd41a…): the same four compat layers, the same cold run.
run cold cold
test "$(hash "$run_dir/cold.prefix.sems")" = 7dddd41a13c51b0e4d4d63be09b8bfff1db1654439d290343f24959758761c7c
# Cold transcript (held twice): 76315 session operations; the WHOLE personal
# save completes here — 6 closes at size 1727 over 408 settings/personal lines
# (the old 228-operation synthetic personal phase collapsed into this); no
# settings/general traffic (the general save lives in the general-budget
# flow), no settings/time traffic, no refusals, no reset.
test "$(hash "$run_dir/cold.log")" = de040f443d24cc41e35189c69522f65edcdebb247ae75b0419453e05d029436d
test "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/cold.log")" -eq 76315
test "$(grep -c 'path=settings/personal' "$run_dir/cold.log")" -eq 408
test "$(grep -c 'operation=close path=settings/personal result=1 size=1727 cursor=1727' "$run_dir/cold.log")" -eq 6
test "$(grep -c 'path=settings/general' "$run_dir/cold.log")" -eq 92
test "$(grep -c 'path=settings/time' "$run_dir/cold.log")" -eq 0
for attempt in first second; do
    run "$attempt" "$run_dir/cold.prefix.sems"
    # RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): terminal golden, twice
    # byte-identical. The old full terminal 3960123530 / 32455738919
    # (ac0a3289…) and idle terminal 3152721353 / 32538694863 (4faf5b89…) are
    # unreachable — the injected-input phases they depended on are retired
    # with the storage wall; the clean flow stops at the first awake refusal,
    # identical for both branches: instr 1296811148 / virtual time 32775096969
    # at pc=0x1291cc. The old mid 68ab2fa1…, time-mid b85eed95… and the
    # 279-operation / 132-write / 140-write census are retired with them.
    grep -F -x -q 'END reason=8 instructions=1296811148 time=32775096969 pc=001291cc frames=4 crc=3bd12ac8 sha=07944160817f67bb02efd0fdcebe6e0e38353d1950adb112f34a912d1a40396f detail=2.39 GPS awake lifecycle or hit budget refused' "$run_dir/$attempt.out"
    test "$(hash "$run_dir/$attempt.final.sems")" = cd0ca7121847e88afa4f8d89e51dd59d517a22aa013167fa1152bafb6f30ee35
    test "$(hash "$run_dir/$attempt.refused.sems")" = cd0ca7121847e88afa4f8d89e51dd59d517a22aa013167fa1152bafb6f30ee35
    cmp "$run_dir/$attempt.final.sems" "$run_dir/$attempt.refused.sems"
    cmp "$run_dir/$attempt.final.sems" "$run_dir/$attempt.mid.sems"
    # RE-PINNED (777/E-SAP239-DEEPCLEAN-001): resumed transcript (held twice):
    # the four awake-pulse interventions fire and are logged, then the fourth
    # pulse's dependency law refuses; the refusal is a machine stop, not a
    # logged event — zero refusals in the stream, zero session operations
    # (the personal save completed inside the cold prefix), zero writes of any
    # path, zero settings/time traffic, no reset.
    test "$(hash "$run_dir/$attempt.log")" = 8f00d1cdcdb4184a03d0de205a6952116e07006b4f33219b736e0f7df3ff5de4
    test "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/$attempt.log")" -eq 0
    test "$(grep -c 'operation=write path=settings/personal ' "$run_dir/$attempt.log")" -eq 0
    test "$(grep -c 'operation=write path=settings/general ' "$run_dir/$attempt.log")" -eq 0
    test "$(grep -c 'path=settings/time ' "$run_dir/$attempt.log")" -eq 0
    test "$(grep -c 'layer=sapporo-2.39-gps-awake trigger=gps-awake-pulse' "$run_dir/$attempt.log")" -eq 4
    test "$(grep -c 'refus' "$run_dir/$attempt.log")" -eq 0
    if grep -q 'event=machine-reset-request' "$run_dir/$attempt.log"; then exit 1; fi
done
cmp "$run_dir/first.out" "$run_dir/second.out"
cmp "$run_dir/first.final.sems" "$run_dir/second.final.sems"
# The refusal state is itself a resumable checkpoint: resuming it, a single
# step re-refuses at the identical stop without advancing instructions or
# virtual time (frames=1: the snapshot load publishes one frame, the refusal
# publishes none) and re-saves the identical image — a stable refusal.
run resumed "$run_dir/first.final.sems"
grep -F -x -q 'END reason=8 instructions=1296811148 time=32775096969 pc=001291cc frames=1 crc=3bd12ac8 sha=07944160817f67bb02efd0fdcebe6e0e38353d1950adb112f34a912d1a40396f detail=2.39 GPS awake lifecycle or hit budget refused' "$run_dir/resumed.out"
cmp "$run_dir/first.final.sems" "$run_dir/resumed.final.sems"
cmp "$run_dir/resumed.final.sems" "$run_dir/resumed.refused.sems"
test "$(hash "$run_dir/resumed.log")" = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
test "$(grep -c 'refus' "$run_dir/resumed.log")" -eq 0
# The idle branch is the same flow: same cold prefix, same refusal terminal,
# twice byte-identical (the divergence point — the personal-save injected-
# input phase — is unreachable now).
for attempt in idle-first idle-second; do
    run "$attempt" "$run_dir/cold.prefix.sems" idle
    grep -F -x -q 'END reason=8 instructions=1296811148 time=32775096969 pc=001291cc frames=4 crc=3bd12ac8 sha=07944160817f67bb02efd0fdcebe6e0e38353d1950adb112f34a912d1a40396f detail=2.39 GPS awake lifecycle or hit budget refused' "$run_dir/$attempt.out"
    cmp "$run_dir/$attempt.final.sems" "$run_dir/$attempt.refused.sems"
    cmp "$run_dir/first.final.sems" "$run_dir/$attempt.final.sems"
done
cmp "$run_dir/idle-first.out" "$run_dir/idle-second.out"
run idle-resumed "$run_dir/first.final.sems" idle
cmp "$run_dir/idle-first.final.sems" "$run_dir/idle-resumed.final.sems"
cmp "$run_dir/idle-resumed.final.sems" "$run_dir/idle-resumed.refused.sems"
test "$(hash "$run_dir/idle-resumed.log")" = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
# The helper independently checks firmware and full flash before execution.
if "$run_dir/probe" "$manifest" "$manifest" cold "$run_dir/bad-flash" full \
    >"$run_dir/bad-flash.out" 2>"$run_dir/bad-flash.log"; then exit 1; fi
grep -q 'private input hash mismatch' "$run_dir/bad-flash.log"
test ! -e "$run_dir/bad-flash.prefix.sems"
# Preserve component paths while corrupting only the application's hash pin.
manifest_dir=$(cd "$(dirname "$manifest")" && pwd)
ln -s "$manifest_dir" "$run_dir/components"
awk '
    /^\[component application\]$/ {application=1}
    /^\[component / && $0!="[component application]" {application=0}
    /^path=/ {$0="path=components/" substr($0,6)}
    application && /^sha256=/ {$0="sha256=0000000000000000000000000000000000000000000000000000000000000000"}
    {print}
' "$manifest" >"$run_dir/wrong.semu"
if "$run_dir/probe" "$run_dir/wrong.semu" "$full_flash" cold "$run_dir/bad-component" full \
    >"$run_dir/bad-component.out" 2>"$run_dir/bad-component.log"; then exit 1; fi
grep -F -q 'component application does not match profile' "$run_dir/bad-component.log"
test ! -e "$run_dir/bad-component.prefix.sems"
test "$(hash "$full_flash")" = "$flash_hash"
# A valid snapshot image with ONE byte of the payload corrupted (file size
# preserved, format intact) must refuse on its hash pin before executing or
# saving.
cp "$run_dir/resumed.refused.sems" "$run_dir/bad-snap.sems"
python3 - "$run_dir/bad-snap.sems" <<'PY'
import sys
with open(sys.argv[1], "r+b") as stream:
    stream.seek(0, 2)
    size = stream.tell()
    stream.seek(size // 2)
    stream.write(bytes([stream.read(1)[0] ^ 0x01]))
PY
if "$run_dir/probe" "$manifest" "$full_flash" "$run_dir/bad-snap.sems" \
    "$run_dir/bad-snapshot" full >"$run_dir/bad-snapshot.out" 2>"$run_dir/bad-snapshot.log"; then exit 1; fi
grep -F -q 'private input hash mismatch' "$run_dir/bad-snapshot.log"
test ! -e "$run_dir/bad-snapshot.final.sems"
echo "PASS sapporo-2.39.20 personal/time saves: clean-boot personal save in cold prefix (408 lines, 6x1727 closes), refusal fixed point at instr 1296811148, full/idle branches identical"
