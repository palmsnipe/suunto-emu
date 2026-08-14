# 245 — NVIC, SCB, Priorities, Nesting, and Fault Escalation

**Status:** blocked
**Phase:** 2
**Dependencies:** 240
**Estimate:** 2–3 days

## Goal

Implement CPU-owned System Control Space behavior for NVIC/SCB, deterministic priority arbitration and nesting, and architectural configurable-fault escalation.

## Execution Budget

One cheaper-model agent for at most three working days; two implementation files, two test binaries, fixtures, and CPU-owned SCS offsets only.

## Required Reading

- `src/cpu/armv7m/{armv7m_internal.h,cpu.c,exceptions.c,special.c}` after ticket 240
- `include/semu/{cpu,bus,scheduler}.h`
- `docs/{execution-model.md,migration-evidence.md,testing-strategy.md}`
- `E-CPU-0003` NVIC, SCB, priority, and configurable-fault sections

## Current Baseline

`cpu.c::pending_irq` chooses the lowest asserted IRQ and masks all IRQs whenever BASEPRI is nonzero; `irq_level[256]` stores level only. There is no enable/pending/active/priority state, priority mask, SCS register routing, SHCSR/CFSR/HFSR/BFAR/MMFAR, nesting, preemption, or tail chaining.

## Allowed Files

- `src/cpu/armv7m/nvic.c`, `src/cpu/armv7m/scb.c`
- `tests/unit/test_cpu_nvic.c`, `tests/unit/test_cpu_faults.c`
- `tests/fixtures/cpu/nvic/**`, `tests/fixtures/cpu/faults/**`
- assigned rows in `tests/fixtures/cpu/coverage.tsv`

## Frozen Interfaces

Use 190's IRQ level/priority setters and fault inspection, 240's exception request/frame helpers, and existing bus API. CPU intercepts only documented SCS offsets; every unimplemented/reserved offset refuses. Model 256 external lines internally but expose only architecturally addressable implemented register words.

## Evidence Inputs

`E-CPU-0003` is mandatory. This ticket is **blocked** unless it pins NVIC ISER/ICER/ISPR/ICPR/IABR/IPR, SCB ICSR/VTOR/AIRCR/SCR/CCR/SHPR/SHCSR/CFSR/HFSR/BFAR/MMFAR/CPACR, priority comparison, exception escalation, and return arbitration. Apollo4's implemented priority-bit mask is deferred to profile/SoC evidence and defaults to the reference-configured Cortex-M4 value only when explicitly pinned.

## Implementation

Add enable, software/line pending, active, and raw priority state; fixed system-exception priorities; PRIGROUP comparison; PRIMASK/BASEPRI/FAULTMASK filtering; preemption and nested return; PendSV/NMI requests; VTOR relocation/alignment; configurable fault status/address recording; disabled-fault escalation to HardFault; lockup as an explicit firmware-assert stop only where the reference requires it. Implement tail chaining only after separate entry/return tests pass.

## Tests and Commands

```sh
make test TEST_FILTER=cpu_nvic
make test TEST_FILTER=cpu_faults
make test TEST_FILTER=cpu_exceptions
```

`test_cpu_nvic` must print passes for enable/disable/set/clear/active registers, level re-pending, tie ordering, masks, PRIGROUP, nesting/preemption, PendSV, and VTOR. `test_cpu_faults` must print passes for precise bus/usage faults, HardFault escalation, and invalid SCS refusal; `test_cpu_exceptions` remains passing. Bound every test to 64 instructions/2,000 ticks and repeat the arbitration transcript identically.

## Acceptance

Arbitration is priority-based rather than IRQ-number polling; SCS reads/writes and W1C fields match vectors; nested frames restore in order; fault status/address and escalation are exact; unknown offsets fail closed.

## Forbidden Scope

No Apollo4 peripheral registers, profile-specific IRQ wiring, SysTick counter, FPU arithmetic/stacking, debug/DWT/MPU, host signals/threads, permissive SCS register bank, or public-header edits.

## Handoff

Report implemented SCS offsets, IRQ/priority limits, arbitration transcript, fault matrix, exact commands/results, and frozen hooks for SysTick and FPU context work.
