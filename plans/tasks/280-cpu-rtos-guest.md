# 280 — Synthetic ARM_CM4F RTOS Guest

**Status:** done
**Phase:** 2
**Dependencies:** 250, 275
**Estimate:** 2–3 days

## Goal

Build a tiny committed synthetic guest that proves SVC startup, PendSV context switching, SysTick preemption, nested IRQs, WFI wake, and FP context preservation without private firmware.

## Execution Budget

One cheaper-model agent for at most three working days; one tiny guest fixture, one integration binary, guest helper files, and a 100,000-instruction ceiling.

## Required Reading

- `include/semu/{cpu,bus,scheduler}.h`
- `tests/support/{cpu_fixture.c,cpu_fixture.h}` and all ticket 250/275 handoffs
- `docs/{execution-model.md,migration-evidence.md,testing-strategy.md}`
- `E-CPU-0005` exact ARM_CM4F port revision and AAPCS32 context rules

## Current Baseline

There are no synthetic CPU binaries or integration tests. `tests/unit/test_cpu.c` loads short byte arrays into a 4 KiB RAM fixture. `fixtures/synthetic/rtos/**` does not exist. Existing authentic `E-SAP-0008` reaches a default-handler loop and is not an RTOS success gate.

## Allowed Files

- `fixtures/synthetic/rtos/**`
- `tests/integration/test_cpu_rtos_guest.c`
- `tests/support/cpu_guest.c`, `tests/support/cpu_guest.h`
- `Makefile` only to include `tests/integration/test_*.c` in `TEST_SOURCES`/`TEST_BINS` and add their integration-test build rule

## Frozen Interfaces

Use the public CPU/bus/scheduler API and Phase 2 instruction/exception behavior unchanged. The committed fixture is raw little-endian bytes plus C99/assembly source, linker layout, build provenance, SHA-256, vector map, and expected checkpoint table. Tests consume the committed binary; rebuilding it is an optional maintainer action, not a normal dependency.

## Evidence Inputs

`E-CPU-0002` through `E-CPU-0005` and `E-CPU-0010` are mandatory. `E-SAP-0008` is only a negative boundary demonstrating why a synthetic gate is needed. The exact ARM_CM4F port revision and AAPCS32 context rules come from `E-CPU-0005`; the hardware-saved FP context and S16–S31 software-preservation boundary come from `E-CPU-0010`. This ticket is ready because those inputs are present.

## Implementation

Create two tasks with separate PSP stacks and callee-saved registers; start through SVC; switch through PendSV; drive periodic SysTick; pend a higher-priority synthetic external IRQ inside a lower-priority handler; execute WFI in idle; perform FP arithmetic in one task and verify S16–S31 software preservation plus hardware-saved low registers. Emit deterministic checkpoint IDs to a RAM mailbox, then BKPT halt.

## Tests and Commands

```sh
make test TEST_FILTER=cpu_rtos_guest
make test TEST_FILTER=cpu_fpu_context
make test TEST_FILTER=cpu_systick
```

`test_cpu_rtos_guest` must print the ordered checkpoints `reset,svc-start,task-a,tick,pendsv,task-b,irq-low,irq-high,irq-low-return,wfi,wake,fp-restored,done`, final `SEMU_STOP_HALT`, a fixed instruction count/virtual time recorded with the fixture, and matching mailbox/stack guards. A second run must produce byte-identical checkpoint/state output. Limit each run to 100,000 instructions and 1,000,000 ticks.

## Acceptance

The committed binary hash matches provenance; both runs match exactly; all required checkpoints appear once in order; stack guards and integer/FP task state survive; there is no unsupported instruction, compatibility hit, host tool/runtime dependency, or private firmware access.

## Forbidden Scope

No copied FreeRTOS source/binary, full scheduler/kernel, Apollo4 peripherals, private firmware, dynamic assembler dependency, timing relaxation, compatibility hooks, or changes to CPU semantics to fit the guest.

## Handoff

Report fixture source/hash/layout, expected transcript and budgets, exact commands/results, deterministic diff result, and any unsupported instruction sent back to the owning family ticket.
