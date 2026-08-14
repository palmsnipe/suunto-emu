# 200 — Thumb-16 Execution

**Status:** blocked
**Phase:** 2
**Dependencies:** 190

## Goal

Implement assigned 16-bit Thumb encodings required by ARMv7E-M firmware with vector-driven tests.

## Allowed Files

`src/cpu/armv7m/thumb16_*.c`, `tests/unit/cpu_thumb16_*.c`, `tests/fixtures/cpu/thumb16/**`; no shared decoder/header edits.

## Frozen Interfaces

Use decoder registration/dispatch, state helpers, and bus APIs from 190. Instruction handlers return the frozen execution result and cannot stop the machine directly.

## Evidence Inputs

ARMv7E-M instruction semantics and encodings; observed instruction gaps added as evidence entries by the integrator.

## Implementation

Split shifts/arithmetic, data processing, branches, loads/stores, stack/multiple, hints/system, and extend/reverse families into small files. Implement precise flags, alignment, PC operands, and fault-before-write behavior.

## Tests and Commands

`make test TEST_FILTER=thumb16`; `make test TEST_FILTER=cpu_fault_atomicity`; `make check`.

## Acceptance

Every assigned 16-bit encoding is either implemented with success/fault vectors or explicitly listed unsupported with architecture justification; invalid memory operations do not partially update state.

## Forbidden Scope

No Thumb-32 files, public interfaces, exceptions beyond returning a fault, cycle accuracy, firmware patches, or broad opcode-as-NOP fallback.

## Handoff

Report encoding coverage table, vector count, unsupported list, and exact commands.

