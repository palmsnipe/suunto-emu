# 767 — Sapporo 2.39 Time Settings Persistence Evidence

**Status:** done
**Phase:** 7
**Dependencies:** 729,759,766

## Goal

Measure the native `settings/time` save at the weight-screen boundary,
including complete writes, native success checks, minimum observed capacity
and the next independent refusal. Propose a separately owned integration.

## Execution Budget

One model-day for bounded private observation and firmware analysis.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; tickets 764 and 766;
E-SAP-COMPAT-PERSONAL-239-001, E-SAP-TIME-SCHEMA-239-001,
E-SAP-COMPAT-FILES-239-001; `include/semu/{machine,compat,bus}.h`;
`src/compat/{sapporo_239.c,sapporo_239_files.h,sapporo_239_files_internal.h,
sapporo_239_files.c,sapporo_239_file_hook.c,layer.c}`;
`src/boards/machine_internal.h` (read-only observation),
`src/frontends/cli_snapshot.c`, `src/display/nema_backend.h`;
ticket 764's external personal probe/file observer and ticket 766's private
observer/gate. Read pristine wrapper `0x000acb78..0x000acba4`, serializer
`0x000d5084..0x000d51b8` and helpers before interpreting their contracts.

## Current Baseline

All dependencies are done. Production `bca8f7d` has 76,599 logical / 76,602
aggregate hits and refuses unknown mode-two `settings/time` at
`000920b4 / 3885178598 / 30368914377`, GPS `2,2,4`. Static analysis recovers
19 fields, not their complete dynamic write sequence or sufficient capacity.

## Allowed Files

- `docs/migration-evidence.md`, `docs/current-status.md`
- `plans/tasks/767-sapporo-239-time-settings-evidence.md`
- External temporary observational probes and isolated diagnostic sources;
  no modified runtime library, firmware, snapshots or pixels enter Git.

## Frozen Interfaces

Production runtime, public headers, Makefile, profiles, registry, budgets,
file paths/capacities, formats, renderer and goldens remain read-only.
Preserve the four explicit layers and four-pulse GPS bound. No live private
state mutation, counter replenishment or new parallel runtime API.

## Evidence Inputs

Ticket 766 final production snapshot SHA-256
`3d19f80df2c47a1e98f6bfa8dd0640f0d6cbb38a4d06f933d16f09c2c54245c6`,
personal midpoint `68ab2fa1fda8596e1b51a6b3d83950363effb0598f1835506492d20636fad7d8`,
exact E-SAP-0011 components and full flash
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.

## Implementation

Repeat production refusal and inspect native pending object without changing
it. To measure the otherwise refused path, an external explicitly diagnostic
build may append only `settings/time` to a copy of the existing file adapter,
with a 4,096-byte observation ceiling and at most 256 extra logical hits.
These are experimental bounds, not production capacity evidence. Preserve
all existing file slots, ABI and historical snapshot encodings; diagnostic
time state may append a thirteenth slot and must resume deterministically.
Do not replace native serialization or seed time-file bytes. Pin the complete
diagnostic delta and log every operation/return/size/cursor/payload hash.

Use at most five billion instructions, 35 billion virtual ns and 5,000 frames;
no further button input is necessary for the initial save. Repeat from the
production refusal and an earlier pinned personal midpoint with ticket 766's
remaining input edges. Require mid-time-save resume. An isolated native-path
control may forward only this open to the original firmware to test whether
lower-level storage already suffices; stop at its first fault/refusal.
Propose only measured bytes/operations, not the observation ceilings.

## Tests and Commands

Record exact compile/run commands and hashes for production/control/diagnostic
probes. Validate all component bytes and full flash before any execution.
Compare repeated final snapshots/logs and the mid-save resumed suffix.
Run `make test TEST_FILTER=sapporo_239`,
`make test TEST_FILTER=machine_snapshot`, `make check-lines`,
`make check-task-contracts`, `make check` and `git diff --check`.
Identity mismatch fails; absent private inputs may skip but cannot complete
acceptance. Production C is unchanged; diagnostic runs are not release goldens.

## Acceptance

Complete native save with full return lengths, serializer success, close and
pending clear; repeated and resumed agreement; exact next refusal; justified
minimum observed capacity and finite operation count. Record the smallest
integration scope including snapshot compatibility requirements. A static
schema or boundary trace alone does not complete acceptance.

## Forbidden Scope

No production time-file slot or ceiling change, fabricated clock/GPS value,
extra awake pulse, assertion bypass, firmware edit, opaque machine repair,
renderer change, new dependency, index/status update or weakened golden.

## Handoff

Planning baseline only. Production remains at the unknown-time-file boundary;
setup completion, watch face and post-setup menus are not yet demonstrated.

### Evidence handoff, 2026-09-08

All local acceptance observations pass; status remains ready for independent
review. E-SAP-TIME-NATIVE-239-001 records the complete sequence and the decisive
native-storage control. Changed repository files are this ticket and
`docs/{current-status,migration-evidence}.md`. The separate preceding planning
review accepted 766 and instantiated this index row. Production code is still
commit `bca8f7d`; no production implementation or new commit is made here.

