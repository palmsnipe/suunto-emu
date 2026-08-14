# 210 — Thumb-16 Memory, Stack, and Multiple Transfers

**Status:** done
**Phase:** 2
**Dependencies:** 190
**Estimate:** 1–2 days

## Goal

Complete Thumb-16 byte/halfword/word loads and stores, literal/SP addressing, PUSH/POP, LDM/STM, and address generation with fault-path vectors.

## Execution Budget

One cheaper-model agent for at most two working days; one memory implementation file, one test binary, fixture rows, and at most four-instruction vectors.

## Required Reading

- `src/cpu/armv7m/{armv7m_internal.h,thumb16_memory.c}` after ticket 190
- `include/semu/bus.h`, `src/core/bus.c`
- `tests/support/{cpu_fixture.c,cpu_fixture.h}` and `tests/fixtures/cpu/vector-format.md`
- `docs/{execution-model.md,migration-evidence.md,testing-strategy.md}`

## Current Baseline

`thumb16_memory.c` currently contains `transfer`, `immediate_transfer`, `sp_or_literal`, `push`, `pop`, `multiple`, and `armv7m_exec16_memory`; `thumb16.c` contains ADR/add-SP behavior. Existing coverage checks only aligned word STR/LDR and the STMDB Thumb-32 regression.

## Allowed Files

- `src/cpu/armv7m/thumb16_memory.c`
- `tests/unit/test_cpu_thumb16_memory.c`
- `tests/fixtures/cpu/thumb16-memory/**`
- assigned rows in `tests/fixtures/cpu/coverage.tsv`

## Frozen Interfaces

Use 190's bus, SP, branch-exchange, fault-address, and vector-fixture contracts. Follow `E-CPU-0002` for partial transfer/writeback behavior on a fault; do not invent blanket atomicity.

## Evidence Inputs

`E-CPU-0002` is mandatory and must identify addressing, alignment, sign-extension, register-list, and exception behavior for the assigned instructions. This ticket is **blocked** when those sections are absent.

## Implementation

Cover register and immediate STR/LDR/STRB/LDRB/STRH/LDRH, LDRSB/LDRSH, literal LDR, SP-relative forms, ADR, ADD/SUB SP, PUSH/POP including PC/LR, and LDMIA/STMIA writeback. Validate empty/illegal lists, PC/SP restrictions, address wrap, unmapped access, and load-to-PC Thumb state.

## Tests and Commands

```sh
make test TEST_FILTER=cpu_thumb16_memory
make test TEST_FILTER=cpu_contract
```

`test_cpu_thumb16_memory` must print passes for every width/addressing class, negative sign-extension, lowest/highest aligned mapped address, exact stack layout, legal writeback, and load-to-PC. It must also pass unaligned/unmapped, address-overflow, empty-list, illegal base/list, and even-PC refusal vectors; each run is bounded to four steps. `test_cpu_contract` remains passing.

## Acceptance

Exact memory/register/SP/PC/fault-address expectations pass; source bytes outside touched ranges remain unchanged; writeback/fault behavior matches the pinned pseudocode; coverage records only verified forms.

## Forbidden Scope

No Thumb-32 transfers, exclusives, MPU, exception stacking, bus/core edits, permissive unaligned emulation, public/shared-header changes, or device-specific memory behavior.

## Handoff

Report addressing/list coverage, precise-fault cases, exact commands/results, coverage changes, and any bus limitation preventing the referenced behavior.
