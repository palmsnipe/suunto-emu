# 200 — Thumb-16 Shifts and Arithmetic

**Status:** blocked
**Phase:** 2
**Dependencies:** 190
**Estimate:** 1–2 days

## Goal

Complete Thumb-16 shift, add/subtract, immediate, compare, and ALU arithmetic semantics with exact flag vectors.

## Execution Budget

One cheaper-model agent for at most two working days; one implementation file, one test binary, fixture rows, and no cross-family edits.

## Required Reading

- `src/cpu/armv7m/{armv7m_internal.h,thumb16_arith.c}` after ticket 190
- `tests/support/{cpu_fixture.c,cpu_fixture.h}` and `tests/fixtures/cpu/vector-format.md`
- `docs/migration-evidence.md` entries `E-CPU-0001` and `E-CPU-0002`
- `docs/testing-strategy.md`

## Current Baseline

After 190, baseline code originates from `thumb16.c` functions `shift`, `shifts_and_adds`, `immediate`, and arithmetic cases in `data_processing`; current smoke coverage is `test_reset_arithmetic_and_branch` only. Implementation target is `armv7m_exec16_arith` in `thumb16_arith.c`.

## Allowed Files

- `src/cpu/armv7m/thumb16_arith.c`
- `tests/unit/test_cpu_thumb16_arith.c`
- `tests/fixtures/cpu/thumb16-arith/**`
- assigned rows in `tests/fixtures/cpu/coverage.tsv`

## Frozen Interfaces

Use 190's family signature, register/SP helpers, portable shift helpers, bus API, and CPU fixture unchanged. Do not edit dispatch or shared flag helpers; report a wrong helper with a failing vector to the integrator.

## Evidence Inputs

`E-CPU-0002` instruction pseudocode is mandatory. This ticket is **blocked** if the ID or the exact sections for LSL/LSR/ASR/ROR, ADD/ADC/SUB/SBC/RSB, CMP/CMN, MUL, and immediate forms are absent.

## Implementation

Cover register/immediate shifts including zero and 32-or-greater rules; add/subtract register and immediate forms; MOV/CMP/ADD/SUB immediate; AND/EOR/ADC/SBC/ROR/TST/RSB/CMP/CMN/ORR/MUL/BIC/MVN. Apply APSR flags exactly, preserve C/V where specified, and reject reserved encodings without mutation.

## Tests and Commands

```sh
make test TEST_FILTER=cpu_thumb16_arith
make test TEST_FILTER=cpu_contract
```

`test_cpu_thumb16_arith` must print passes for zero, sign, carry, borrow, signed-overflow, shift amounts 0/1/31/32/33/255, multiply wrap, and one reserved/refusal case per routed encoding class. Each vector is one instruction and at most two steps; `test_cpu_contract` remains passing.

## Acceptance

All assigned valid encodings produce exact register/APSR/PC results; negative vectors stop unsupported with unchanged registers/memory; no host signed shift or overflow is used; coverage rows become `verified` only for tested forms.

## Forbidden Scope

No branches, memory, high-register operations, Thumb-32, exceptions, DSP/saturation, public/shared-header edits, generated tables, or opcode-as-NOP behavior.

## Handoff

Report encoding classes, vector names/count, command output, coverage rows changed, and any shared-helper defect with its smallest failing vector.
