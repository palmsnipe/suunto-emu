# 312 — Apollo4 System Timer

**Status:** done
**Phase:** 3
**Dependencies:** 285, 295, 298

## Goal

Implement verified retained STIMER counter/compare/NVRAM/IRQ behavior. This supplies deterministic WFI wake behavior to integration 320; CTIMER and host persistence remain deferred.

## Execution Budget

Complete within the stated one-to-three agent-day budget: Implement retained STIMER configuration, compares, NVRAM words, and IRQ A–I used by Sapporo.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, the ticket-specific evidence row from 295, and every baseline source named below.

## Current Baseline

No `0x40008800` mapping exists. `Apollo4RetainedSystemTimer` special-cases NVRAM `0x50/0x5c`, configuration `0x00`, compares `0x20..0x3c`, and resynchronizes compare enable; current scheduler has no STIMER client.

## Allowed Files

Only `src/soc/apollo4/{stimer.c,stimer.h}` and `tests/devices/test_apollo4_stimer.c`.

## Frozen Interfaces

Opaque create/destroy/reset and bus ops; scheduler-based monotonically derived counter; nine numbered IRQ callbacks. Reset policy explicitly distinguishes retained evidenced NVRAM words from cleared volatile registers; no host persistence.

## Evidence Inputs

`E-A4-STIMER-001` must cite `SapporoApollo4Extensions.cs:Apollo4RetainedSystemTimer` methods and `sapporo-extensions.repl:sapporo_stimer`, plus an authentic trace hash establishing NVRAM reset retention, compare masks, counter units, and used IRQs. Block missing aspects individually.

## Implementation

Implement only evidence-enumerated behavior behind the frozen interface; validate the complete operation before mutation and split any hand-written file approaching 300 lines.

## Tests and Commands

`make test TEST_FILTER=apollo4_stimer` runs only its binary and exits 0; expected reset/retention, compare enable/rewrite, next-event wake, IRQ clear, wrap, unknown access, and repeatability tests pass. `make check` exits 0.

## Acceptance

All verified deadlines/IRQs and reset classes match; WFI can wake through scheduler events; equal deadlines preserve stable ordering; no fabricated persistent host file.

## Forbidden Scope

No CTIMER/SysTick, guessed counter frequency, all-offset retention, host wall time, board wake policy, or integration/public edits.

## Handoff

Report counter unit, retained fields, compare/IRQ table, evidence hash, and 320 wiring requirements.
