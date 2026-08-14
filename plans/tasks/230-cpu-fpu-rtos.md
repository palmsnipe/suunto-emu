# 230 — FPU and Synthetic RTOS Phase 2 Gate

**Status:** blocked
**Phase:** 2
**Dependencies:** 220

## Goal

Complete single-precision VFP behavior and prove the interpreter with a deterministic RTOS-style synthetic guest.

## Allowed Files

`src/cpu/armv7m/fpu_*.c`, FPU decoder glue, `tests/unit/cpu_fpu_*`, `tests/integration/rtos_*`, `fixtures/synthetic/rtos/**`, Makefile test/source lists, `plans/index.tsv` status only.

## Frozen Interfaces

Use CPU/machine contracts unchanged. FPU state includes S registers, FPSCR, enable/access state, and architectural exception stacking fields. Guest fixture format is raw little-endian bytes with a provenance manifest and source/assembly listing.

## Evidence Inputs

ARMv7E-M FPv4-SP semantics; Phase 2 gates in the approved plan.

## Implementation

Implement required data processing, conversions, compare, register/memory transfers, enable faults, FPSCR flags/rounding, and basic/extended frame save/restore. Avoid host floating-point behavior where it violates architectural determinism.

## Tests and Commands

`make test TEST_FILTER=fpu`; `make test TEST_FILTER=rtos_guest`; `make test TEST_FILTER=determinism`; `make check`; `make sanitize`.

## Acceptance

The guest performs SysTick, SVC, PendSV context switching, nested IRQ, WFI wake, and FPU save/restore twice with identical state/checkpoints and no compatibility or unsupported-instruction stop.

## Forbidden Scope

No double-precision extension, cycle accuracy, JIT/Unicorn/Renode runtime dependency, host-specific assembly, firmware blobs, or golden weakening.

## Handoff

Publish the Phase 2 instruction coverage and gate transcript before Apollo4 work begins.

