# 751 — Sapporo 2.39 Native Quiet Read Routing

**Status:** done
**Phase:** 7
**Dependencies:** 729

## Goal

Replace the ZIP-specific native mode-nine exception with evidence-backed
quiet-read routing for paths outside the synthetic writable-file table.

## Execution Budget

One model-day for routing regression, UI resource tracing and exact runs.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; E-SAP-COMPAT-ZIP-READ-239-001,
E-SAP-COMPAT-QUIET-READ-239-001 and E-SAP-0011;
`src/compat/sapporo_239_file_hook.c`, `src/compat/sapporo_239_files.h`,
`src/compat/sapporo_239_files_internal.h`, `src/compat/sapporo_239.h`;
`semu_s239_file_index` in `src/compat/sapporo_239_files.c`;
`tests/unit/test_sapporo_239_zip_read.c`.

## Current Baseline

Ticket 749's native ZIP open succeeds, but the adapter next rejects the same
read mode for `ui/js/config.js` at instruction 459,796,107. Pristine mode
parsing is filename-independent: nine means `r` plus suppressed failure log.

## Allowed Files

- `src/compat/sapporo_239_file_hook.c`
- `tests/unit/test_sapporo_239_zip_read.c`
- `tests/unit/test_sapporo_239_quiet_read.c`
- `tests/integration/test_firmware_sapporo_239_quiet_read.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/751-sapporo-239-native-quiet-read.md`

## Frozen Interfaces

No headers, file table/capacity, snapshot format, opaque handle, mode stored
in a synthetic handle, descriptor, hit budget, profile or API changes. All
twelve table-owned paths retain mode-nine refusal whether present or absent.
Mode 1/2/3 behavior is unchanged. No synthetic operation is added or counted.

## Evidence Inputs

E-SAP-COMPAT-QUIET-READ-239-001 identifies the filename-independent native
read ABI. This routing rule does not grant access to host files or fabricate
missing guest files: the CPU executes native volume/path/content checks.

## Implementation

Validate the existing path syntax, look it up in the existing table, and
return not-handled only for mode nine with no table ownership. Preserve CPU,
RAM, files, counters and log stream. Other unsupported modes remain refused.
The former ZIP-only refusal expectations for non-table native reads are
superseded explicitly; retain malformed syntax and synthetic-path refusals.

## Tests and Commands

Run `make test TEST_FILTER=sapporo_239_quiet_read` before/after; cover native
UI/arbitrary validated paths, all twelve protected synthetic paths absent and
present, preserved synthetic contents/handles, and unchanged state on refusal.
Run `make test TEST_FILTER=sapporo_239`,
`make test TEST_FILTER=machine_snapshot`, `make check-task-contracts`,
`make check-lines`, `make check`, `make sanitize`.
Run `SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_quiet_read`.

## Acceptance

Failing-before/passing-after regression, identical two fresh authentic
logs/snapshots, exact old ticket-749 prefix and identical resumed state.
Trace the native UI open result and identify a later explicit boundary.
Private inputs must validate and remain immutable. Report incomplete gates.

## Forbidden Scope

No synthetic mode-nine handle, content/file substitution, path alias repair,
mode masking, unknown-mode fallthrough, budget increase, provider/CPU/MMIO/
device change, public header, Makefile, profile, or historical golden edits.

## Handoff

Implemented and verified; status remains in-progress for integrator review.
Changed files are exactly the Allowed Files above. References used:
E-SAP-COMPAT-QUIET-READ-239-001, E-SAP-COMPAT-ZIP-READ-239-001 and E-SAP-0011.
Only the native mode-nine routing predicate and lookup order change production
code. Headers, synthetic files, modes 1/2/3, snapshots, descriptors and all
budgets are untouched. Native execution owns path/content/failure semantics;
`SEMU_OK` at an unchanged hook PC means not-handled, not successful file open.

The narrow regression fails before the change at native UI/arbitrary paths
and passes afterward. All twelve synthetic paths refuse mode nine both absent
and present; complete serialized state is unchanged. Their normal mode-one
reopens read the original synthetic contents. The retained ZIP regression
checks no mutation or log emission even at exhausted budget, and preserves
wrong mode/flag, malformed syntax, range and disabled-layer refusals. Four
non-table mode-nine expectations are explicitly superseded by the native ABI;
no synthetic file-success expectation replaces them.

All exact Tests and Commands above pass: Sapporo 2.39 tests 21 cases, machine
snapshot one case, task contracts 122 indexed tickets, check-lines (existing
warnings only), full `make check` and `make sanitize` (128 binaries, 735 cases
each), and the exact private runner. `git diff --check` and
`sh -n tests/integration/test_firmware_sapporo_239_quiet_read.sh` pass.

Bounded read-only tracing proves the native configuration open returns handle
`0x40`; the first segment records 134 opens over 105 paths by 600 million
instructions. The full pre-fault trace uses a 610-million-instruction and
30-billion-ns ceiling, no guest-state edits and no diagnostic budget increase.
Trace hashes and call/return addresses are recorded in the evidence entry.

Two fresh authentic logs/snapshots match at
`stop=budget pc=0x000cb852 instructions=607105617 virtual_time_ns=2146062159`.
Log SHA-256 `740750cbc6460fe8b9c3b420a5509d992dc0757e50de9102da316df7d21be1ec`;
snapshot `15b5f2076d7107e50b693333d1d19bcd3bd18014b8208c33f02ed8e45676dd19`.
The old ticket-749 prefix retains original log hash
`ea04ac89a277fc58cc1c653e59e595f2a40f25d7202afd16b5adc309e6bf2732` and snapshot
`13e104c98a6fdf5a741a15615bf1b77ea53ebe05db39e7bf224cf0818560b5ee`.
Resuming it reaches the identical new snapshot. All 76,258 logical-file hits
are unchanged; source flash is immutable and no reset/device refusal occurs
before this boundary. The next step reaches PC `0x001c0db4`, instruction
607,105,618, time 2,146,062,160 ns, proving the strict device fault remains.

Remaining gap: CTIMER INTEN write `0x08004001` to `0x40008060`, from
`STR r4,[r0]` at `0x000cb852`; this task does not authorize its added bit.
Native fault handling later resets. Recover that interrupt contract next;
no normal 2.39 frame is claimed. No additional integrator-owned interface
change is requested. Historical private goldens are untouched, and private
firmware, snapshots and diagnostics remain outside Git.
