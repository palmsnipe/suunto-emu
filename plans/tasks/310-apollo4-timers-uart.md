# 310 — Apollo4 CTIMER

**Status:** ready
**Phase:** 3
**Dependencies:** 285, 295, 298

## Goal

Implement the evidenced Apollo4 CTIMER modes, compares, and IRQs. This unlocks integration 320 and backlight boundary work in 404; STIMER and product-specific Timer14 behavior remain deferred.

## Execution Budget

Complete within the stated one-to-three agent-day budget: Implement the CTIMER instances and compare/IRQ behavior observed during startup and backlight control.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, the ticket-specific evidence row from 295, and every baseline source named below.

## Current Baseline

No timer block is mapped locally. `SapporoApollo4Extensions.cs:SapporoApollo4Timer` wraps Renode and retains only pattern-address offset `0x104`; `sapporo-extensions.repl` maps `0x40008000` with outputs 0–15 to NVIC 67–82.

## Allowed Files

Only `src/soc/apollo4/{timer.c,timer.h}` and `tests/devices/test_apollo4_timer.c`.

## Frozen Interfaces

Opaque timer create/destroy/reset plus bus ops; create accepts scheduler and numbered IRQ sink. Counter/compare derives only from integer virtual time. Writes validate the complete configuration before cancel/reschedule; pattern address is retained only if traced.

## Evidence Inputs

`E-A4-TIMER-001` must cite class methods `SapporoApollo4Timer.ReadDoubleWord/WriteDoubleWord/Reset`, `sapporo-extensions.repl:sapporo_timer`, and a 2.22.60 trace of every used offset/mask/IRQ. `E-SAP-BACKLIGHT-001` must verify CTIMER9 writes. Block rather than infer upstream Renode semantics.

## Implementation

Implement only evidence-enumerated behavior behind the frozen interface; validate the complete operation before mutation and split any hand-written file approaching 300 lines.

## Tests and Commands

`make test TEST_FILTER=apollo4_timer` runs only its binary and exits 0; reset, compare, wrap, cancel/reschedule, IRQ clear/reassert, CTIMER9 transcript, wrong width/offset, and two-run virtual-time cases pass. `make check` exits 0.

## Acceptance

Verified compare deadlines and IRQ outputs match exactly; disabled timers schedule nothing; rejected writes leave prior events intact; no wall-clock/cycle-accuracy claim.

## Forbidden Scope

No STIMER, UART, button/backlight policy, guessed timer modes, host timer, Timer14 Wismar quirk, or public/integration edits.

## Handoff

Report supported instances/modes, functional tick conversion, IRQ map, trace hashes, and 320/410 connection needs.
