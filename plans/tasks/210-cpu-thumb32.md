# 210 — Thumb-32 Arithmetic, Branches, and Memory

**Status:** blocked
**Phase:** 2
**Dependencies:** 190

## Goal

Implement non-FPU 32-bit Thumb-2 arithmetic/DSP, branches, loads/stores, exclusives, barriers, and multiple-register operations.

## Allowed Files

`src/cpu/armv7m/thumb32_*.c`, `tests/unit/cpu_thumb32_*.c`, `tests/fixtures/cpu/thumb32/**`; no shared decoder/header edits.

## Frozen Interfaces

Consume 190 dispatch/state/bus helpers. Exclusive state belongs to CPU state. Barrier instructions are deterministic functional barriers, not host synchronization.

## Evidence Inputs

ARMv7E-M instruction semantics; `E-CPU-0001` requires native vectors for `MOV.W r0,sp` and `STMDB`.

## Implementation

Split by encoding family; validate unpredictable/reserved combinations; handle PC/SP restrictions, signed/unsigned saturation, multiply/DSP results, literal addressing, and all-or-nothing multi-access faults.

## Tests and Commands

`make test TEST_FILTER=thumb32`; `make test TEST_FILTER=renode_regressions`; `make test TEST_FILTER=exclusive`; `make check`.

## Acceptance

Required families pass success/fault vectors; `MOV.W r0,sp` and `STMDB` execute without hooks; exclusives and barriers are deterministic; undefined encodings stop unsupported.

## Forbidden Scope

No FPU, exception entry, SoC atomics, compatibility hooks, public-header edits, or decoding reserved space as a related opcode.

## Handoff

Report coverage and all Renode-regression vector results to ticket 220 integrator.

