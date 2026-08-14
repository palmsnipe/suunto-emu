# 310 — Apollo4 Timers and UART

**Status:** blocked
**Phase:** 3
**Dependencies:** 300

## Goal

Implement evidence-backed timer/STIMER and UART register behavior, scheduling, FIFO state, and IRQs needed by Sapporo.

## Allowed Files

`src/soc/apollo4/{timer,stimer,uart}*`, `tests/unit/apollo4_{timer,stimer,uart}*`; no shared public headers or board files.

## Frozen Interfaces

Use scheduler, IRQ, and UART transaction callbacks frozen by 300. Timers derive only from virtual time. UART external receive is a scheduled typed transaction; transmit refusal propagates to machine stop.

## Evidence Inputs

Apollo4/Sapporo register and traffic ledger entries created during 300 migration.

## Implementation

Cover only observed registers with explicit masks/reset values; schedule compare/overflow events; cancel/reschedule on control writes; model bounded RX/TX FIFOs and interrupt thresholds.

## Tests and Commands

`make test TEST_FILTER=apollo4_timer`; `make test TEST_FILTER=apollo4_uart`; `make test TEST_FILTER=wfi_wake`; `make check`.

## Acceptance

Reset, compare, wrap, reschedule, IRQ clear/reassert, FIFO overflow/underflow, unknown-offset refusal, and two-run timing tests pass.

## Forbidden Scope

No host serial port, wall-clock timing, GPS behavior, Timer14 product quirk, guessed register banks, public headers, or DMA owned by UART.

## Handoff

Report register coverage/transcripts and IRQ lines expected by integration ticket 320.