The diagnostic measures two open/22-write/close saves: 24 operations and 349
bytes each, all full-length, serializer success and pending clear. Its bounded
4,096-byte/256-extra-hit setup is only an experiment. The native-path control
passes the identical 44 payloads through original firmware writes and retains
the final exact bytes once at flash offset `0x00a91a00` (absent beforehand).
There is no reason to integrate the experimental file slot or extra hits.
Native storage dirty pages increase from 69 to 72, without modifying source
flash. A production snapshot restore confirms retained bytes and atomic GPS
refusal. The native close caller ignores its public R0; do not claim its
observed `0x10053b2c` is the adapter's synthetic return-one ABI.

Two production refusal repeats retain ticket 766's exact final snapshot.
Two native refusal-start runs, two earlier personal-midpoint runs and a
mid-native-save resume converge to:

```text
compat-refused pc=001291cc instructions=3960123530 time=32455738919
logical_hits=76599 gps_hits=2,2,4 crc=a8c9f3d3
snapshot=ac0a32899f57d7b84733f458d4bc2b246d005b2885554686d20e157b5674193d
pixels=3820703556359211f629aea5ef013dda45fba092229b8d61e5a10d684c42d585
```

Native midpoint: `3885613020 / 30369348799 / 000af89c`, SHA-256
`b85eed95839285b520bb560cd1fff13431b837c59b29b60f5d6a12d8b86b58f2`.
Diagnostic midpoint: `3885191917 / 30368927696 / 000af89e`, SHA-256
`d96e97d12badecb2bff5ed295923fa528fed0c80ef9a6b77cbc1979571387af5`.
Diagnostic repeats/resume agree at final SHA-256
`8c4461bd698930930aea0620a0a4985cfa5b1da8b01d5220045da281826314e6`;
their log suffix matches from time 30,368,927,696 ns. Native SAVE/WRITE_RETURN/
END records match from the native midpoint. No private evidence was skipped.

### Reproduction commands and source identity

External sources and outputs are in `/tmp/semu-767-time.PaLEy5/`. Each input
snapshot is hash-checked before execution. Native/control source delta is only
the conditional exact-path native forwarding in a copied file hook; all other
file logic and the production descriptor remain unchanged. The diagnostic
build additionally links copied files/descriptor with the stated bounded
append-only slot and ceiling. `probe.c` includes the unchanged ticket-766
observer for frame/hash/snapshot helpers; the machine structure is inspected
read-only. Firmware serializers still execute in the in-tree CPU.

```sh
research_dir=/tmp/semu-767-time.PaLEy5
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/compat -Isrc/devices "$research_dir/probe.c" "$research_dir/file-observer.c" build/libsemu.a -o "$research_dir/production-probe"
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -DTIME_NATIVE_CONTROL -Iinclude -Isrc -Isrc/compat -Isrc/devices "$research_dir/probe.c" "$research_dir/file-observer.c" build/libsemu.a -o "$research_dir/native-probe"
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -DTIME_DIAGNOSTIC -Iinclude -Isrc -Isrc/compat -Isrc/devices "$research_dir/probe.c" "$research_dir/file-observer.c" "$research_dir/files.c" "$research_dir/descriptor.c" build/libsemu.a -o "$research_dir/probe"
start=/tmp/semu-766.FStvr2/first.final.sems
start_hash=3d19f80df2c47a1e98f6bfa8dd0640f0d6cbb38a4d06f933d16f09c2c54245c6
for attempt in a b; do
  "$research_dir/production-probe" "$research_dir/prod-$attempt" "$start" "$start_hash" > "$research_dir/prod-$attempt.trace" 2> "$research_dir/prod-$attempt.log"
  TIME_PAYLOAD_PATH="$research_dir/time-payload.bin" "$research_dir/probe" "$research_dir/diag-$attempt" "$start" "$start_hash" > "$research_dir/diag-$attempt.trace" 2> "$research_dir/diag-$attempt.log"
  TIME_EXPECTED_PAYLOAD="$research_dir/time-payload.bin" "$research_dir/native-probe" "$research_dir/native-full-$attempt" "$start" "$start_hash" > "$research_dir/native-full-$attempt.trace" 2> "$research_dir/native-full-$attempt.log"
  TIME_EXPECTED_PAYLOAD="$research_dir/time-payload.bin" "$research_dir/native-probe" "$research_dir/earlier-$attempt" /tmp/semu-766.FStvr2/first.mid.sems 68ab2fa1fda8596e1b51a6b3d83950363effb0598f1835506492d20636fad7d8 > "$research_dir/earlier-$attempt.trace" 2> "$research_dir/earlier-$attempt.log"
done
"$research_dir/probe" "$research_dir/diag-resume" "$research_dir/diag-a.mid.sems" d96e97d12badecb2bff5ed295923fa528fed0c80ef9a6b77cbc1979571387af5 > "$research_dir/diag-resume.trace" 2> "$research_dir/diag-resume.log"
TIME_EXPECTED_PAYLOAD="$research_dir/time-payload.bin" "$research_dir/native-probe" "$research_dir/native-resume" "$research_dir/native-full-a.mid.sems" b85eed95839285b520bb560cd1fff13431b837c59b29b60f5d6a12d8b86b58f2 > "$research_dir/native-resume.trace" 2> "$research_dir/native-resume.log"
TIME_EXPECTED_PAYLOAD="$research_dir/time-payload.bin" "$research_dir/production-probe" "$research_dir/production-restore" "$research_dir/native-full-a.final.sems" ac0a32899f57d7b84733f458d4bc2b246d005b2885554686d20e157b5674193d > "$research_dir/production-restore.trace" 2> "$research_dir/production-restore.log"
for pair in prod diag native-full earlier; do
  cmp "$research_dir/$pair-a.trace" "$research_dir/$pair-b.trace"
  cmp "$research_dir/$pair-a.log" "$research_dir/$pair-b.log"
  cmp "$research_dir/$pair-a.final.sems" "$research_dir/$pair-b.final.sems"
done
cmp "$research_dir/diag-a.final.sems" "$research_dir/diag-resume.final.sems"
for continuation in native-resume earlier-a production-restore; do
  cmp "$research_dir/native-full-a.final.sems" "$research_dir/$continuation.final.sems"
done
cc -std=c99 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -DTIME_NATIVE_CONTROL -Iinclude -Isrc -Isrc/compat -Isrc/devices "$research_dir/probe.c" "$research_dir/file-observer.c" build/sanitize/libsemu.a -o "$research_dir/native-sanitize"
TIME_EXPECTED_PAYLOAD="$research_dir/time-payload.bin" "$research_dir/native-sanitize" "$research_dir/sanitize" "$start" "$start_hash" > "$research_dir/sanitize.trace" 2> "$research_dir/sanitize.log"
cmp "$research_dir/native-full-a.final.sems" "$research_dir/sanitize.final.sems"
```

