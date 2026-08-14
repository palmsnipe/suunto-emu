# 316 — Apollo4 MSPI Controller

**Status:** ready
**Phase:** 3
**Dependencies:** 285, 295, 298

## Goal

Implement verified common MSPI command/queue/descriptor/IRQ behavior. This unlocks integration 320 and flash/panel tickets 402/404; opcode semantics and DMA copying remain deferred.

## Execution Budget

Complete within the stated one-to-three agent-day budget: Implement the common observed MSPI register, command-queue, descriptor, PIO, completion, and IRQ behavior for Sapporo MSPI1/MSPI2.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, the ticket-specific evidence row from 295, and every baseline source named below.

## Current Baseline

No MSPI ranges exist locally. `SapporoApollo4Mspi1` accepts observed descriptor/queue shapes; `SapporoApollo4Mspi2` mixes controller and flash semantics. Current external flash is plain RAM, so no typed endpoint receives MSPI traffic.

## Allowed Files

Only `src/soc/apollo4/{mspi.c,mspi.h}` and `tests/devices/test_apollo4_mspi.c`.

## Frozen Interfaces

Opaque controller per instance with create/destroy/reset/bus ops, one typed endpoint attachment, IRQ sink, and immutable DMA-request callback. Descriptor/queue reads use checked bus copies; validate full range/count/control before endpoint calls. Device opcodes are endpoint data, never decoded here.

## Evidence Inputs

`E-A4-MSPI-001` must cite `SapporoApollo4Mspi1.CompleteObservedCommand/CompleteObservedCommandQueue` and `SapporoApollo4Mspi2.ReadDoubleWord/WriteDoubleWord`, exact `0x40061000`/`0x40062000` ranges and IRQs 21/22, with authentic queue/descriptor trace hashes. Block untraced forms.

## Implementation

Implement only evidence-enumerated behavior behind the frozen interface; validate the complete operation before mutation and split any hand-written file approaching 300 lines.

## Tests and Commands

`make test TEST_FILTER=apollo4_mspi` runs only its binary and exits 0; direct command, queue/descriptor, range/count refusal, endpoint wait/refuse, IRQ enable/status/clear, DMA-request emission, reset, and repeatability pass. `make check` exits 0.

## Acceptance

Verified controller transcripts match without flash/panel special cases; malformed descriptors cannot read guest memory; completion IRQ timing is deterministic; no partial queue retirement on refusal.

## Forbidden Scope

No flash ID/program/erase, panel protocol, OTA staging, Ulsan persistence state, DMA copy, guessed queue forms, or integration/public edit.

## Handoff

Report instance differences, accepted descriptor/queue forms, IRQ contract, evidence hashes, and 317/405/410 attachment needs.
