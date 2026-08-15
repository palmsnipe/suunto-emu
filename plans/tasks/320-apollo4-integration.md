# 320 — Apollo4 Phase 3 Integration

**Status:** done
**Phase:** 3
**Dependencies:** 300, 302, 304, 305, 310, 312, 314, 315, 316, 317, 318

## Goal

Integrate all verified Apollo4 blocks and reach the bounded `apollo4-stable-wfi` synthetic checkpoint twice. This unlocks Sapporo profile/wiring 400; physical devices and private firmware remain deferred.

## Execution Budget

Complete within the stated one-to-three agent-day budget: Replace the legacy regbank aggregate with the verified blocks, wire scheduler/CPU IRQs, and pass a synthetic Apollo4 startup/WFI gate.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, the ticket-specific evidence row from 295, and every baseline source named below.

## Current Baseline

`apollo4.c:semu_apollo4_create/reset` owns five regbanks and raw GPIO; `apollo4_internal.h` embeds them; `regbank.c` retains listed words. `test_apollo4.c` checks only clock `0x84` retention and GPIO level. No controller is attached to scheduler/CPU.

## Allowed Files

Only `src/soc/apollo4/{apollo4.c,apollo4_internal.h,regbank.c}`, `include/semu/apollo4.h`, `tests/devices/test_apollo4.c`, `tests/integration/test_apollo4_startup.c`, and `fixtures/synthetic/apollo4/**`.

## Frozen Interfaces

Preserve public create/destroy/reset/GPIO functions; extend create options only for scheduler, CPU IRQ sink, and endpoints defined by 298. Integration owns block lifetimes and reset order from 304. No component-private header is modified.

## Evidence Inputs

All dependency evidence IDs must be verified except `E-A4-MRAM-001`, which may remain missing only if MRAM accesses stay fail-closed and the synthetic gate does not touch them. Gate sequence must be derived from trace IDs, not the old regbank test.

## Implementation

Implement only evidence-enumerated behavior behind the frozen interface; validate the complete operation before mutation and split any hand-written file approaching 300 lines.

## Tests and Commands

`make test TEST_FILTER=apollo4_startup` runs only the integration binary, reaches `checkpoint=apollo4-stable-wfi` under 250000 instructions/100000000 virtual ns, and exits 0. `make test TEST_FILTER=apollo4` runs all Apollo4 unit binaries and exits 0. `make sanitize TEST_FILTER=apollo4` and `make check` exit 0.

## Acceptance

Legacy regbanks are removed or unused; synthetic guest produces the expected ordered MMIO/DMA/IRQ transcript twice; stop is WFI deadlock, not unsupported/unmapped; all unknown accesses still refuse.

## Forbidden Scope

No Sapporo profile/devices, private firmware, compatibility fixture, global read zero, weakening component tests, Nema/display, or source changes outside owned integration seams.

## Handoff

Report final map/reset/IRQ table, synthetic fixture provenance/hash, bounded transcript hashes, and frozen attachment API for Phase 4.
