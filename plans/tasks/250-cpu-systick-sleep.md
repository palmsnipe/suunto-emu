# 250 — SysTick, PendSV/SVC Scheduling, WFI, and WFE

**Status:** ready
**Phase:** 2
**Dependencies:** 245
**Estimate:** 2 days

## Goal

Complete deterministic SysTick and architectural sleep/event behavior, proving SVC→PendSV and scheduled wake paths needed by an RTOS guest.

## Execution Budget

One cheaper-model agent for two working days; two implementation files, three focused test binaries, fixtures, and no RTOS task implementation.

## Required Reading

- `src/cpu/armv7m/{cpu.c,nvic.c,scb.c,systick.c,sleep.c}` after ticket 245
- `include/semu/scheduler.h`, `src/core/scheduler.c`
- `docs/{execution-model.md,migration-evidence.md,testing-strategy.md}`
- `E-CPU-0003` SysTick, SCR, WFI/WFE/SEV, and wake rules

## Current Baseline

`cpu.c::finish_instruction` advances one scheduler tick; `step_waiting_cpu` runs the next event or reports `SEMU_STOP_WFI_DEADLOCK`; 16/32-bit WFI set `waiting_for_interrupt`. There is no SysTick SCS block, event register, wake-source classification, SLEEPONEXIT, SEVONPEND, or PendSV scheduling test.

## Allowed Files

- `src/cpu/armv7m/systick.c`, `src/cpu/armv7m/sleep.c`
- `tests/unit/test_cpu_systick.c`, `tests/unit/test_cpu_sleep.c`
- `tests/unit/test_cpu_pendsv.c`
- `tests/fixtures/cpu/systick/**`, `tests/fixtures/cpu/sleep/**`
- assigned rows in `tests/fixtures/cpu/coverage.tsv`

## Frozen Interfaces

Use the integer scheduler API, 190's event signal, 240's SVC handling, and 245's pending/arbitration/SCR hooks unchanged. One retired instruction costs one nanosecond in the present functional model. Sleep fast-forward may run only the next deterministic scheduler event, then re-evaluate eligibility.

## Evidence Inputs

`E-CPU-0003` is mandatory for SysTick CSR/RVR/CVR/CALIB, COUNTFLAG, SCR, WFI/WFE/SEV, pending wake, SLEEPONEXIT, and SEVONPEND. This ticket is **blocked** if those sections or the project's one-tick functional-time rule in `docs/execution-model.md` are absent.

## Implementation

Implement 24-bit SysTick reload/current/count flag/interrupt scheduling, enable/disable/reprogram behavior, processor-clock functional timing, WFI wake versus exception entry under masks, one-bit event register for WFE/SEV, SEVONPEND, SLEEPONEXIT, and deadlock only with no pending/event/future scheduler source. Add a small SVC handler that pends PendSV and returns through both handlers.

## Tests and Commands

```sh
make test TEST_FILTER=cpu_systick
make test TEST_FILTER=cpu_sleep
make test TEST_FILTER=cpu_pendsv
```

`test_cpu_systick` must print passes for reload 0/1/0xffffff, COUNTFLAG clearing, and reprogram/cancel. `test_cpu_sleep` must print passes for masked/unmasked wake, pre-signaled WFE, one-shot SEV, scheduled wake, deadlock, and SLEEPONEXIT. `test_cpu_pendsv` must print the SVC→PendSV order. Bounds: 128 instructions and 10,000 virtual ticks.

## Acceptance

All tests stop at declared halt/deadlock/budget reasons and exact virtual times; repeated runs have identical exception order; no wall-clock wait occurs; scheduler queue changes cannot skip an eligible interrupt.

## Forbidden Scope

No Apollo4 STIMER/timers, tick calibration against host time, RTOS task stacks, FPU context, debug timers, busy polling when a deterministic event exists, or public/shared-header changes.

## Handoff

Report SysTick register contract, wake/deadlock table, SVC/PendSV transcript, exact virtual times, commands/results, and limitations passed to ticket 280.
