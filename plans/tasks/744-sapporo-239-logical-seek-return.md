# 744 — Sapporo 2.39 Logical Seek Return Contract

**Status:** in-progress
**Phase:** 7
**Dependencies:** 729

## Goal

Correct the logical-file seek success return to match the exact native public
wrapper ABI, allowing TrainingTss to read its own newly created records.

## Execution Budget

One model-day for ABI tracing, a narrow regression, and deterministic runs.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`,
`docs/testing-strategy.md`, `docs/compatibility-policy.md`; ticket 743;
E-SAP-COMPAT-FILE-SIZE-239-001; `src/compat/sapporo_239_files.h`,
`src/compat/sapporo_239_files_internal.h`,
`src/compat/sapporo_239_file_hook.c`, `tests/unit/test_sapporo_239_files.c`,
`tests/unit/test_sapporo_239_file_size.c`.

## Current Baseline

Ticket 743 reaches TrainingTss command zero result 500. The logical adapter
moves the cursor to 32 but returns zero at the public seek wrapper. The guest
compares against the requested offset and aborts before reading a record.

## Allowed Files

- `src/compat/sapporo_239_file_hook.c`
- `tests/unit/test_sapporo_239_file_seek.c`
- `tests/integration/test_firmware_sapporo_239_file_seek.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/744-sapporo-239-logical-seek-return.md`

## Frozen Interfaces

Keep all public interfaces, PC registry, layer IDs/hash pins, hit budgets,
path/handle policy, signed cursor calculation and bounds, snapshot format,
file data, other operation returns, and native-handle fallthrough unchanged.

## Evidence Inputs

E-SAP-COMPAT-SEEK-239-001 records exact native wrapper `0x00092182`, helper
`0x000be0a4`, and the failing guest at `0x0016fac2`. The helper saves R1
(requested signed offset bit pattern) and returns that value on success,
including relative/end-relative seeks; it does not return the resulting
absolute cursor. The lower native seek's zero success code is not this ABI.

## Implementation

Only after existing validation succeeds, return the original R1 offset through
the existing hook return path. Keep cursor mutation and hit accounting exactly
as before. No provider-result translation or firmware instruction patch.

## Tests and Commands

Run `make test TEST_FILTER=sapporo_239_file_seek` before and after the fix.
Cover nonzero/zero absolute seeks, current/end relative positive/negative
offsets, raw negative return bits, capacity-edge and beyond-EOF success,
read-after-seek and snapshot cursor restore, and refusal atomicity for bad
origins, underflow/overflow/capacity, stale handles, disabled layer and budget.
Confirm native fallthrough and the exact seek log result/cursor distinction.
Run `make test TEST_FILTER=sapporo_239`,
`make test TEST_FILTER=machine_snapshot`, `make check-task-contracts`,
`make check-lines`, `make check`, and `make sanitize TEST_FILTER=sapporo_239`.
Run `SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_file_seek`.

## Acceptance

The narrow regression fails before the fix and passes after. Native TrainingTss
advances past the former seek failure; two authentic logs/snapshots match at a
later measured boundary, source flash stays immutable, and one-step snapshot
continuation reproduces the next boundary. Retain older golden values.

## Forbidden Scope

No callback/status bypass, file content fabrication, native FAT changes,
budget increase, new file operations/paths, CPU/device/MMIO/renderer changes,
profile or public-header edits, or snapshot-version change.

## Handoff

Implemented and verified; status remains in-progress for integrator review.
Changed files are exactly the Allowed Files above. Evidence is
E-SAP-COMPAT-SEEK-239-001 and E-SAP-COMPAT-FILE-SIZE-239-001; component pins
are E-SAP-0011, and the synthetic full-flash identity remains
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.

The pre-fix narrow test ran two cases: success/return/log checks failed while
atomic refusals passed. Both pass after the one-line correction. All exact
commands above pass: Sapporo 2.39 tests 12 cases, machine snapshot 1 case,
task contracts 117 tickets, check-lines (existing review warnings only), full
make check, and Sapporo 2.39 ASan/UBSan 12 cases. The private runner validates
all components and flash, checks the exact seek and all 42 record reads,
compares two complete logs/snapshots and one-instruction resume, and verifies
source-flash immutability.

Read-only tracing starts at instruction 405,860,000, snapshot SHA-256
`2414e607e40dc665d6a0615d3d38c92580e1499365f51e6b0bfbf5af40f02116`.
Pre-fix trace SHA-256 is
`20cff47d367c122168f5002382a87fe993918aa61e659c3ea4fb882b9c08a398`;
post-fix trace is
`974e54112810fc99d4601643e6050a419affc6a0700b69e026a1c557654689ed`.
The normal TrainingTss callback now receives R3=200 at instruction 405,884,883.
No callback/record was modified. Diagnostic artifacts remain outside Git.

The authentic checkpoint is `stop=budget pc=0x00079e1c
instructions=416256851 virtual_time_ns=1955213393`. Log SHA-256 is
`4f8e749ebe80774b968a091dda8086aabeb85229e6dd08b916cb615e24e6497e`;
snapshot SHA-256 is
`92e7f05339ff218402f0b788a6263dc8173c81fba30d56d4cf8c66c1bab09a16`.
There are 595 logical-file operations with unchanged limits and no reset or
device refusal. One-step resume is `stop=halt pc=0x00079e1e
instructions=416256852 virtual_time_ns=1955213394`.
The next failure is WbStoPreload command one, result 500: saved descriptor
`0x10025634` has provider pointer `0x001c0ed8` and command byte one;
LR `0x001248b9` and stack result 500 identify the normal StartupClient failure
path. Missing command-one state remains unresolved; no normal frame yet.
Older goldens are untouched. No interface change is requested; the integrator
must review acceptance before marking done.
