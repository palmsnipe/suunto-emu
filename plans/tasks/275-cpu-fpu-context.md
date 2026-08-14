# 275 — FPU Exception Context and Lazy Stacking

**Status:** blocked
**Phase:** 2
**Dependencies:** 245, 250, 265, 270
**Estimate:** 2–3 days

## Goal

Integrate FPv4-SP basic/extended exception frames, FPCCR state, and deterministic lazy preservation needed for Cortex-M4F context switching.

## Execution Budget

One cheaper-model agent for at most three working days; one context file, one test binary, fixtures, and no OS-specific scheduler code.

## Required Reading

- `src/cpu/armv7m/{exceptions.c,nvic.c,sleep.c,fpu_transfer.c,fpu_context.c}` after dependencies
- `docs/{execution-model.md,migration-evidence.md,testing-strategy.md}`
- `E-CPU-0003/0004` exception/FP context sections
- `E-CPU-0005` pinned ARM_CM4F context layout only

## Current Baseline

`exceptions.c` from 240 supports basic integer frames only. CPU state has S0–S31/FPSCR but no active/lazy context fields. Ticket 260 adds FPCCR/FPCAR/FPDSCR; no current test covers EXC_RETURN frame bit 4 or FP state across nested exceptions.

## Allowed Files

- `src/cpu/armv7m/fpu_context.c`
- `tests/unit/test_cpu_fpu_context.c`
- `tests/fixtures/cpu/fpu-context/**`
- assigned rows in `tests/fixtures/cpu/coverage.tsv`

## Frozen Interfaces

Use 240's exception-frame extension hooks, 245's nested arbitration, 250's sleep return, and 260's FP system state unchanged. Extended frames contain S0–S15, FPSCR, and the reserved word in the exact referenced order; S16–S31 remain software-saved.

## Evidence Inputs

`E-CPU-0003`, `E-CPU-0004`, and `E-CPU-0005` are mandatory. This ticket is **blocked** unless EXC_RETURN bit 4, ASPEN/LSPEN/LSPACT, lazy allocation triggers, nested exception behavior, and ARM_CM4F context assumptions are pinned.

## Implementation

Implement active FP context tracking, basic versus extended frame selection, correct EXC_RETURN token, eager preservation, lazy reservation/materialization on first handler FP use, FPCCR/FPCAR updates, nested exception behavior, restore validation, alignment, and fault escalation during FP stack/unstack. Do not expose host pointers or copy S16–S31 automatically.

## Tests and Commands

```sh
make test TEST_FILTER=cpu_fpu_context
make test TEST_FILTER=cpu_exceptions
make test TEST_FILTER=cpu_nvic
```

`test_cpu_fpu_context` must print passes for no-FP basic frames, active-FP extended frames, lazy untouched/materialized paths, nested integer/FP handlers, MSP/PSP, alignment, EXC_RETURN bit 4, corrupt frames, refused stack/unstack, and exact S0–S15/FPSCR restore. `test_cpu_exceptions` and `test_cpu_nvic` remain passing. Bound each to 64 instructions/5,000 ticks.

## Acceptance

Frame bytes, SP, FPCCR/FPCAR, EXC_RETURN, restored raw FP state, and fault status are exact; lazy behavior is deterministic; no unreferenced automatic context save occurs.

## Forbidden Scope

No double precision, S16–S31 hardware stacking, OS-specific TCB code, host lazy allocation semantics, public/shared-header edits, or swallowing stack faults.

## Handoff

Report frame diagrams as test expectations, eager/lazy/nested transcript, exact commands/results, and the context contract delivered to ticket 280.
