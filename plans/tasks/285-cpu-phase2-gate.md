# 285 — Phase 2 CPU Integration and Determinism Gate

**Status:** blocked
**Phase:** 2
**Dependencies:** 280
**Estimate:** 1–2 days

## Goal

Integrate and audit all Phase 2 CPU families, prove coverage/refusal discipline and deterministic RTOS behavior, and freeze the CPU contract for Apollo4 work.

## Execution Budget

One integration agent for at most two working days; wiring/conflict fixes and one phase-gate binary only, with semantic defects returned to owning tickets.

## Required Reading

- `include/semu/cpu.h` and every handoff from tickets 180–280
- `tests/fixtures/cpu/coverage.tsv`, `fixtures/synthetic/rtos/**`
- `docs/{architecture.md,execution-model.md,migration-evidence.md,testing-strategy.md}`
- `plans/roadmap.md` Phase 2 exit gate

## Current Baseline

Before Phase 2, five combined CPU source files and one smoke-test binary provide a partial interpreter, two `E-CPU-0001` regressions, basic exception frames, level IRQ, WFI deadlock, partial MSR/FPSCR, and selected Thumb-32 instructions. Tickets 190–280 replace that partial status with family tests and a synthetic RTOS gate.

## Allowed Files

- `src/cpu/armv7m/**` integration-only conflict fixes
- `tests/integration/test_cpu_phase2.c`
- `tests/fixtures/cpu/coverage.tsv`
- `fixtures/synthetic/rtos/**` expected metadata only; no guest behavior changes
- `Makefile` CPU source/test integration only

## Frozen Interfaces

The `semu/cpu.h` API frozen by 190 is final for Phase 3. Family entry points remain internal. Coverage states and RTOS checkpoint format from 180/280 are final. Integration fixes must be returned to the owning ticket when they change instruction semantics; this ticket only resolves wiring and cross-family state transitions.

## Evidence Inputs

`E-CPU-0001` through `E-CPU-0005` are mandatory, and every `implemented`/`verified` coverage row must cite one. This ticket is **blocked** if any ID is absent, any dependency handoff reports an unresolved correctness gap, or the RTOS fixture provenance/hash is incomplete.

## Implementation

Audit decoder overlaps/gaps, shared flag/PC/SP behavior, fault-before-retirement, exclusive invalidation, exception/FPU frame interaction, scheduler ordering, line-count policy, and refusal vectors. Add a cross-family phase test that runs the two Renode vectors, representative 16/32/DSP/FPU instructions, faults, nested IRQ/SysTick/sleep, then invokes the RTOS guest twice and compares normalized records.

## Tests and Commands

```sh
make test TEST_FILTER=cpu_phase2
make test TEST_FILTER=cpu_rtos_guest
make test TEST_FILTER=cpu_renode_regressions
make test TEST_FILTER=cpu
make sanitize TEST_FILTER=cpu
make check
```

`test_cpu_phase2` must print zero decode overlaps, zero uncited verified rows, both Renode regressions passed, the exact RTOS checkpoint sequence, identical two-run records, and final `phase2 gate: PASS`. `test_cpu_renode_regressions` and `test_cpu_rtos_guest` must be selected by their matching commands. Every CPU test must have an explicit instruction/time bound.

## Acceptance

All commands exit 0; no CPU file exceeds 500 lines and new files normally remain below 300; sanitizers report no issue; valid assigned encodings do not stop unsupported; reserved encodings and unknown SCS offsets fail closed; the public CPU contract is frozen and documented in the handoff to ticket 300.

## Forbidden Scope

No Apollo4 peripheral implementation, authentic-firmware success claim, compatibility hook, new profile, decoder-as-NOP fallback, test/golden weakening, host-specific assembly/JIT/external runtime dependency, or public API change after the gate run begins.

## Handoff

Publish the dependency completion list, CPU API/IRQ limits, coverage summary by family, unsupported list, exact commands/results, sanitizer/platform results, RTOS normalized transcript/hash, and explicit Phase 3 integration assumptions.
