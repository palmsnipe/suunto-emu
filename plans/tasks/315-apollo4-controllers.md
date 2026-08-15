# 315 — Apollo4 IOM Controller

**Status:** done
**Phase:** 3
**Dependencies:** 285, 295, 298

## Goal

Implement verified IOM PIO/FIFO/IRQ behavior and DMA-request emission. This unlocks integration 320 and typed Sapporo endpoints; DMA copying and physical device semantics remain deferred.

## Execution Budget

Complete within the stated one-to-three agent-day budget: Implement observed IOM PIO command, FIFO, device selection, completion, and IRQ behavior while delegating DMA to ticket 317.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, the ticket-specific evidence row from 295, and every baseline source named below.

## Current Baseline

No IOM range is mapped. `Apollo4IomDma` and `SapporoApollo4Iom4` wrap Renode, mix PIO, DMA, and every physical device, and include permissive compatibility paths. Local devices are not attached to the SoC.

## Allowed Files

Only `src/soc/apollo4/{iom.c,iom.h}` and `tests/devices/test_apollo4_iom.c`.

## Frozen Interfaces

Opaque create/destroy/reset/bus ops; attach typed endpoint by bus instance/address/chip-select; emit immutable `semu_dma_request` through the 298 callback; IRQ sink. PIO command validation precedes endpoint mutation. Unknown address, command, continuation, or FIFO shape returns refusal.

## Evidence Inputs

`E-A4-IOM-001` must cite `Apollo4IomDma.ReadDoubleWord/WriteDoubleWord/RetireDirectCommand` and `SapporoApollo4Iom4` equivalents, plus exact IOM0/2/3/4/6 bases, IRQs, and authentic PIO MMIO/transaction trace hashes. Do not copy the wrappers' device shortcuts.

## Implementation

Implement only evidence-enumerated behavior behind the frozen interface; validate the complete operation before mutation and split any hand-written file approaching 300 lines.

## Tests and Commands

`make test TEST_FILTER=apollo4_iom` runs only its binary and exits 0; reset, PIO read/write, continued transaction, FIFO bounds, address refusal, endpoint refusal, IRQ status/clear, DMA-request emission, wrong offset/width, and repeatability pass. `make check` exits 0.

## Acceptance

All verified PIO sequences match; no device-specific address exists in controller code; refused commands have no partial endpoint/FIFO mutation; DMA is requested, not performed here.

## Forbidden Scope

No physical device, DMA memory copy, broad Renode wrapper port, MSPI/UART, direct CPU access, compatibility response, or integration/public edit.

## Handoff

Report instances/bases/IRQs, command forms, attachment/DMA APIs, evidence hashes, and gaps for 317/320.
