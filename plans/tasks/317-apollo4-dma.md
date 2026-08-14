# 317 — Apollo4 DMA Engine

**Status:** blocked
**Phase:** 3
**Dependencies:** 285, 295, 298

## Goal

Implement atomic bounded IOM/MSPI DMA execution with deterministic completion. This unlocks controller integration 320 and flash transfers 402; controller decode and scatter/gather remain deferred.

## Execution Budget

Complete within the stated one-to-three agent-day budget: Implement checked, atomic IOM/MSPI DMA execution and deterministic completion scheduling behind the shared request callback.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, the ticket-specific evidence row from 295, and every baseline source named below.

## Current Baseline

There is no DMA object. Renode wrappers perform memory reads/writes inside `Apollo4IomDma.PrepareDma/CompleteDma`, `SapporoApollo4Iom4.PrepareDma/CompleteDma`, and `SapporoApollo4Mspi2.CompleteObservedDma`, sometimes byte-by-byte after partial validation.

## Allowed Files

Only `src/soc/apollo4/{dma.c,dma.h}` and `tests/devices/test_apollo4_dma.c`.

## Frozen Interfaces

Consume `semu_dma_request` frozen by 298: controller ID, direction, guest address, count, endpoint, continuation, completion callback/context. Validate entire guest range/count/direction and endpoint readiness before transfer. Completion is exactly once: immediate `OK`, scheduled `WAIT`, or no callback on `REFUSE`.

## Evidence Inputs

`E-A4-DMA-001` must cite the three source methods above and contain authentic IOM/MSPI DMA traces establishing direction bits, count interpretation, completion/command IRQ flags, continuation, and timing. Missing direction/form remains refused.

## Implementation

Implement only evidence-enumerated behavior behind the frozen interface; validate the complete operation before mutation and split any hand-written file approaching 300 lines.

## Tests and Commands

`make test TEST_FILTER=apollo4_dma` runs only its binary and exits 0; valid TX/RX, zero/overflow/cross-region range, endpoint wait/refuse, no-partial-write, continuation, completion-once, and two-run timing cases pass. `make check` exits 0.

## Acceptance

Bounds are checked before endpoint/memory mutation; RX stages data before one checked guest write; completions/IRQs match evidenced order; sanitizer run finds no overflow.

## Forbidden Scope

No IOM/MSPI register decode, device opcode, direct IRQ sink, scatter/gather without evidence, partial DMA, host threads, or integration/public edit.

## Handoff

Report accepted request shapes, atomicity strategy, timing, trace hashes, and controller callbacks for 320.
