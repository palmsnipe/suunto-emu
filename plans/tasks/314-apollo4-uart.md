# 314 — Apollo4 UART

**Status:** ready
**Phase:** 3
**Dependencies:** 285, 295, 298

## Goal

Implement the Apollo4 UART register/FIFO/IRQ transcript used by GPS. This unlocks integration 320 and CXD5610 attachment 416; GPS protocol and host serial remain deferred.

## Execution Budget

Complete within the stated one-to-three agent-day budget: Implement the UART register/FIFO/IRQ behavior exercised by the Sapporo GPS transport.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, the ticket-specific evidence row from 295, and every baseline source named below.

## Current Baseline

No UART MMIO or controller exists. `semu_serial_endpoint` is synchronous; `SapporoCxd5610Transport.BaudRate` reports 921600, but that device model does not prove Apollo4 UART register values or FIFO timing.

## Allowed Files

Only `src/soc/apollo4/{uart.c,uart.h}` and `tests/devices/test_apollo4_uart.c`.

## Frozen Interfaces

Opaque create/destroy/reset/bus ops; attach/detach one byte-stream endpoint; schedule RX injection and TX completion; numbered IRQ sink. FIFO capacity, status bits, thresholds, and baud divisor come only from evidence. Detached TX and invalid accesses refuse atomically.

## Evidence Inputs

`E-A4-UART-001` must name the exact Apollo4 UART base/IRQ and provide an ordered 2.22.60 MMIO+byte trace through first GPS request. It may cite `SapporoCxd5610Transport.WriteChar` and `sapporo-gps-transport.repl`, but these are endpoint evidence only. Block without controller trace.

## Implementation

Implement only evidence-enumerated behavior behind the frozen interface; validate the complete operation before mutation and split any hand-written file approaching 300 lines.

## Tests and Commands

`make test TEST_FILTER=apollo4_uart` runs only its binary and exits 0; expected reset, divisor/status, FIFO threshold, overflow/underflow, RX scheduling, TX order, IRQ clear/reassert, detached endpoint, wrong-width, and repeated transcript tests pass. `make check` exits 0.

## Acceptance

Verified MMIO/byte/IRQ order matches; FIFO limits are enforced; `WAIT` always has a scheduled wake; no bytes are lost or fabricated; two runs match.

## Forbidden Scope

No GPS protocol/exact exchanges, host serial device, wall-clock baud timing, guessed FIFO depth, DMA, or integration/public edit.

## Handoff

Report mapped instance/base/IRQ, FIFO/baud rules, evidence hash, and endpoint attachment call for 320/416.
