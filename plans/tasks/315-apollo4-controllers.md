# 315 — Apollo4 IOM, MSPI, DMA, and MRAM

**Status:** blocked
**Phase:** 3
**Dependencies:** 300

## Goal

Implement evidence-backed IOM, MSPI, DMA, and MRAM controller behavior behind the frozen typed-bus contract.

## Allowed Files

`src/soc/apollo4/{iom,mspi,dma,mram}*`, `tests/unit/apollo4_{iom,mspi,dma,mram}*`; no shared public headers or board files.

## Frozen Interfaces

Use core range validation and typed transactions from 300. Validate complete DMA source/destination and controller state before transfer. `WAIT` must schedule a completion; `REFUSE` must make no partial mutation.

## Evidence Inputs

Apollo4/Sapporo controller traces and ledger IDs created during migration; explicit offsets only.

## Implementation

Model required command/register paths, FIFO/descriptor state, transfer completion, IRQ status/clear, MRAM access policy, and bus refusal propagation.

## Tests and Commands

`make test TEST_FILTER=apollo4_iom`; `make test TEST_FILTER=apollo4_mspi`; `make test TEST_FILTER=apollo4_dma`; `make test TEST_FILTER=apollo4_mram`; `make check`.

## Acceptance

Valid and invalid controller transcripts, DMA bounds/atomicity, wait/completion, IRQ, reset, and unknown-command refusal pass deterministically.

## Forbidden Scope

No attached sensors/flash, panel renderer, SDIO/eMMC, board addresses, partial DMA, blanket success responses, or public-interface edits.

## Handoff

Report supported command forms, evidence IDs, and unimplemented observed transactions for ticket 320.

