# 240 — Special Registers, Privilege, and Exception Frames

**Status:** done
**Phase:** 2
**Dependencies:** 200, 205, 210, 220, 225, 230
**Estimate:** 2–3 days

## Goal

Implement MRS/MSR, privilege/mask rules, architectural exception entry/return, SVC, and precise basic exception frames before adding NVIC arbitration.

## Execution Budget

One cheaper-model agent for at most three working days; two implementation files, two test binaries, fixtures, and basic frames only.

## Required Reading

- `src/cpu/armv7m/{armv7m_internal.h,cpu.c,exceptions.c,special.c}` after ticket 190
- `include/semu/{cpu,bus,types}.h`
- `docs/{execution-model.md,migration-evidence.md,testing-strategy.md}`
- `E-CPU-0002/0003` special-register, exception-entry, and EXC_RETURN sections

## Current Baseline

`support.c` currently owns `exception_return`, `armv7m_branch_exchange`, and `armv7m_take_exception`; `thumb32.c` partially handles MSR to MSP/PSP/PRIMASK/BASEPRI/BASEPRI_MAX/FAULTMASK/CONTROL; `thumb16.c` routes SVC and CPS. Current tests exercise only one SVC/basic return, one level IRQ, and one PSP write.

## Allowed Files

- `src/cpu/armv7m/exceptions.c`
- `src/cpu/armv7m/special.c`
- `tests/unit/test_cpu_exceptions.c`
- `tests/unit/test_cpu_special.c`
- `tests/fixtures/cpu/exceptions/**`, `tests/fixtures/cpu/special/**`
- assigned rows in `tests/fixtures/cpu/coverage.tsv`

## Frozen Interfaces

Consume the public CPU API and internal dispatch/frame helpers frozen by 190. Exception frames use guest little-endian memory; xPSR exception number is authoritative. This ticket may expose internal exception request/return helpers to 245/275 but may not change public headers.

## Evidence Inputs

`E-CPU-0002` and `E-CPU-0003` are mandatory. This ticket is **blocked** if special-register access rules, stack alignment, vector fetch, EXC_RETURN, and fault-on-entry/return sections are not pinned.

## Implementation

Implement MRS/MSR for APSR/xPSR subsets, MSP, PSP, PRIMASK, BASEPRI, BASEPRI_MAX, FAULTMASK, CONTROL; privilege restrictions and ISB-visible CONTROL effects; SVC exception request; MSP/PSP selection; 8-byte alignment word; basic frame stack/unstack; valid EXC_RETURN tokens; IT-state preservation; handler/thread transitions; and exception-entry exclusive-monitor clearing. Convert invalid vectors/frames/tokens into the referenced architectural fault request rather than emulator hooks.

## Tests and Commands

```sh
make test TEST_FILTER=cpu_special
make test TEST_FILTER=cpu_exceptions
make test TEST_FILTER=cpu_thumb32_memory
```

`test_cpu_special` must print passes for privileged/unprivileged reads/writes and every mask register. `test_cpu_exceptions` must print passes for MSP/PSP thread/handler selection, aligned/misaligned SP, exact eight-word frames, SVC immediate independence, IT restoration, bad vectors, stack/unstack refusal, and every accepted/rejected EXC_RETURN value. `test_cpu_thumb32_memory` remains passing. Each program has a 32-instruction/1,000-tick bound.

## Acceptance

Exact frames and restored state pass; privilege violations follow pinned behavior; exception faults do not become compatibility stops; all memory failures report the precise fault address when architecturally available.

## Forbidden Scope

No NVIC priority arbitration/nesting, SysTick, tail chaining/late arrival, extended FPU frame, Apollo4 SCS behavior, compatibility layers, public-header edits, or broad invalid-token acceptance.

## Handoff

Report frame layout, supported special registers, fault requests emitted, exact commands/results, and internal helpers frozen for 245 and 275.
