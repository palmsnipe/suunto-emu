#!/bin/sh
# TEST_TAGS: sapporo_239_general_budget
set -eu
if [ "${TEST_PROFILE-}" != sapporo-2.39.20 ]; then
    echo "SKIP Sapporo 2.39 general budget: different profile"
    exit 0
fi
emulator=${SEMU_EMULATOR-}
manifest=${SEMU_FIRMWARE_MANIFEST-}
full_flash=${SEMU_SAPPORO_239_FULL_FLASH-}
if [ -z "$full_flash" ] || [ -z "$manifest" ]; then
    echo "SKIP Sapporo 2.39 general budget: set manifest and SEMU_SAPPORO_239_FULL_FLASH"
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
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-sapporo-239-general.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT HUP INT TERM
${CC:-cc} -std=c99 -Wall -Wextra -Werror -pedantic -O2 \
    -Iinclude -Isrc -Isrc/devices tests/integration/sapporo_239_general_probe.c \
    "$library" -o "$run_dir/probe"
run()
{
    name=$1; start=$2
    if ! "$run_dir/probe" "$manifest" "$full_flash" "$start" "$run_dir/$name" \
        >"$run_dir/$name.out" 2>"$run_dir/$name.log"; then
        cat "$run_dir/$name.out" "$run_dir/$name.log" >&2
        echo "error: general-settings native gate failed: $name" >&2
        exit 1
    fi
}
# Generate the private prefix with the production library, not a diagnostic build.
run cold cold
test "$(hash "$run_dir/cold.prefix.sems")" = 7dddd41a13c51b0e4d4d63be09b8bfff1db1654439d290343f24959758761c7c
# RE-PINNED (777/E-SAP239-DEEPCLEAN-001): the storage-law corrections removed
# the old deep-boot wall, so the cold prefix is a clean boot. Its prefix image
# moved 7650d82f… -> 7dddd41a… (only the recycled-handle line drifted); the
# cold transcript hash is new. Census of the cold prefix (held, twice):
# 76315 session operations. The WHOLE general save completes here — open
# settings/general, 90 writes, the 1505/1505 close — and settings/personal
# traffic (408 lines, 6 closes at size 1727) completes here. No settings/time
# traffic, no refusals, no reset in the cold prefix.
test "$(hash "$run_dir/cold.log")" = de040f443d24cc41e35189c69522f65edcdebb247ae75b0419453e05d029436d
test "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/cold.log")" -eq 76315
test "$(grep -c 'operation=open path=settings/general ' "$run_dir/cold.log")" -eq 1
test "$(grep -c 'operation=write path=settings/general ' "$run_dir/cold.log")" -eq 90
grep -F -q 'operation=close path=settings/general result=1 size=1505 cursor=1505' "$run_dir/cold.log"
test "$(grep -c 'operation=close path=settings/general ' "$run_dir/cold.log")" -eq 1
test "$(grep -c 'path=settings/personal' "$run_dir/cold.log")" -eq 408
test "$(grep -c 'operation=close path=settings/personal result=1 size=1727 cursor=1727' "$run_dir/cold.log")" -eq 6
for attempt in first second; do
    run "$attempt" "$run_dir/cold.prefix.sems"
    # RE-SCOPED (777/E-SAP239-DEEPCLEAN-001): terminal golden, twice
    # byte-identical through this flow. The old golden numbers hold here
    # (frames 677, crc 405d1af6, sha 6eb15b72…, refusal detail) except the
    # instruction count: the old 2363623546 / 32619070564 belonged to the
    # retired input-edge virtual-time lineage; the clean boot's refused stop
    # is 2391136680 / 32620918072 at pc=0x1291cc.
    grep -F -x -q 'END reason=8 instructions=2391136680 time=32620918072 pc=001291cc frames=677 crc=405d1af6 sha=6eb15b72ea2d250b1106d6a89c39ac87eb3827ebd1ce7c367bf1efcfeb2b4742 detail=2.39 GPS awake lifecycle or hit budget refused' "$run_dir/$attempt.out"
    # Refused terminal snapshot == mid == refused. RE-PINNED
    # (710/E-EMU-SAP235-TICKTRAIL-002, 2026-09-30): the tsc6a per-resolve
    # frame lifecycle changed the serialized shadow inside the terminal
    # image (resting state at the stop instead of accumulated strokes);
    # the END golden, the transcripts and the census are unchanged and
    # only the image bytes moved bb17b7a8… -> 24d5a4dd… (twice
    # byte-identical; the resumed leg re-saves the same image).
    test "$(hash "$run_dir/$attempt.final.sems")" = 24d5a4dd8859155426adf98b89b1f3d6b8fdf53618069b8fa92bad208fcabeba
    test "$(hash "$run_dir/$attempt.refused.sems")" = 24d5a4dd8859155426adf98b89b1f3d6b8fdf53618069b8fa92bad208fcabeba
    cmp "$run_dir/$attempt.final.sems" "$run_dir/$attempt.refused.sems"
    cmp "$run_dir/$attempt.final.sems" "$run_dir/$attempt.mid.sems"
    # RE-PINNED (777/E-SAP239-DEEPCLEAN-001): resumed transcript — this
    # flow's own log, held twice (5× byte-identical in the 777/E-SAP239-
    # DEEPCLEAN-001 verification series): the old 8f00d1cd… belongs to the
    # retired lineage. Census (held): 92 session operations, ordinals
    # 76316-76407 continuing the cold prefix; 90 settings/general writes
    # with the 1505/1505 close (the guest retries the save here after the
    # cold prefix); four awake pulses; zero refusals in the stream.
    test "$(hash "$run_dir/$attempt.log")" = 0cc0b54d8547bb16bf2baff183c3118aa2053710cd9dc22127a704fe5035a082
    test "$(grep -c 'trigger=logical-file ordinal=' "$run_dir/$attempt.log")" -eq 92
    grep -F -q 'trigger=logical-file ordinal=76316 ' "$run_dir/$attempt.log"
    test "$(grep -c 'operation=open path=settings/general ' "$run_dir/$attempt.log")" -eq 1
    test "$(grep -c 'operation=write path=settings/general ' "$run_dir/$attempt.log")" -eq 90
    grep -F -q 'operation=close path=settings/general result=1 size=1505 cursor=1505' "$run_dir/$attempt.log"
    test "$(grep -c 'layer=sapporo-2.39-gps-awake trigger=gps-awake-pulse' "$run_dir/$attempt.log")" -eq 4
    # Zero refusals anywhere in the log stream, and no reset: the single
    # refusal of the whole flow is the machine stop pinned by the END line.
    test "$(grep -c 'refus' "$run_dir/$attempt.log")" -eq 0
    if grep -q 'event=machine-reset-request' "$run_dir/$attempt.log"; then exit 1; fi
