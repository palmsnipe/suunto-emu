# 205 — Thumb-16 Control, High Registers, and Bit Utilities

**Status:** done
**Phase:** 2
**Dependencies:** 190
**Estimate:** 1–2 days

## Goal

Complete Thumb-16 conditional/unconditional control flow, high-register operations, IT/hints, CBZ/CBNZ, extend/reverse, and breakpoint/undefined behavior.

## Execution Budget

One cheaper-model agent for at most two working days; one control implementation file, one test binary, fixture rows, and no exception-mechanics work.

## Required Reading

- `src/cpu/armv7m/{armv7m_internal.h,thumb16_control.c,cpu.c}` after ticket 190
- `tests/support/{cpu_fixture.c,cpu_fixture.h}` and `tests/fixtures/cpu/vector-format.md`
- `docs/{execution-model.md,migration-evidence.md,testing-strategy.md}`
- `E-CPU-0002` sections named in Evidence Inputs

## Current Baseline

After 190, baseline behavior originates from `thumb16.c` functions `special_data`, `miscellaneous`, `branches`, and control routing in `armv7m_exec16`. Existing tests cover one conditional branch, one unconditional branch, WFI, BKPT, and UDF. Implementation target is `armv7m_exec16_control` in `thumb16_control.c`.

## Allowed Files

- `src/cpu/armv7m/thumb16_control.c`
- `tests/unit/test_cpu_thumb16_control.c`
- `tests/fixtures/cpu/thumb16-control/**`
- assigned rows in `tests/fixtures/cpu/coverage.tsv`

## Frozen Interfaces

Use 190's PC operand, branch-exchange, IT-state, condition, event, and stop helpers unchanged. PC in vector expectations is the architectural aligned address stored by this emulator; LR retains the Thumb bit where required.

## Evidence Inputs

`E-CPU-0002` is mandatory and must pin sections for high-register ADD/CMP/MOV, BX/BLX, B/Bcc, CBZ/CBNZ, IT, hints, REV family, SXTB/SXTH/UXTB/UXTH, BKPT, UDF, and CPS. This ticket is **blocked** if those sections are absent.

## Implementation

Implement high-register and PC/SP restrictions, positive and negative branch displacements, BLX-register LR semantics, all IT mask lengths and condition inversion, NOP/YIELD/WFE/WFI/SEV hints, CPSIE/CPSID privilege checks, REV/REV16/REVSH, extensions, BKPT halt, and UDF unsupported stop. Invalid IT nesting and reserved condition `0xf` fail before mutation.

## Tests and Commands

```sh
make test TEST_FILTER=cpu_thumb16_control
make test TEST_FILTER=cpu_contract
```

`test_cpu_thumb16_control` must print passes for taken/not-taken conditions, forward/backward bounds, PC/LR exact values, every IT block length, SEV→WFE consumption, WFI deadlock, BKPT halt, even BX target refusal, and reserved encodings. Programs use a maximum of eight instructions or 100 virtual ticks; `test_cpu_contract` remains passing.

## Acceptance

Control-flow vectors match exact PC/LR/xPSR/stop state; skipped IT instructions cause no side effects; WFI/WFE never consult host time; invalid encodings and targets fail closed.

## Forbidden Scope

No SVC exception mechanics, SysTick, NVIC priority, load/store, Thumb-32 branch, public/shared-header edits, or treating unsupported hints as NOP.

## Handoff

Report the branch/IT matrix, sleep/event observations, commands/results, unsupported list, and coverage updates.