All runs/comparisons pass. A wrong all-zero expected snapshot hash returns two
with `private input hash mismatch` before creating a loaded snapshot. The
native-control ASan/UBSan run has no findings and matches normal final state.
The 44-write audit confirms 698 total bytes and exact payload-hash equality
between native and diagnostic paths. No payload bytes are checked into Git.

| Source/input | SHA-256 |
| --- | --- |
| `probe.c` | `611fd0c04d823f3a76018b5cd8fa789deb96e2264cc8e4da089904eddb55acbb` |
| `file-observer.c` | `2847569ed3c7f9bf7ca826d30f93a4c93200f143f40aff06428f4179d41f702a` |
| `file-hook.c` | `a1c43880507d02b333498774a4522a8ebf9ba377bea6ce5cc5f00585a9ebf2df` |
| `files.c` | `f333eb11b92a548fb15e185ace8ff7ab65ca6aa088e8d3c8c55b19532b880172` |
| `sapporo_239_files_internal.h` | `8413f0aec621d48fcb5bfaff156a27eca5f383afae556618a546dddb27eaad98` |
| `descriptor.c` | `2212f558b9aa11e1d42272f73b3548eb015f2575c734a32d3583e91175011949` |
| Included ticket-766 observer | `9034b690340401a8d749a0ffcaa19a50a71c8e5b5ec48d0c24e40fee29b4559c` |
| Production library | `7a0cb81857760b224d4511ea2e1c49392c2591ee1df34166110a15ce1f4d3e05` |
| Native probe executable | `6054c43e78f19ceaa8e1301c0e2fd986c7b19118aec0e6d5f74b614e6f13ac99` |
| Diagnostic executable | `15898edc71acb0a4c50dcbd0a19c0a141f5cc5f417296aa3cfcde72e145fad59` |

`make test TEST_FILTER=sapporo_239`: 46 passed;
`make test TEST_FILTER=machine_snapshot`: four passed; `make check`: all
831 passed; `make check-task-contracts`: 135 tickets validated;
`make check-lines` and `git diff --check`: pass (existing line warnings only).

### Smallest requested integration

Create a separate ticket for exact mode-two `settings/time` native forwarding
inside `src/compat/sapporo_239_file_hook.c`, with a focused routing regression
and private native-storage repeat/resume gate. Keep unknown other creates,
invalid modes and disabled/hash-mismatched layers fail-closed. Require unchanged
PC/registers/RAM/log/counters on routing itself, followed by native execution;
no synthetic result or extra compatibility hit is appropriate. No public
header, new file slot/capacity, budget, profile or snapshot-format change is
requested. Preserve all historical goldens and test production native snapshot
restoration/flash bytes, not the experimental thirteenth-slot snapshot.

The four-pulse GPS bound is the next independent refusal. Native time-file
support does not imply a GPS fix, invented clock, setup completion, a new
onboarding screen, watch-face activation or menu navigation. Production still
refuses the time path until that separately reviewed routing change is made.

### Integrator acceptance, 2026-09-08

Separate planning review accepts the complete native/control write sequence,
retained flash bytes, repeat/resume/sanitizer agreement and unchanged production
refusal recorded above. All acceptance evidence was available; native storage
obviates the diagnostic slot and extra allowance. Mark 767 done and instantiate
768 for exact-path routing and production regression integration only.
