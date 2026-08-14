# 220 — Exceptions, Special Registers, and NVIC Core

**Status:** blocked
**Phase:** 2
**Dependencies:** 200, 210

## Goal

Integrate architectural faults, SVC/PendSV/SysTick, nested IRQs, exception return, privilege/masks, special registers, WFI/WFE, and core NVIC/SCB behavior.

## Allowed Files

`src/cpu/armv7m/{exceptions,special,nvic,scb,systick,sleep}*`, `tests/unit/cpu_{exceptions,special,nvic,systick,sleep}*`, decoder registration glue, Makefile source lists.

## Frozen Interfaces

Retain CPU API from 190 and machine stop reasons from 120. SoC code may drive external IRQ pending/level/priority only; stacking and arbitration stay in CPU. WFI queries scheduler through the machine's frozen wake interface.

## Evidence Inputs

ARMv7E-M exception model and `docs/execution-model.md`; completed instruction coverage from 200/210.

## Implementation

Implement precise stacking/unstacking, EXC_RETURN, vector faults, tail/nested selection needed by FreeRTOS, BASEPRI/PRIMASK/FAULTMASK/CONTROL, exclusive clearing, SysTick scheduling, and deadlock detection.

## Tests and Commands

`make test TEST_FILTER=exception`; `make test TEST_FILTER=nvic`; `make test TEST_FILTER=systick`; `make test TEST_FILTER=sleep`; `make check`.

## Acceptance

Synthetic tests pass SVC, PendSV switching, nested priority/masks, vector/stack faults, SysTick wake, event-register WFE, and deadlocked WFI with stable virtual time.

## Forbidden Scope

No Apollo4 external controller registers, FPU stacking/arithmetic, cycle timing, global IRQ polling, or converting faults to compatibility events.

## Handoff

Report exception vectors, supported NVIC limits, scheduler assumptions, and failures reserved for 230.

