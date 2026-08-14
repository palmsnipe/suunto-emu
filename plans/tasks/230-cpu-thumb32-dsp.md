# 230 — Thumb-32 Multiply, DSP, Saturation, and Packing

**Status:** blocked
**Phase:** 2
**Dependencies:** 190, 200, 220
**Estimate:** 2–3 days

## Goal

Implement the ARMv7E-M integer multiply/DSP, divide, saturating, parallel arithmetic, packing, and byte-selection families independently from FPU work.

## Execution Budget

One cheaper-model agent for at most three working days; one DSP implementation file, one test binary, fixture rows, and integer-only C99 helpers.

## Required Reading

- `src/cpu/armv7m/{armv7m_internal.h,thumb32_dsp.c}` after ticket 190
- `tests/support/{cpu_fixture.c,cpu_fixture.h}` and `tests/fixtures/cpu/vector-format.md`
- `docs/migration-evidence.md` entry `E-CPU-0002`
- `docs/{contributing.md,testing-strategy.md}` portability and vector rules

## Current Baseline

No dedicated DSP file or DSP vectors exist. `thumb16.c` has only 32-bit-wrap MUL; `thumb32.c` has general ADD/SUB/logical helpers but no long multiply, divide, Q/GE flags, saturation, packing, or selection implementation. Target is `armv7m_exec32_dsp` in `thumb32_dsp.c`.

## Allowed Files

- `src/cpu/armv7m/thumb32_dsp.c`
- `tests/unit/test_cpu_thumb32_dsp.c`
- `tests/fixtures/cpu/thumb32-dsp/**`
- assigned rows in `tests/fixtures/cpu/coverage.tsv`

## Frozen Interfaces

Use 190's family signature and portable integer helpers. Store APSR.Q and APSR.GE only in their architectural xPSR bits. C99 code must avoid signed overflow, implementation-defined signed shifts, division by zero, and host-width assumptions.

## Evidence Inputs

`E-CPU-0002` is mandatory and must pin the multiply/divide, saturation, parallel add/subtract, PKH, extend-and-add, REV/RBIT/CLZ, and SEL sections. This ticket is **blocked** if any implemented family lacks pinned pseudocode.

## Implementation

Cover MUL/MLA/MLS, SMULL/UMULL/SMLAL/UMLAL, SDIV/UDIV, SSAT/USAT and 16-bit variants, QADD/QSUB/QDADD/QDSUB, signed/unsigned parallel byte/halfword arithmetic with GE flags, SEL, PKHBT/PKHTB, signed/unsigned extend-and-add with rotation, REV/RBIT/CLZ aliases, and firmware-observed dual multiply forms only when their exact reference section is pinned.

## Tests and Commands

```sh
make test TEST_FILTER=cpu_thumb32_dsp
make test TEST_FILTER=cpu_thumb32_data
```

`test_cpu_thumb32_dsp` must print passes for min/max/zero, 64-bit carry, accumulate wrap, signed and unsigned divide, divide-by-zero architectural result, saturation with sticky Q, each GE lane pattern, every legal rotation, overlapping destinations, and reserved register combinations. Each vector is at most two steps; `test_cpu_thumb32_data` remains passing.

## Acceptance

Results and Q/GE flags exactly match vectors on macOS/Linux and sanitizers; every routed reserved form stops unsupported without mutation; only reference-backed families become verified.

## Forbidden Scope

No FPU/SIMD/NEON, double precision, host intrinsics/assembly, cycle timing, public/shared-header changes, or inferred opcodes from neighboring encodings.

## Handoff

Report exact families implemented, Q/GE coverage, sanitizer command/results, unsupported list, and coverage changes.
