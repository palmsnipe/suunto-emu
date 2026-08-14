# 190 — CPU Contract, State, and Fetch

**Status:** ready
**Phase:** 2
**Dependencies:** 120

## Goal

Freeze the ARMv7E-M CPU integration contract and implement architectural state, reset, checked Thumb fetch, condition/flag helpers, and decoder dispatch boundaries.

## Allowed Files

`include/semu/cpu.h`, `src/cpu/armv7m/{cpu,state,fetch,decode,flags}*`, `tests/unit/cpu_{state,fetch,flags}*`, Makefile source-list additions.

## Frozen Interfaces

CPU exposes create/reset/step, IRQ line/priority updates, pending-event notification, and read-only state/fault inspection. It accesses memory only through `semu_bus`; decoder families return executed, fault, unsupported, or wait. PC observes Thumb architectural semantics.

## Evidence Inputs

ARMv7E-M architectural behavior; `E-CPU-0001`; machine/bus headers frozen by 120.

## Implementation

Model R0-R15, xPSR, MSP/PSP, masks, CONTROL, privilege, event/exclusive state, and FPU storage placeholder. Dispatch 16/32-bit encodings without executing unassigned encodings.

## Tests and Commands

`make test TEST_FILTER=cpu_state`; `make test TEST_FILTER=cpu_fetch`; `make test TEST_FILTER=cpu_flags`; `make check`.

## Acceptance

Reset vector, alignment, fetch boundary, condition code, and unsupported-encoding tests pass; no decoder reads host endianness directly; public CPU header is frozen for 200/210.

## Forbidden Scope

No instruction-family implementation beyond helpers, exceptions, NVIC, FPU arithmetic, SoC registers, compatibility hooks, or permissive undefined encodings.

## Handoff

Report frozen dispatch contract and independent path globs for parallel tickets 200 and 210.
