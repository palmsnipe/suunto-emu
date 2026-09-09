# 746 — Sapporo 2.39 Second Preload Integration

**Status:** done
**Phase:** 7
**Dependencies:** 728,729,615

## Goal

Advance the exact command-one WbStoPreload failure with a separate, one-shot
synthetic-state intervention, only after revalidating the installed cache.

## Execution Budget

One model-day for direct tracing, integration, and bounded verification.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; tickets 728 and 744;
E-SAP-COMPAT-WBSTO-239-001 and E-SAP-COMPAT-PRELOAD1-239-001;
`src/compat/sapporo_239.c`, `src/compat/sapporo_239.h`,
`src/compat/layer.c`, `include/semu/compat.h`, `include/semu/machine.h`,
`src/boards/machine_snapshot.c`, `tests/unit/test_sapporo_239_compat.c`,
`tests/unit/test_machine_snapshot.c`,
`tests/integration/test_firmware_sapporo_239_file_seek.sh`.

## Current Baseline

Ticket 744 reaches command-one result 500 after four missing record reads.
The same four synthetic cache values remain intact; native persisted bytes
are unavailable in the OTA fragment. No normal frame yet.

## Allowed Files

- `src/compat/sapporo_239.c`, `src/compat/sapporo_239.h`
- `src/boards/machine_snapshot.c`
- `tests/unit/test_sapporo_239_compat.c`
- `tests/unit/test_sapporo_239_preload1.c`
- `tests/unit/test_sapporo_239_preload1_snapshot.c`
- `tests/integration/test_firmware_sapporo_239_preload1.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/746-sapporo-239-second-preload.md`

## Frozen Interfaces

Existing intervention indices and individual budgets, component/profile pins,
cache payload/layout, logical files, CPU/device semantics and snapshot version
remain unchanged. Append one intervention; total budget becomes 2674.
Accept only the existing two- and three-counter legacy Sapporo 2.39 snapshots,
zero-initializing missing counters. Other layer identity/count rules stay strict.

## Evidence Inputs

E-SAP-COMPAT-PRELOAD1-239-001: command one rereads the exact four missing
settings and returns 500 while the installed synthetic cache remains intact.
The native reference explicitly handles both commands. Real persisted payloads
cannot be represented faithfully without the absent sectors, so this remains
an explicit synthetic-state fallback, not recovered device behavior.

## Implementation

At the existing exact callback/client/provider with command one and status
500, require one cache-install hit and one command-zero translation hit;
revalidate all context/tree/data bytes, then record the new one-hit intervention
before setting R3 to 200. Do not patch instructions or change any cache bytes.

## Tests and Commands

Run `make test TEST_FILTER=sapporo_239_preload1` before and after the fix.
Cover success/logging, repeat refusal, missing prerequisites, changed cache
context/tree/data, disabled layer, unrelated PC/client/provider/command/status,
and snapshot counter restore/legacy migration/unknown-count atomic refusal.
Run `make test TEST_FILTER=sapporo_239`,
`make test TEST_FILTER=machine_snapshot`, `make check-task-contracts`,
`make check-lines`, `make check`, and `make sanitize TEST_FILTER=sapporo_239`.
Run `SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_preload1`.

## Acceptance

The narrow regression fails before the change and passes after. Two exact
firmware runs have identical logs/snapshots at a later measured boundary;
one-step resume reproduces the next stop, and source flash remains immutable.
Retain historical goldens unchanged and report the next unsupported boundary.

## Forbidden Scope

No generic provider-success fallback, new settings/file payloads, missing-record
read overrides, native FAT changes, CPU/MMIO/device changes, or profile changes.

## Handoff

Implemented and verified; status remains in-progress for integrator review.
Changed files are exactly the Allowed Files above. Evidence used:
E-SAP-COMPAT-PRELOAD1-239-001, E-SAP-COMPAT-WBSTO-239-001, E-SAP-0011.

The pre-change narrow run selected three cases: success and prerequisite/cache
refusal failed, unrelated callbacks passed. Post-change all three pass, plus
the new snapshot case. `make test TEST_FILTER=sapporo_239` passes 16 cases;
`make test TEST_FILTER=machine_snapshot` passes one. `make check-task-contracts`
validates 118 tickets; `make check-lines` passes with existing review warnings;
`make check` and `make sanitize TEST_FILTER=sapporo_239` pass. The exact private
command above passes after matching the CLI's full refusal diagnostic in the
new runner (the first runner attempt omitted its trailing detail field).

Two fresh exact runs produce identical logs and snapshots at `stop=budget
pc=0x000921dc instructions=435333559 virtual_time_ns=1974290101`.
Log SHA-256 is `476cf8603492ff3cfc456a61babd1c0cc7c5347b4bc81a7e8a39594e595f9b71`;
snapshot SHA-256 is `27b6e51ee66569d9e68ef56c7608c280cfd4e48f6cfe5d4beb1aaaf68f4fa79f`.
There is one new preload hit at 1,955,180,209 ns and 2,671 logical-file hits;
no reset or device refusal occurs before the checkpoint. Source flash remains
immutable. The next attempted instruction returns `stop=compat-refused` with
unchanged PC, instruction count and virtual time, reporting logical-file budget
exhaustion. The pending read is in `sleepln/sleep.bin` at cursor 11,480 of 17,888.

A real three-counter snapshot at instruction 405,860,000 (SHA-256
`2414e607e40dc665d6a0615d3d38c92580e1499365f51e6b0bfbf5af40f02116`)
was additionally resumed with the same profile/layer/flash and limits
435,333,559 instructions / 30,000,000,000 ns; `cmp` confirms its output snapshot
is byte-identical to the fresh checkpoint. Synthetic tests cover two-counter
legacy state with no logical-file trailer, three-counter migration, current
counter restoration and repeat refusal, and atomic rejection of counts 0/1/5.

No normal frame is claimed. Next: trace repeated sleep-record scans to determine
whether the logical-file bound needs a separately justified finite extension or
a native ABI defect remains. This ticket leaves that limit and all historical
goldens unchanged. No additional interface change is requested; integrator
review is required before marking done. Firmware/diagnostic artifacts stay
outside Git.
