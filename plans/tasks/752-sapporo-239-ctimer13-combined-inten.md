# 752 — Sapporo 2.39 Combined Timer13 Interrupt Enable

**Status:** done
**Phase:** 7
**Dependencies:** 310, 727, 729, 615

## Goal

Accept the exact combined Timer0 CMP0 / Timer7 CMP0 / Timer13 CMP1 INTEN
value requested during native 2.39 UI startup, then identify the next boundary.

## Execution Budget

One model-day for the exact-value correction, regression and authentic runs.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; ticket 727 and 751 handoffs;
E-SAP-0029, E-SAP-COMPAT-QUIET-READ-239-001 and E-SAP-CTIMER13-INTEN-239-001;
`src/soc/apollo4/timer.h`, `src/soc/apollo4/timer_internal.h`, both timer
implementation files below, `src/core/snapshot_io.h`, `include/semu/scheduler.h`,
`tests/devices/test_apollo4_timer.c`, `tests/devices/test_apollo4_timer_snapshot.c`,
`tests/integration/test_firmware_sapporo_239_quiet_read.sh`;
hash-pinned PAC `timer.rs` INTEN offset and `timer/inten.rs` bit 27;
pristine disassembly `0x000cb84c..0x000cb854` and the pre-fault registers.

## Current Baseline

At instruction 607,105,617, PC `0x000cb852` writes `0x08004001` to
`0x40008060` after reading `0x00004001`. Existing INTEN values already
include Timer13 CMP1 alone and with Timer0; only this combination is missing.

## Allowed Files

- `src/soc/apollo4/timer.c`, `src/soc/apollo4/timer_snapshot.c`
- `tests/devices/test_apollo4_timer_inten13.c`
- `tests/integration/test_firmware_sapporo_239_ctimer13_inten.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/752-sapporo-239-ctimer13-combined-inten.md`

## Frozen Interfaces

Retain public/private headers, snapshot format, scheduler, IRQ wiring/gates,
reset values, older exact values, compatibility budgets and historical goldens.

## Evidence Inputs

E-SAP-CTIMER13-INTEN-239-001 combines the native read/OR/store observation
with the independently hash-verified PAC definition of Timer13 CMP1 bit 27.
No new compare mode or IRQ policy is inferred from accepting the combination.

## Implementation

Append only `0x08004001` to MMIO and snapshot exact-value validators.
Prove retained readback and atomic neighboring-value/width/offset refusal;
exercise the existing channel-13 compare event and clear unchanged.

## Tests and Commands

Run `make test TEST_FILTER=apollo4_timer_inten13` before/after;
`make test TEST_FILTER=apollo4_timer`, `make test TEST_FILTER=machine_snapshot`,
`make check-task-contracts`, `make check-lines`, `make check`, `make sanitize`.
Run `SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_ctimer13_inten`.

## Acceptance

Regression fails before and passes after. Two fresh private logs/snapshots
match, the ticket-751 prefix retains exact hashes and resumes identically,
and the next independent boundary is identified. Validate all private inputs
before execution, preserve immutable flash and report any skipped conditions.

## Forbidden Scope

No generic INTEN mask, other register/value expansion, timer or IRQ redesign,
compatibility intervention, budget growth, CPU/device bypass, header/Makefile/
profile changes, historical golden edits or private bytes in Git.

## Handoff

Implemented and verified; status remains in-progress for integrator review.
Changed files are exactly the Allowed Files above. Production code adds one
whole-register value to each of the two existing validators. References used:
E-SAP-CTIMER13-INTEN-239-001, E-SAP-0029,
E-SAP-COMPAT-QUIET-READ-239-001, E-SAP-0011, and the new diagnostic
E-SAP-SERIALIZER-ARRAY-239-001. No headers, IRQ/scheduler logic, format, profile,
compatibility layer, hit limit or old golden changes.

`make test TEST_FILTER=apollo4_timer_inten13` first failed both cases at
the MMIO acceptance and independently constructed snapshot import. Afterward
both pass, covering readback, neighboring-value/width/offset atomic refusals,
unchanged channel-13 compare deadline/clear, reset, exact-format reserialization
and malformed/truncated import refusal with complete state comparison.
`make test TEST_FILTER=apollo4_timer` passes 20 cases;
`make test TEST_FILTER=machine_snapshot` passes one case. `make check` and
`make sanitize` each pass all 737 cases. `make check-task-contracts` validates
123 indexed tickets; `make check-lines` has existing warnings only.
`git diff --check` and
`sh -n tests/integration/test_firmware_sapporo_239_ctimer13_inten.sh` pass.
Direct runner checks with `TEST_PROFILE=sapporo-2.39.20` confirm an empty
`SEMU_SAPPORO_239_FULL_FLASH` skips (exit zero), while pointing it at the
known non-flash `tests/private/sapporo-2.39.20.22297/application.raw` refuses
the hash mismatch (exit two), before execution.

The exact private command in Tests and Commands passes, validating all three
components, two fresh logs/snapshots, the historical ticket-751 prefix and
identical resumed snapshot. Fresh log SHA-256:
`2223de22981528b7cd2df049be68ea2e4022627763da13aab9293ef1fbbf7e16`;
snapshot `74e45df965216d809cf41e090ee0fc56b740affbc6d32aec35413dd65db1aa0c`.
Boundary: `stop=budget pc=0x00079e1c instructions=608140266
virtual_time_ns=2147096849`. Prefix log remains
`740750cbc6460fe8b9c3b420a5509d992dc0757e50de9102da316df7d21be1ec`, snapshot
`15b5f2076d7107e50b693333d1d19bcd3bd18014b8208c33f02ed8e45676dd19`.
No reset/device refusal occurs before the boundary, file hits remain 76,258,
and source flash retains SHA-256
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
One more instruction executes native BKPT and halts at PC `0x00079e1e`,
instruction 608,140,267, time 2,147,096,850 ns.

Read-only tracing identifies `ChunkSerializer.cpp:38`, not another timer
fault. A native array consumer reads the synthetic JSON value at LID `0xa432`
as a native object, yielding count 24,946 and allocation size 199,568 in a
16-byte buffer. The complete replacement native value ABI remains unresolved;
recover it before changing the cache, and preserve the bounds check/assertion.
No normal 2.39 frame or physical interrupt-mask completeness is claimed.
No additional integrator-owned interface change is requested. All private
firmware, snapshots and diagnostic artifacts stay outside Git. Historical
runners remain unchanged; only ticket 751's subsequent INTEN refusal is
superseded, while its pre-refusal hashes are enforced by the new runner.
