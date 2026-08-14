# 190 — CPU Contract and Baseline Decomposition

**Status:** done
**Phase:** 2
**Dependencies:** 180
**Estimate:** 2–3 days

## Goal

Freeze the public CPU/IRQ inspection contract, extract a reusable CPU test fixture, and split the existing combined decoders into non-overlapping family files without expanding instruction behavior. This is the only Phase 2 ticket allowed to change shared CPU interfaces or dispatch ownership.

## Execution Budget

One cheaper-model agent for at most three working days; decomposition and contract tests only, no instruction-coverage expansion and no subdelegation.

## Required Reading

- `include/semu/{cpu,bus,scheduler,types}.h`
- `src/cpu/armv7m/{armv7m_internal.h,cpu.c,support.c,thumb16.c,thumb16_memory.c,thumb32.c}`
- `tests/unit/test_cpu.c`, `tests/support/{test.c,test.h}`
- `docs/{architecture.md,execution-model.md,migration-evidence.md,testing-strategy.md}`

## Current Baseline

- `cpu.c`: `sync_itstate`, `advance_itstate`, `instruction_is_32bit`, `pending_irq`, `finish_instruction`, CPU create/destroy/reset/step/state/IRQ/stop accessors, and `armv7m_set_itstate`.
- `support.c`: bus wrappers, unsupported stop, register/SP/flag helpers, `armv7m_add`, sign extension, conditions, exception return, branch exchange, and `armv7m_take_exception`.
- `thumb16.c`: `shift`, arithmetic/immediate/data processing, high-register operations, miscellaneous/control, branches, and `armv7m_exec16`.
- `thumb16_memory.c`: register/immediate/literal transfers, PUSH/POP, LDM/STM, and `armv7m_exec16_memory`.
- `thumb32.c`: register/immediate moves, modified immediates, shifted-register ADD/SUB, branches, wide/indexed transfers, STMDB, barriers/hints, partial MSR and FPSCR transfers, and `armv7m_exec32`.
- `tests/unit/test_cpu.c`: reset/arithmetic/branch, load/store, WFI, unsupported, SVC, level IRQ, MSR PSP, modified-immediate ORR, FPSCR transfer, indexed load, and both `E-CPU-0001` vectors.

## Allowed Files

- `include/semu/cpu.h`
- `src/cpu/armv7m/**`
- `tests/support/cpu_fixture.c`, `tests/support/cpu_fixture.h`
- `tests/unit/test_cpu.c`, `tests/unit/test_cpu_contract.c`
- `tests/fixtures/cpu/coverage.tsv`

## Frozen Interfaces

Preserve every existing `semu/cpu.h` function and `semu_cpu_state` field. Add exactly these operations: `void semu_cpu_set_irq_priority(semu_cpu *, unsigned, uint8_t)`, `void semu_cpu_signal_event(semu_cpu *)`, and `int semu_cpu_fault_address(const semu_cpu *, uint32_t *)`. Out-of-range IRQ setters are no-ops; fault-address returns zero when no precise address is recorded.

Freeze internal family entry points in `armv7m_internal.h`: `armv7m_exec16_arith`, `armv7m_exec16_control`, `armv7m_exec16_memory`, `armv7m_exec32_data`, `armv7m_exec32_memory`, `armv7m_exec32_dsp`, `armv7m_exec32_system`, and `armv7m_exec32_fpu`, all using the existing `(cpu, halfword[s], pc, error)` style and returning `semu_status`. Top-level `armv7m_exec16/32` owns mutually exclusive routing; a family validates reserved combinations. Architectural exceptions update CPU state and return `SEMU_OK`; emulator refusal returns non-OK with a stop reason.

`TEST_FILTER` is integration-owned and already frozen: it selects matching test binaries/cases/groups, runs all tests when empty, and exits 2 with `error: TEST_FILTER='<value>' matched no test source, case, or group` when non-empty and unmatched. This ticket tests but does not reimplement it.

## Evidence Inputs

`E-CPU-0001` and `E-CPU-0002` are required. This ticket is **blocked** if either is absent or `E-CPU-0002` lacks an exact reference revision. Decomposition must preserve all current vectors before downstream work.

## Implementation

Keep `cpu.c` for lifecycle/fetch/retirement, move exception mechanics out of `support.c`, keep shared deterministic integer helpers in files owned by this ticket, and reduce `thumb16.c`, `thumb32.c`, and `test_cpu.c` below 300 lines. Create named family files matching the frozen entry points; move existing behavior rather than duplicating it. The reusable fixture maps vector/RAM, loads exact bytes, applies initial state, runs a bounded step count, and compares complete expected state plus touched memory.

## Tests and Commands

```sh
make test TEST_FILTER=cpu_contract
make test TEST_FILTER=cpu
make test TEST_FILTER=does-not-exist; test $? -eq 2
make check
```

`test_cpu_contract` must print passing reset, fetch-boundary, 16/32 dispatch, unsupported, unmapped fault-address, IRQ-range, and deterministic-retirement cases. `TEST_FILTER=cpu` must include `test_cpu`, both current `E-CPU-0001` byte vectors, and all pre-split smoke tests with identical final state.

## Acceptance

Public additions compile and behave as specified; each opcode routes to one family; no valid baseline vector changes; invalid fetch/encoding fails closed; CPU-owned hand-written files are below 300 lines; the filter's match/no-match behavior is observable and deterministic.

## Forbidden Scope

No new instruction families, exception priorities, SysTick, FPU arithmetic, Apollo4 changes, permissive opcode fallback, compatibility hooks, firmware execution, or Makefile edits.

## Handoff

Report changed files, frozen public/internal declarations, dispatcher ownership table, exact commands/results, moved baseline functions, and interface gaps. Tickets 200–275 must not edit `cpu.h`, the shared fixture, or top-level dispatch after this handoff.
