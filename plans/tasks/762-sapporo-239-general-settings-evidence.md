# 762 — Sapporo 2.39 General Settings Persistence Evidence

**Status:** done
**Phase:** 7
**Dependencies:** 729,759,761

## Goal

Recover the finite native settings-file sequence following the second MIDDLE
click in Sapporo 2.39. Distinguish a measured compatibility-budget requirement
from an ABI defect; propose the smallest separate integration change.

## Execution Budget

One model-day of bounded native tracing and read-only firmware analysis.
No production behavior change in this evidence ticket.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; tickets 729, 754, 759 and 761 handoffs;
E-SAP-0011, E-SAP-COMPAT-FILES-239-001, E-SAP-COMPAT-ACTIVITY-239-001,
E-SAP-UI-239-001 and E-SAP-UI-239-002; all Allowed Files;
`include/semu/compat.h`, `include/semu/machine.h`, `include/semu/bus.h`,
`src/compat/layer.c`, `src/compat/sapporo_239.c`,
`src/compat/sapporo_239_files.h`, `src/compat/sapporo_239_files.c`,
`src/compat/sapporo_239_files_internal.h`, `src/compat/sapporo_239_file_hook.c`,
`tests/unit/test_sapporo_239_activity_budget.c`,
`tests/unit/test_sapporo_239_history_budget.c`,
`tests/integration/test_firmware_sapporo_239_activity_budget.sh`;
the hash-pinned external observer sources in E-SAP-UI-239-002 and pristine
application caller `0x000ad064..0x000ad092`, serializer `0x000d5794` and
its reachable file-writing helpers. Read discovered callees before interpreting
their ABI; do not infer writes from a function name or a static call count.

## Current Baseline

All dependencies are done. Commit `2d230a2` repeats the two-click run with
148 accepted renderer submissions, no renderer refusals, and the exact
production logical-file budget refusal documented in E-SAP-UI-239-002.
The next open is `settings/general`, mode two, LR `0x000ad079`.
Production limits remain 76,279 logical-file hits and 76,282 aggregate hits.
No complete post-open operation sequence or replacement budget is established.

## Allowed Files

- `docs/migration-evidence.md`, `docs/current-status.md`
- `plans/tasks/762-sapporo-239-general-settings-evidence.md`
- External temporary observational probes and diagnostic artifacts only;
  no firmware bytes, frame pixels, snapshots or modified libraries enter Git.

## Frozen Interfaces

Production code, headers, Makefile, registries, profiles, persistent formats,
compatibility descriptors and all historical goldens are read-only. Preserve
the normal renderer, all four explicit layers and the four-pulse GPS ceiling.
Do not mutate private opaque state or create a parallel compatibility API.
If the existing interfaces cannot support a justified diagnostic experiment,
report the smallest integration change needed before running it.

## Evidence Inputs

E-SAP-UI-239-002 supplies exact component/probe/library hashes, native input
edges, the reproducible refusal, and the first static caller contract. The
older E-SAP-UI-239-001 trace swallowed renderer failures and is not a renderer
golden. Ticket 754's 21-operation activity suffix is historical, not evidence
for additional general-settings writes.

## Implementation

Reproduce the unchanged production refusal first. Follow pristine native
serialization code, including helper return checks and failure branches.
Record each observed operation's PC/LR, path/handle, mode, offset, size,
return value and ordinal; retain only hashes/metadata of proprietary bytes.
Any isolated diagnostic extension must be explicitly labelled, hash-pinned,
bounded in operations/instructions/virtual time, and separate from production
and its goldens. Do not replenish counters, bypass assertions, fabricate
provider events, or directly invoke native UI/serializer callbacks.

Require two equivalent runs and a mid-sequence snapshot continuation before
claiming the finite sequence. Identify the first independently unsupported
operation after the completed save. If evidence is unavailable, record exactly
what is missing; do not invent a production budget or file ABI.

## Tests and Commands

Run the two unchanged-production observer commands and final-state inspection
from E-SAP-UI-239-002; compare trace and every snapshot hash. Run
`make test TEST_FILTER=sapporo_239`,
`make test TEST_FILTER=machine_snapshot`, `make check-task-contracts`,
`make check-lines` and `make check`. Record exact commands and finite limits
for any subsequent diagnostic experiment. Missing private inputs may skip;
component/full-flash mismatches must fail before guest execution.

## Acceptance

Provide a reproducible, hash-pinned native operation sequence through save
completion, matching repeat/resume checkpoints, actual success checks, and
the next refusal. Separate observation from static inference. State whether
an exact finite budget suffices or an ABI change needs independent evidence.
Propose a separately owned integration ticket with precise allowed files and
success/excess/unknown-operation refusal regressions. A boundary-only trace
or unexecuted serializer analysis does not complete this ticket.

## Forbidden Scope

No production limit increase, broad fallback, new path/mode, guessed settings,
GPS fix/time/GSTP, IRQ heartbeat, assertion bypass, firmware edits, renderer
change, new runtime dependency, index/status update or weakened golden.

## Handoff

