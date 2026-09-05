# 748 — Sapporo 2.39 Ongoing Activity File Integration

**Status:** in-progress
**Phase:** 7
**Dependencies:** 729,615

## Goal

Retain the native-created ongoing Activity Timeline file in the exact-build
logical adapter, with an evidenced capacity and backward-compatible snapshots.

## Execution Budget

One model-day for native layout recovery, regression and bounded exact runs.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; tickets 729 and 747;
E-SAP-0011, E-SAP-COMPAT-FILES-239-001, E-SAP-COMPAT-ONGOING-239-001;
`src/compat/sapporo_239.c`, `src/compat/sapporo_239.h`,
`src/compat/sapporo_239_files.c`, `src/compat/sapporo_239_files.h`,
`src/compat/sapporo_239_files_internal.h`, `src/compat/sapporo_239_file_hook.c`;
`tests/unit/test_sapporo_239_files.c`,
`tests/unit/test_sapporo_239_compat.c`,
`tests/unit/test_sapporo_239_history_budget.c`,
`tests/integration/test_firmware_sapporo_239_history_budget.sh`.

## Current Baseline

At instruction 439,081,594 the firmware enum-creates the unknown path
`actitmln/ongoing.bin`. Its store capacity is three records; native creation
writes a 24-byte header, eight padding bytes and three 40-byte records.

## Allowed Files

- `src/compat/sapporo_239.c`
- `src/compat/sapporo_239_files.c`
- `src/compat/sapporo_239_files_internal.h`
- `tests/unit/test_sapporo_239_compat.c`
- `tests/unit/test_sapporo_239_history_budget.c`
- `tests/unit/test_sapporo_239_ongoing.c`
- `tests/integration/test_firmware_sapporo_239_ongoing.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/748-sapporo-239-ongoing-file.md`

## Frozen Interfaces

Append only the new exact path with capacity 152. Preserve all eleven old
indices, capacities, wrapper ABIs, intervention indices and hash pins. The
internal file-count header/arrays are explicitly integration-owned here.
Keep S29F version one: emit count eleven until the new slot exists, otherwise
twelve; read only counts eleven/twelve atomically. Preserve old checkpoint
bytes. Only the logical-file budget may grow by the measured finite native
operations up to the next refusal: exactly 76,258 file hits, 76,261 aggregate.
Keep all historical private goldens intact.

## Evidence Inputs

E-SAP-COMPAT-ONGOING-239-001 pins native creation/validation disassembly and
the immutable ticket-747 snapshot's store capacity. No initial file contents
or activity-store state repair is authorized.

## Implementation

Append the path/capacity and implement count-aware snapshot I/O. Let firmware
write every byte. Measure the newly reachable finite operations in a bounded
diagnostic run and pin that exact ceiling, never diagnostic headroom.

## Tests and Commands

Run `make test TEST_FILTER=sapporo_239_ongoing` before and after the change;
cover create/write/reopen/read, exact capacity plus atomic excess refusal,
unknown path, legacy/new snapshots and malformed/truncated atomic refusal.
Run `make test TEST_FILTER=sapporo_239`,
`make test TEST_FILTER=machine_snapshot`, `make check-task-contracts`,
`make check-lines`, `make check`, `make sanitize TEST_FILTER=sapporo_239`.
Run `SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_ongoing`.

## Acceptance

Failing-before/passing-after regression, byte-identical two fresh native
runs/snapshots, unchanged ticket-747 prefix, identical snapshot continuation,
native 152-byte creation and a named later refusal. All components must
validate and source flash remain unchanged. Report any missing acceptance.

## Forbidden Scope

No wildcard, fabricated header/record, provider-status override, activity
cursor repair, unrelated budget increase, CPU/device/MMIO behavior, public
API, Makefile, profile, global snapshot version or historical golden edits.

## Handoff

Implemented; integrator owns completion status. Changed files are exactly the
Allowed Files above. Evidence used: E-SAP-COMPAT-ONGOING-239-001,
E-SAP-COMPAT-FILES-239-001 and E-SAP-0011. Only the exact path/capacity,
count-aware snapshot codec/internal table header and measured file budget
change production behavior. There is no initial record, new wrapper ABI,
provider translation or activity-state repair.

The two-case narrow regression fails before the implementation on unknown
enum-create and passes afterward. It covers byte-preserving create/reopen/
read, 152-byte limit and atomic excess refusal, missing native-read fallthrough,
unknown mode/path, legacy/new snapshot round trips, malformed counts,
presence, capacity, handle index/cursor and truncation with unchanged state.
The history-budget regression retains all 144 x 248 scans and exact last-hit/
first-refusal checks at the new measured limit. Its unknown-path test now
uses `actitmln/unknown.bin`; no permissive expectation replaces a refusal.

All listed focused commands pass: Sapporo 2.39 unit tests 19 cases, machine
snapshot one case, task contracts 120 indexed tickets, check-lines (existing
review warnings only), full `make check`, focused ASan/UBSan 19 cases, and
the exact private runner. `git diff --check` and
`sh -n tests/integration/test_firmware_sapporo_239_ongoing.sh` pass.
Full `make sanitize` also passes with no address/undefined-behavior findings.
`make check` is repeated after the final test/documentation edits and passes.

Two fresh authentic runs produce identical logs and snapshots at
`stop=budget pc=0x000920b4 instructions=451511675 virtual_time_ns=1990468217`.
Log SHA-256 `33f75a3051a8487405a4f5221d9db36a806fc54b2cf26fee8ebff5082bf62a7e`;
snapshot `42de6549afe8bef32603a4acd497f2aee0bb41a92a77f59022064c23e194998b`.
The runner verifies 76,258 file hits, twenty ongoing-file operations, five
native writes, successful 152-byte size validation and record reads, no reset
or device refusal, unchanged source flash and exact old ticket-747 prefix
hashes. Resuming that real eleven-slot snapshot reaches the identical new
twelve-slot state. Resuming the new snapshot for one attempted instruction
refuses mode nine before PC/time/count advance with
`detail=unknown Sapporo 2.39 file open mode`.

The remaining gap is `zapp/zwspee01.zip`, mode nine, LR `0x000843e9`.
Recover flags, native resource routing and failure semantics before changing
that refusal. No normal 2.39 frame or completed release milestone is claimed.
No further integrator-owned interface change is requested; review is needed
before marking the ticket done. Private bytes, snapshots and diagnostic
artifacts remain outside Git.
