# 220 — Thumb-32 Data Processing and Branches

**Status:** blocked
**Phase:** 2
**Dependencies:** 190, 200, 205
**Estimate:** 2–3 days

## Goal

Complete non-DSP Thumb-32 data-processing immediate/register forms, wide immediates, bitfield operations, and B.W/BL control flow.

## Execution Budget

One cheaper-model agent for at most three working days; one data/branch implementation file, one test binary, fixture rows, and no memory/DSP work.

## Required Reading

- `src/cpu/armv7m/{armv7m_internal.h,thumb32_data.c}` after ticket 190
- `tests/support/{cpu_fixture.c,cpu_fixture.h}` and `tests/fixtures/cpu/vector-format.md`
- `tests/unit/test_cpu.c` current `MOV.W` and ORR vectors
- `docs/migration-evidence.md` entries `E-CPU-0001` and `E-CPU-0002`

## Current Baseline

`thumb32.c` currently implements `move_register`, `move_immediate`, `rotate_right`, `expand_modified_immediate`, `shifted_register`, `branch`, and partial ORR/AND/ADD/SUB routing in `armv7m_exec32`. Tests cover exact `MOV.W r0,sp`, one ORR modified immediate, and no full branch matrix. Target after 190 is `armv7m_exec32_data` in `thumb32_data.c`.

## Allowed Files

- `src/cpu/armv7m/thumb32_data.c`
- `tests/unit/test_cpu_thumb32_data.c`
- `tests/fixtures/cpu/thumb32-data/**`
- assigned rows in `tests/fixtures/cpu/coverage.tsv`

## Frozen Interfaces

Use the shared portable shift/add/condition helpers and `armv7m_exec32_data` signature from 190. Preserve exact memory-order vectors `4f ea 0d 00` for `MOV.W r0,sp`. Branch state changes use the shared branch-exchange rules.

## Evidence Inputs

Both `E-CPU-0001` and `E-CPU-0002` are mandatory. This ticket is **blocked** if either is absent or if `E-CPU-0002` lacks the data-processing, immediate-expansion, bitfield, and branch pseudocode sections.

## Implementation

Implement logical/arithmetic register and modified-immediate forms, MOV/MVN, MOVW/MOVT, ADDW/SUBW, ADR.W, compare/test aliases, shift aliases, CLZ, BFI/BFC, SBFX/UBFX, B.W, and BL. Enforce S-bit flag behavior, bad-register restrictions, immediate-expansion validity, PC alignment, LR Thumb bit, and signed branch ranges.

## Tests and Commands

```sh
make test TEST_FILTER=cpu_thumb32_data
make test TEST_FILTER=cpu_renode_regressions
make test TEST_FILTER=cpu_thumb16_arith
```

The planned `test_cpu_renode_regressions` binary must print a pass for `EA4F 000D`. `test_cpu_thumb32_data` must cover every alias boundary, flag/no-flag pair, bitfield widths 1 and maximum, zero/negative branch limits, reserved immediates, bad PC/SP operands, and unchanged state on refusal. Runs are at most four steps.

## Acceptance

Assigned vectors produce exact registers/APSR/PC/LR; valid `MOV.W r0,sp` needs no hook; reserved encodings stop unsupported; portable integer helpers introduce no UB under `make sanitize TEST_FILTER=cpu_thumb32_data`.

## Forbidden Scope

No loads/stores, multiply/DSP/saturation, special-register moves, exceptions, FPU, compatibility hooks, decoder/public/shared-header edits, or broad alias matching.

## Handoff

Report encoding/alias coverage, branch bounds, `E-CPU-0001` result, exact commands, sanitizer result, and remaining unsupported data forms.
