# 270 — FPU Compare, Conversion, and Immediate Forms

**Status:** blocked
**Phase:** 2
**Dependencies:** 265
**Estimate:** 2 days

## Goal

Complete FPv4-SP compare, integer conversion, fixed-point conversion, and immediate/register utility instructions using the deterministic binary32 helpers.

## Execution Budget

One cheaper-model agent for two working days; one conversion file, one test binary, fixtures, and reuse of ticket 265's frozen integer helpers.

## Required Reading

- `src/cpu/armv7m/{fpu_transfer.c,fpu_arith.c,fpu_softfloat.h,fpu_convert.c}` after ticket 265
- `docs/migration-evidence.md` entry `E-CPU-0004`
- `docs/testing-strategy.md`
- `tests/fixtures/cpu/vector-format.md`

## Current Baseline

No compare or conversion decoder exists. `thumb32.c` can copy FPSCR.NZCV into xPSR through VMRS; ticket 260 owns that transfer and ticket 265 owns arithmetic rounding primitives.

## Allowed Files

- `src/cpu/armv7m/fpu_convert.c`
- `tests/unit/test_cpu_fpu_convert.c`
- `tests/fixtures/cpu/fpu-convert/**`
- assigned rows in `tests/fixtures/cpu/coverage.tsv`

## Frozen Interfaces

Use 260's access/FPSCR helpers and the integer-only classify/round/pack interface frozen by 265. Compare writes FPSCR.NZCV, with APSR updated only by the existing VMRS-to-PC form.

## Evidence Inputs

`E-CPU-0004` is mandatory for VCMP/VCMPE, VCVT integer/single/fixed forms, VCVTR, VMOV immediate, and utility instructions. This ticket is **blocked** if the relevant NaN, invalid, saturation, or rounding rules are absent.

## Implementation

Implement compare with register and +0.0, quiet/signaling NaN distinction, signed/unsigned 32-bit conversions, round-to-current and round-toward-zero variants, supported fixed-point fractions, VMOV immediate expansion, and referenced utility encodings. Conversion overflow/NaN results and IOC follow the pinned rules exactly.

## Tests and Commands

```sh
make test TEST_FILTER=cpu_fpu_convert
make test TEST_FILTER=cpu_fpu_arith
make test TEST_FILTER=cpu_fpu_transfer
```

`test_cpu_fpu_convert` must print passes for ordered less/equal/greater, unordered quiet/signaling NaNs, ±0 equality, integer min/max and one-past bounds, negative-to-unsigned, every rounding mode, half-way values, legal fixed-fraction endpoints, every VMOV immediate class, and reserved encodings. `test_cpu_fpu_arith` and `test_cpu_fpu_transfer` remain passing. Maximum two steps.

## Acceptance

Raw destination/FPSCR/xPSR values match exact vectors; no implementation-defined casts or host floating behavior occur; invalid/reserved cases are explicit; conversion coverage is marked verified only after 265's helper contract passes.

## Forbidden Scope

No double precision, extended exception frames, arbitrary fixed widths not in FPv4-SP, host math/conversion calls, public/shared-header edits, or guessing result values for invalid conversions.

## Handoff

Report compare/conversion matrix, exact raw results/flags, commands, dependency on 265 helpers, and unsupported forms.
