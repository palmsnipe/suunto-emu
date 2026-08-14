# 320 — Apollo4 Phase 3 Integration

**Status:** blocked
**Phase:** 3
**Dependencies:** 310, 315

## Goal

Integrate Apollo4 blocks with CPU/machine and reproduce known Sapporo startup transactions through a stable native RTOS/WFI checkpoint.

## Allowed Files

`src/soc/apollo4/{soc,map,irq}_*`, `tests/integration/apollo4_*`, `fixtures/synthetic/apollo4/**`, Makefile source/test lists, public-header corrections required for integration, `plans/index.tsv` status only.

## Frozen Interfaces

Finalize SoC create/reset/map, IRQ, and controller attachment interfaces; preserve typed transaction and stop semantics. Synthetic fixture manifests name load/vector addresses and hashes.

## Evidence Inputs

Outputs and ledger IDs from 300/310/315; Sapporo startup sequence evidence migrated before golden creation.

## Implementation

Wire clocks, reset, IRQs, timers, UART, IOM, MSPI, DMA, and MRAM; add deterministic startup fixture and ordered register/controller checkpoints; keep unattached devices as explicit refusals.

## Tests and Commands

`make test TEST_FILTER=apollo4_integration`; `make test TEST_FILTER=apollo4_startup`; `make test TEST_FILTER=determinism`; `make check`; `make sanitize`.

## Acceptance

The fixture reaches stable WFI with expected ordered transactions, no CPU patches, no unmapped access, no unsupported instruction, and identical repeated-run summaries.

## Forbidden Scope

No Sapporo board wiring/profile, physical devices, native firmware, Nema rendering, compatibility fixtures, or relaxing controller refusals to reach the checkpoint.

## Handoff

Publish final controller attachment contract and Phase 3 transcript for board agents.

