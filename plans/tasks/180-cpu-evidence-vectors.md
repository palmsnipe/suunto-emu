# 180 — CPU Evidence Pins and Vector Contract

**Status:** ready
**Phase:** 2
**Dependencies:** 120
**Estimate:** 1 day

## Goal

Pin the authoritative ARMv7E-M, exception/NVIC, FPv4-SP, and RTOS ABI references before instruction work begins, and define one dependency-free instruction-vector schema. This ticket records evidence; it does not change CPU behavior.

## Execution Budget

One cheaper-model agent for one working day; documentation/fixture metadata only, no subdelegation and no more than four changed files.

## Required Reading

- `docs/migration-evidence.md`
- `docs/testing-strategy.md`
- `plans/task-template.md`
- `include/semu/cpu.h`
- `tests/unit/test_cpu.c`

## Current Baseline

`docs/migration-evidence.md` contains only CPU entry `E-CPU-0001`, covering the exact `MOV.W r0,sp` and `STMDB sp!,{r3-r8,lr}` Renode regressions. `tests/unit/test_cpu.c` embeds byte arrays and expected state directly; no reusable vector corpus exists.

## Allowed Files

- `docs/migration-evidence.md`
- `tests/fixtures/cpu/vector-format.md`
- `tests/fixtures/cpu/coverage.tsv`
- `plans/tasks/180-cpu-evidence-vectors.md` handoff notes only

## Frozen Interfaces

The vector schema is textual documentation for C99 test tables, not a runtime parser. Each row fixes: evidence ID and section, instruction bytes in memory order, initial PC/xPSR/registers/memory, maximum steps, expected status/stop reason, final PC/xPSR/registers/memory, and whether an exception is expected. Every unspecified register and byte remains unchanged.

## Evidence Inputs

- Existing `E-CPU-0001` is mandatory.
- Add `E-CPU-0002` for Arm ARM DDI 0403E.e, *ARMv7-M Architecture Reference Manual*, with revision, stable source location, and the instruction/decode/pseudocode sections used by tickets 200–230.
- Add `E-CPU-0003` for DDI 0403E.e exception/System Control Space sections plus Arm DUI 0553B, *Cortex-M4 Devices Generic User Guide*, chapters 2 and 4.
- Add `E-CPU-0004` for the FPv4-SP architectural reference used by tickets 260–275, pinned by document number/revision and relevant sections.
- Add `E-CPU-0005` for AAPCS32 IHI 0042J and one exact upstream `FreeRTOS-Kernel` ARM_CM4F port revision used only to shape the synthetic gate.
- If a named primary reference cannot be accessed or its exact revision cannot be established, mark the corresponding ledger entry absent and report the dependent tickets as **blocked**. Do not substitute memory, Renode behavior, or a secondary opcode table.

## Implementation

Record bibliographic metadata and symbolic/local source locations; do not commit reference PDFs. Define coverage statuses `missing`, `vector-only`, `implemented`, and `verified`. Seed coverage only for behavior already tested in `tests/unit/test_cpu.c`; do not mark an instruction family complete from a smoke test.

## Tests and Commands

Run the existing baseline command:

```sh
make test TEST_FILTER=cpu
```

The existing integration-owned filter must select `test_cpu` and exit 0 with `cpu tests:` and no failed check. Verify manually that every CPU ticket below names only `E-CPU-0001` through `E-CPU-0005` and that each new ledger row has an exact revision and validation use.

## Acceptance

The four new evidence IDs are present or explicitly reported unavailable; the vector contract defines byte order and unchanged-state rules; coverage does not overclaim the current subset; no emulator behavior or firmware bytes change.

## Forbidden Scope

No CPU/source/header/Makefile edits, copied manual content, generated opcode tables, instruction implementation, firmware-derived vectors, or assumptions based solely on another emulator.

## Handoff

Report the exact reference revisions/locations, created evidence IDs, schema path, baseline command/result, and any missing ID that keeps downstream tickets blocked.