Planning baseline only: E-SAP-UI-239-002 revalidates the production boundary
and identifies the native save wrapper. Complete serialization, finite hit
count, mid-save resume and the next independent stop remain unmeasured.
No implementation or acceptance claim is made by creating this ready ticket.

### Evidence handoff, 2026-09-08

E-SAP-COMPAT-GENERAL-239-001 now provides the complete 92-operation native
suffix, successful native return/flag clear, exact write-size and return-PC
metadata, repeat/resume pins, and the next independent refusal. The unchanged
production baseline was reproduced twice with every prior checkpoint intact.
Only this ticket and `docs/{current-status,migration-evidence}.md` change;
runtime, budgets, profiles and firmware remain untouched. Diagnostic sources,
snapshots and pixels are retained only in `/tmp/semu-762-save.YOUCY2/`.

The isolated diagnostic has 512 extra logical operations, five-billion-
instruction / 35-billion-ns absolute bounds and 5,000 changed-frame capacity.
Only 92 extra hits are actually consumed: open, 90 full writes of 1,505 total
bytes, close. Both native scalar success checks and the eight empty variable
record branches agree with pristine disassembly. The serializer returns one;
firmware closes the handle and clears the pending flag without intervention.
No ABI repair is indicated. The diagnostic allowance is not production evidence
for 512 extra operations.

Two runs from the original pre-screen prefix, two from the production refusal,
and the mid-save resume converge to final snapshot SHA-256
`613712b78fd1ba748517e710d70280300bee0a48ee43133c9fd9f8070f5442f8`.
The mid-save checkpoint is
`76a7af5385eb2ddf5dfe94f6607db34e6e820edb06f5054a31bce7a5ef4ada66`;
resume reproduces the exact remaining log suffix. Final frame CRC `405d1af6`
reads `Define your profile`; all 676 prefix-run submissions succeed. The next
stop is the existing fifth GPS-awake refusal, PC `0x001291cc`, instruction
2,363,623,546 / virtual ns 32,619,070,564, with GPS hits `2,2,4`.
The ledger contains every trace, pixel, file, source and library hash.

Commands/results: both unchanged production observer runs and final-state
inspection pass; both extended diagnostic repeat pairs and mid-save resume
pass with the pins above. `make test TEST_FILTER=sapporo_239` passes 44 tests,
`make test TEST_FILTER=machine_snapshot` passes four, `make check` passes all
829, `make check-lines` passes with existing review warnings, and
`make check-task-contracts` validates 131 tickets. No private input was skipped.
No new C behavior entered the tree; the prior accepted sanitizer/native gates
are not represented as rerun by this evidence-only continuation.

### Proposed next integration (not instantiated or implemented here)

Create a separate Sapporo 2.39 General Settings Budget Integration ticket,
depending on accepted 762, 729 and 759. Its exact code scope should be:

- `src/compat/sapporo_239.c`: only the two constants and evidence comment,
  to 76,371 logical-file / 76,374 aggregate hits.
- `tests/unit/test_sapporo_239_compat.c`,
  `tests/unit/test_sapporo_239_history_budget.c`,
  `tests/unit/test_sapporo_239_activity_budget.c`: current-ceiling assertions
  and new-limit saturation only; preserve historical measured suffixes.
- New `tests/unit/test_sapporo_239_general_budget.c`: synthetic pre-existing
  1,505-byte file, the exact 90-write size sequence with recognizable synthetic
  bytes, successful close at the final allowed hit, and atomic excess refusal
  preserving CPU/RAM/file/handles/counters/log. Unknown path/mode must still
  refuse below budget; verify mid-write snapshot round trip.
- New `tests/integration/test_firmware_sapporo_239_general_budget.sh`: pinned
  native inputs with normal renderer, two pre-screen repeats, mid-save resume,
  exact operation/checkpoint hashes, and explicit wrong-input rejection.
- `docs/current-status.md`, `docs/migration-evidence.md` and that new ticket.

No public headers, Makefile, registry, profile, file ABI/capacity, snapshot
encoding, renderer or GPS change is needed. Preserve 754's 21-operation
activity suffix and all prior private goldens; require focused regression
failure before the constants change, `make check`, `make sanitize`, and the
existing exact 2.22 live-input / 2.39 GPS-awake gates plus the new native gate.
If a mismatch exposes missing evidence, do not repin it automatically.

Evidence acceptance has passing observations; status/index remain unchanged
for separate integrator review. Watch-face activation, further profile inputs,
the fifth GPS pulse and post-setup menus remain unsupported/unproven here.

### Integrator acceptance, 2026-09-08

Separate planning review accepts the complete suffix, native success checks,
two equivalent prefix runs, mid-save continuation, and next explicit refusal
in E-SAP-COMPAT-GENERAL-239-001. The retained trace/snapshot pins and passing
829-test evidence were checked; no acceptance condition is waived. All
dependencies are done. Ticket 763 instantiates the proposed integration,
including a private-only C observer helper so its firmware gate need not
depend on temporary diagnostic sources. This pass changes status/index;
the earlier evidence implementation did not. No production change is included
in this acceptance.
