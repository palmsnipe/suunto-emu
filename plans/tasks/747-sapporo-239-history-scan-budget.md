# 747 — Sapporo 2.39 Native History Scan Budget

**Status:** in-progress
**Phase:** 7
**Dependencies:** 729

## Goal

Integrate an evidence-bounded logical-file operation ceiling for the native
history scans now reached after correcting the file-size and seek ABIs.

## Execution Budget

One model-day for native loop recovery, a narrow regression and exact runs.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; tickets 729 and 746;
E-SAP-COMPAT-FILES-239-001, E-SAP-COMPAT-HISTORY-239-001;
`src/compat/sapporo_239.c`, `src/compat/sapporo_239.h`,
`src/compat/sapporo_239_file_hook.c`, `src/compat/sapporo_239_files.h`,
`src/compat/sapporo_239_files_internal.h`, `src/compat/layer.c`,
`tests/unit/test_sapporo_239_compat.c`, `tests/unit/test_sapporo_239_files.c`,
`tests/unit/test_sapporo_239_file_size.c`,
`tests/unit/test_sapporo_239_file_seek.c`,
`tests/integration/test_firmware_sapporo_239_preload1.sh`.

## Current Baseline

Ticket 746 refuses logical read 2,672 midway through the first sleep-history
window. Read-only disassembly and bounded diagnostic execution identify finite
42/60/42-day windows over 248 records, not a retry loop or seek failure.

## Allowed Files

- `src/compat/sapporo_239.c`
- `tests/unit/test_sapporo_239_compat.c`
- `tests/unit/test_sapporo_239_files.c`
- `tests/unit/test_sapporo_239_file_size.c`
- `tests/unit/test_sapporo_239_file_seek.c`
- `tests/unit/test_sapporo_239_history_budget.c`
- `tests/integration/test_firmware_sapporo_239_history_budget.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/747-sapporo-239-history-scan-budget.md`

## Frozen Interfaces

All headers, paths/capacities, handles, modes, operation ABIs, state formats,
intervention indices/log fields, profile/hash pins, and three other one-hit
budgets remain unchanged. Only the logical-file ceiling changes from 2,671
to exactly 75,764; total layer capacity becomes 75,767. Keep historical
private runners/goldens unchanged; prove the old pre-refusal checkpoint is
byte-identical even though its subsequent refusal is superseded.

## Evidence Inputs

E-SAP-COMPAT-HISTORY-239-001 documents the actual query loops and counts.
The old ceiling came from the reference's pre-correction short scans. A bounded
diagnostic ceiling of 200,000 exposes 75,764 valid operations before an unknown
write path. The production ceiling must equal that measured count, with no
extra headroom, new path, payload, or status translation.

## Implementation

Change the existing descriptor's logical-file/aggregate ceilings only, and
annotate the new budget evidence. Existing negative tests should exhaust the
declared limit instead of embedding the obsolete value; the new regression
pins the exact ceiling and tests its last success and first atomic refusal.

## Tests and Commands

Run `make test TEST_FILTER=sapporo_239_history_budget` before and after the
change. Exercise 144 full 248-record seek/read scans with synthetic file data,
the old boundary, exact new final operation, unchanged guest/file state on
budget refusal, and unknown writable-path refusal below the limit.
Run `make test TEST_FILTER=sapporo_239`,
`make test TEST_FILTER=machine_snapshot`, `make check-task-contracts`,
`make check-lines`, `make check`, `make sanitize TEST_FILTER=sapporo_239`.
Run `SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_history_budget`.

## Acceptance

The narrow regression fails before the correction and passes after. Two fresh
authentic logs/snapshots match at the later boundary. The old pre-refusal
checkpoint keeps its exact hashes and resumes to the same new snapshot.
One-instruction continuation refuses the new path before mutation. All private
components validate and source flash stays immutable. Report unsupported state.

## Forbidden Scope

No wildcard paths, new file or header bytes, generic budget relaxation,
provider success override, activity-store repair, CPU/device/MMIO changes,
public header/Makefile/profile edits, or snapshot format changes.

## Handoff

Implemented and verified; status remains in-progress for integrator review.
Changed files are exactly the Allowed Files above. Evidence used is
E-SAP-COMPAT-HISTORY-239-001, E-SAP-COMPAT-FILES-239-001 and E-SAP-0011.
Only two production constants and their evidence comment changed; no operation
or native record was modified. Existing budget refusals now exhaust the
declared ceiling and retain their original negative expectations.

The new one-case regression fails before the change at read operation 2,672;
afterward all 144 x 248 synthetic record scans complete, the 75,764th operation
succeeds and 75,765 refuses without register, destination, file/cursor or
counter mutation. Unknown ongoing-file creation still refuses below the limit.
All exact commands above pass: Sapporo 2.39 tests 17 cases, machine snapshot
one case, task contracts 119 indexed tickets, check-lines (existing review
warnings only), full make check, and ASan/UBSan 17 cases. `git diff --check`
and `sh -n tests/integration/test_firmware_sapporo_239_history_budget.sh` pass.

The exact private runner validates all three components and the full-flash
hash, compares two fresh full logs/snapshots, and checks the 35,712 record
reads, 35,714 total sleep seeks (two earlier header rewinds), 75,764 file hits,
absence of reset/device refusal, and unchanged source flash. Initial runner
development corrected the two-header-seek count and avoided snapshot-save on
the final refusal so the CLI retains its error detail; no historical test or
golden was edited.

New checkpoint: `stop=budget pc=0x000920b4 instructions=439081594
virtual_time_ns=1978038136`. Log SHA-256
`6f47fad1eeb3b6032955b463e2c4ba26310dbf5ddc453ae3f0f350acf15a9348`;
snapshot SHA-256
`3c56bfb5f3f7b541433ca05a3de999c941df3151484a5e080ad09a89b3672ae1`.
The next attempted instruction yields `stop=compat-refused` at identical
PC/time/count, with `detail=unknown Sapporo 2.39 writable file path`.
Read-only tracing identifies `actitmln/ongoing.bin`, mode two, R0
`0x1003e0b0`, LR `0x000b944d`. No filesystem/path/size fallback is added.

The fresh ticket-746 prefix at instruction 435,333,559 retains log SHA-256
`476cf8603492ff3cfc456a61babd1c0cc7c5347b4bc81a7e8a39594e595f9b71`
and snapshot `27b6e51ee66569d9e68ef56c7608c280cfd4e48f6cfe5d4beb1aaaf68f4fa79f`.
Resuming that snapshot produces the exact new full snapshot. Its historical
next-step budget refusal is superseded only by this explicitly evidenced
budget integration. Neither snapshot format nor prior checkpoint bytes change.

No normal frame is claimed. Next work must recover the ongoing Activity
Timeline file's bounded schema/capacity and lifecycle before adding that path.
No additional interface change is requested. Integrator review is required
before marking done; firmware, private snapshots and diagnostic files remain
outside Git.
