# 265 — FPU Single-Precision Arithmetic

**Status:** ready
**Phase:** 2
**Dependencies:** 260
**Estimate:** 2–3 days

## Goal

Implement deterministic FPv4-SP arithmetic and FPSCR exception/rounding behavior without relying on uncontrolled host floating-point state.

## Execution Budget

One cheaper-model agent for at most three working days; decoder plus project-local binary32 helper, one test binary, fixtures, and no host floating point.

## Required Reading

- `src/cpu/armv7m/{armv7m_internal.h,fpu_transfer.c,fpu_arith.c,fpu_softfloat.c,fpu_softfloat.h}` after ticket 260
- `docs/migration-evidence.md` entries `E-CPU-0004` and `E-CPU-0008`
- `docs/{contributing.md,testing-strategy.md}`
- `tests/fixtures/cpu/vector-format.md`

## Current Baseline

There is no FP arithmetic implementation or test beyond FPSCR transfer. S registers are raw `uint32_t` values. Ticket 260 freezes access checks and transfer/state helpers; target file is `fpu_arith.c` behind `armv7m_exec32_fpu`.

## Allowed Files

- `src/cpu/armv7m/armv7m_internal.h`, `src/cpu/armv7m/thumb32.c`, `src/cpu/armv7m/thumb32_fpu.c`
- `src/cpu/armv7m/fpu_arith.c`, `src/cpu/armv7m/fpu_softfloat.c`, `src/cpu/armv7m/fpu_softfloat.h`
- `tests/unit/test_cpu_fpu_arith.c`
- `tests/fixtures/cpu/fpu-arith/**`
- assigned rows in `tests/fixtures/cpu/coverage.tsv`

## Frozen Interfaces

Consume raw binary32 operands/results and FPSCR helpers from 260. The project-local integer implementation is authoritative. It must produce canonical results prescribed by `E-CPU-0004` and `E-CPU-0008`, preserve NaN payload/sign where required, and never alter the process floating environment. The private `armv7m_exec32_fpu` seam may route arithmetic encodings to `armv7m_fpu_arith`; no public header or shared decoder API may change.

## Evidence Inputs

`E-CPU-0004` and `E-CPU-0008` are mandatory. Together they pin VADD, VSUB, VMUL, VDIV, VMLA/VMLS/VNMLA/VNMLS/VNMUL, VABS/VNEG/VSQRT, rounding modes, default-NaN, flush-to-zero, and cumulative exception flags. This ticket is **blocked** for any family lacking pinned special-case rules.

## Implementation

Implement binary32 unpack/classify/normalize/round/pack using unsigned C99 integers; all four rounding modes; normal/subnormal/zero/infinity/NaN handling; DN/FZ behavior; IOC/DZC/OFC/UFC/IXC cumulative flags; and the listed scalar operations. Use 64-bit integer intermediates with explicit bounds; add helper-level vectors before decoder vectors.

## Tests and Commands

```sh
make test TEST_FILTER=cpu_fpu_arith
make test TEST_FILTER=cpu_fpu_transfer
make sanitize TEST_FILTER=cpu_fpu_arith
```

`test_cpu_fpu_arith` must print exact hex-result passes for normal, tie, every rounding mode, signed zero, subnormal bounds, overflow, infinity, quiet/signaling NaN, divide by zero, invalid operations, square-root bounds, DN/FZ, and sticky flags. `test_cpu_fpu_transfer` remains passing. One opcode per vector, maximum two steps; the filtered sanitizer command exits 0.

## Acceptance

Every result/FPSCR value is bit-exact and repeats across compilers/optimization levels; sanitizer passes; no host `float`, `double`, math library, `fenv`, locale, or external softfloat dependency is used.

## Forbidden Scope

No conversions/compare, double precision, fused behavior unless FPv4-SP explicitly requires it, SIMD/NEON, host intrinsics/assembly, public/shared-header edits, or relaxing NaN/flag checks.

## Handoff

Report arithmetic coverage, raw special-case table, rounding/exception results, commands/sanitizer output, and any reference ambiguity left blocked.
