# 260 — FPU Access Control, Registers, and Transfers

**Status:** blocked
**Phase:** 2
**Dependencies:** 190, 220, 240, 245
**Estimate:** 1–2 days

## Goal

Establish deterministic FPv4-SP access control, S-register/FPSCR state, and core/register/memory transfer instructions before arithmetic work.

## Execution Budget

One cheaper-model agent for at most two working days; one transfer file, one test binary, fixtures, and bit-copy operations only.

## Required Reading

- `include/semu/cpu.h` and `src/cpu/armv7m/{armv7m_internal.h,scb.c,fpu_transfer.c}` after dependencies
- `tests/unit/test_cpu.c` current FPSCR vector
- `docs/{migration-evidence.md,testing-strategy.md}`
- `E-CPU-0003/0004` CPACR, FP system-register, transfer, and NOCP sections

## Current Baseline

`semu_cpu_state` already stores `fpscr` and `s[32]`; `thumb32.c` handles only VMSR FPSCR and VMRS FPSCR, while `test_fpscr_transfer` checks a narrow round trip. CPACR, FPCCR/FPCAR/FPDSCR, NOCP faults, VMOV scalar/pair, VLDR/VSTR, and VLDM/VSTM are absent.

## Allowed Files

- `src/cpu/armv7m/fpu_transfer.c`
- `tests/unit/test_cpu_fpu_transfer.c`
- `tests/fixtures/cpu/fpu-transfer/**`
- assigned rows in `tests/fixtures/cpu/coverage.tsv`

## Frozen Interfaces

Use `armv7m_exec32_fpu`, SCB CPACR/fault hooks, bus helpers, and CPU state from 190/245. S-register values are raw IEEE-754 binary32 bit patterns; transfers never convert through host `float`.

## Evidence Inputs

`E-CPU-0004` is mandatory and must pin CPACR/FP system registers, NOCP behavior, VMRS/VMSR, VMOV, VLDR/VSTR, and VLDM/VSTM sections. `E-SAP-0008` is context only, not an opcode authority. This ticket is **blocked** without `E-CPU-0004`.

## Implementation

Implement CP10/CP11 access checks, reset values for FPSCR/FPCCR/FPCAR/FPDSCR, privileged system-register access, VMRS/VMSR, bit-exact ARM↔S and ARM-pair↔S-pair VMOV, VLDR/VSTR, and bounded VLDM/VSTM with exact addressing/writeback. Disabled access requests UsageFault.NOCP through 245.

## Tests and Commands

```sh
make test TEST_FILTER=cpu_fpu_transfer
make test TEST_FILTER=cpu_faults
```

`test_cpu_fpu_transfer` must print passes for reset/access enable, disabled/partial CPACR NOCP, all transfer directions with NaN/Inf/signed-zero payloads unchanged, first/last S register, load/store bounds, legal writeback, bad list/register refusal, and FPSCR NZCV transfer. `test_cpu_faults` remains passing. Maximum eight steps.

## Acceptance

All transferred bits and fault/status fields match exactly; no host floating operation occurs; memory refusal and writeback follow pinned pseudocode; unknown FP system registers fail closed.

## Forbidden Scope

No FP arithmetic/conversion/compare, lazy/extended stacking, double precision, host `float`/`fenv`, Apollo4 peripherals, public/shared-header changes, or CPACR permissive default.

## Handoff

Report supported transfer encodings/registers, NOCP cases, exact commands/results, raw-bit vectors, and frozen helpers for tickets 265–275.