done
cmp "$run_dir/first.out" "$run_dir/second.out"
cmp "$run_dir/first.final.sems" "$run_dir/second.final.sems"
# The refused terminal state is itself a resumable checkpoint: resuming it, a
# single step re-fires the refusal AT THE IDENTICAL terminal — instr
# 2391136680 / virtual time 32620918072 (the refusal is a fixed point, no
# advance), frames=1 (the snapshot load publishes one frame, the refusal
# publishes none) — and re-saves the identical terminal image.
run resumed "$run_dir/first.final.sems"
grep -F -x -q 'END reason=8 instructions=2391136680 time=32620918072 pc=001291cc frames=1 crc=405d1af6 sha=6eb15b72ea2d250b1106d6a89c39ac87eb3827ebd1ce7c367bf1efcfeb2b4742 detail=2.39 GPS awake lifecycle or hit budget refused' "$run_dir/resumed.out"
cmp "$run_dir/first.final.sems" "$run_dir/resumed.final.sems"
cmp "$run_dir/resumed.final.sems" "$run_dir/resumed.refused.sems"
# The refusal re-fires on its very first step, so it emits no event at all:
# its stream is empty (the empty-string sha256).
test "$(hash "$run_dir/resumed.log")" = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
test "$(grep -c 'refus' "$run_dir/resumed.log")" -eq 0
# The helper independently checks firmware and full flash before execution.
if "$run_dir/probe" "$manifest" "$manifest" cold "$run_dir/bad-flash" \
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
if "$run_dir/probe" "$run_dir/wrong.semu" "$full_flash" cold "$run_dir/bad-component" \
    >"$run_dir/bad-component.out" 2>"$run_dir/bad-component.log"; then exit 1; fi
grep -F -q 'component application does not match profile' "$run_dir/bad-component.log"
test ! -e "$run_dir/bad-component.prefix.sems"
# A valid snapshot image from a wrong checkpoint (the refusal state — a legal
# checkpoint of the sibling input-free lineage, not of this edge-driven flow)
# must fail closed on the start-checkpoint guard before executing or saving.
cp "$run_dir/first.final.sems" "$run_dir/bad-snap.sems"
# Corrupt the snapshot payload with one byte flipped, preserving the size:
# the hash pin fires before the image is executed or any output is saved.
python3 - "$run_dir/bad-snap.sems" <<'PY'
import sys
with open(sys.argv[1], "r+b") as stream:
    stream.seek(0, 2)
    size = stream.tell()
    stream.seek(size // 2)
    stream.write(bytes([stream.read(1)[0] ^ 0x01]))
PY
if "$run_dir/probe" "$manifest" "$full_flash" "$run_dir/bad-snap.sems" \
    "$run_dir/bad-snapshot" >"$run_dir/bad-snapshot.out" 2>"$run_dir/bad-snapshot.log"; then exit 1; fi
grep -F -q 'private input hash mismatch' "$run_dir/bad-snapshot.log"
test ! -e "$run_dir/bad-snapshot.final.sems"
# An untouched snapshot of the sibling input-free refusal fixed point
# (instr 1296811148) is a legal image with foreign counters: the checkpoint
# guard must reject it before executing or saving. It is supplied out of band
# (a private fixture, never generated by this flow).
if [ -n "${SEMU_SAPPORO_239_FOREIGN_SNAPSHOT:-}" ]; then
    cp "$SEMU_SAPPORO_239_FOREIGN_SNAPSHOT" "$run_dir/foreign.sems"
    if "$run_dir/probe" "$manifest" "$full_flash" "$run_dir/foreign.sems" \
        "$run_dir/bad-checkpoint" >"$run_dir/bad-checkpoint.out" 2>"$run_dir/bad-checkpoint.log"; then exit 1; fi
    grep -F -q 'unexpected native start checkpoint' "$run_dir/bad-checkpoint.log"
    test ! -e "$run_dir/bad-checkpoint.final.sems"
fi
test "$(hash "$full_flash")" = "$flash_hash"
echo "PASS sapporo-2.39.20 general save: held resumed census (92 ops, 90 writes, 1505/1505 close), refusal fixed point at instr 2391136680 with identical terminal-state resume"
