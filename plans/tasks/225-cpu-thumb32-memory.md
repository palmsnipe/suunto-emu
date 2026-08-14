# 225 — Thumb-32 Memory, Multiple, Exclusives, and Barriers

**Status:** done
**Phase:** 2
**Dependencies:** 190, 210
**Estimate:** 2–3 days

## Goal

Complete Thumb-32 load/store addressing, multiple-register transfers, exclusive monitor operations, table branches, and architectural barriers.

## Execution Budget

One cheaper-model agent for at most three working days; one memory implementation file, two test binaries, fixture rows, and no global atomic/cache model.

## Required Reading

- `src/cpu/armv7m/{armv7m_internal.h,thumb32_memory.c}` after ticket 190
- `include/semu/bus.h`, `src/core/bus.c`
- `tests/unit/test_cpu.c` current STMDB/indexed-load vectors
- `docs/{execution-model.md,migration-evidence.md,testing-strategy.md}` and `E-CPU-0001/0002`

## Current Baseline

`thumb32.c` currently contains `bit_count`, `store_multiple_decrement`, `wide_transfer`, and `indexed_transfer`; `armv7m_exec32` recognizes DMB/DSB/ISB/CLREX as functional no-ops. Tests cover `E92D 41F8`, one indexed word load, and no exclusive/barrier state.

## Allowed Files

- `src/cpu/armv7m/thumb32_memory.c`
- `tests/unit/test_cpu_thumb32_memory.c`
- `tests/unit/test_cpu_renode_regressions.c`
- `tests/fixtures/cpu/thumb32-memory/**`
- assigned rows in `tests/fixtures/cpu/coverage.tsv`

## Frozen Interfaces

Use 190's `armv7m_exec32_memory`, bus/fault, exclusive-state, SP, and branch helpers. Barriers affect only deterministic guest ordering and exclusive state; they must not call host thread synchronization APIs.

## Evidence Inputs

`E-CPU-0001` and `E-CPU-0002` are mandatory. This ticket is **blocked** without exact reference sections for addressing/writeback, LDM/STM/PUSH/POP, LDREX/STREX/CLREX, TBB/TBH, and DMB/DSB/ISB.

## Implementation

Cover immediate, register-offset, pre/post-indexed, literal, signed, and unprivileged byte/halfword/word transfers required by ARMv7E-M; LDM/STM variants; PUSH.W/POP.W; LDREX/STREX byte/halfword/word; CLREX; TBB/TBH; DMB/DSB/ISB. Clear the local exclusive monitor on reset, CLREX, exception entry, and conflicting local stores as specified. Enforce register-list and writeback restrictions.

## Tests and Commands

```sh
make test TEST_FILTER=cpu_thumb32_memory
make test TEST_FILTER=cpu_renode_regressions
make test TEST_FILTER=cpu_thumb16_memory
```

`test_cpu_renode_regressions` must visibly pass exact bytes `2d e9 f8 41` with SP reduced by 28 and ordered words r3–r8,lr. `test_cpu_thumb32_memory` must pass all widths/address modes, literal/PC alignment, exclusive success/failure/reset/exception invalidation, table branches, barriers, mapped bounds, and precise refused accesses; `test_cpu_thumb16_memory` remains passing. Maximum eight steps.

## Acceptance

Memory/register/writeback results match pinned pseudocode; both positive and refusal vectors pass; the STMDB regression executes natively; barriers are deterministic; no memory outside expected ranges changes.

## Forbidden Scope

No DMA/global multicore monitor, host atomics, cache model, MPU, DSP, FPU, exception arbitration, public/shared-header edits, compatibility hook, or all-address fallback.

## Handoff

Report transfer/list/exclusive coverage, exact regression state, barrier policy, commands/results, and any bus behavior escalated to the integrator.
