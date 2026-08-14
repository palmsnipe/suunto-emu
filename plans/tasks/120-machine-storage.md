# 120 — Storage, Machine Shell, and Phase 1 Gate

**Status:** done
**Phase:** 1
**Dependencies:** 110

## Goal

Provide immutable-base sparse overlays, the ownership-root machine shell, synthetic microprogram loading, bounded run semantics, and Phase 1 integration.

## Execution Budget

Completed Phase 1 integration ticket; later machine features use new tickets.

## Required Reading

`include/semu/{machine,storage,frame,input,peripheral}.h`,
`src/{boards/machine,core/storage,frontends/cli}.c`,
`tests/unit/{test_machine,test_storage}.c`, and Phase 1 dependency handoffs.

## Current Baseline

Machine ownership, reset-time hash revalidation, bounded CPU execution, semantic
button injection, sparse immutable storage overlays, and deterministic repeated
reset are implemented. CPU, SoC, display and device completeness are explicitly
outside this completed foundation ticket.

## Allowed Files

`Makefile`, `include/semu/{machine,storage,frame,input,peripheral}.h`,
`src/boards/machine.c`, `src/core/storage.c`, `src/frontends/{cli,main_headless}.c`,
Phase 1 unit/integration tests, and integrator-owned index status.

## Frozen Interfaces

Freeze `semu_machine`, stop reasons from `docs/architecture.md`, frame/input contracts, and `OK/WAIT/REFUSE`. Run accepts instruction/time budgets. Storage validates the complete program/erase range before mutation and never owns a writable base file handle.

## Evidence Inputs

Phase 1 docs and tests from 100/110.

## Implementation

Create/destroy/reset safely across partial initialization; merge sparse overlay reads with immutable bytes; add bounded headless run plumbing and deterministic state summary. Keep CPU attachment opaque for ticket 190.

## Tests and Commands

`make test TEST_FILTER=storage` selects `test_storage`; `make test
TEST_FILTER=machine` and `make test TEST_FILTER=determinism` select
`test_machine`; `make check-lines`, `make check`, and `make sanitize` exit 0.

## Acceptance

Program/erase/refusal and repeated reset pass; source-file hash is unchanged; all stop reasons stringify; two synthetic runs have identical summaries; Phase 1 commands pass on supported compilers.

## Forbidden Scope

No CPU decoder, SoC/device model, firmware-specific profile, writable source images, SDL, snapshots, or unbounded run loop.

## Handoff

Publish frozen public headers and Phase 1 gate results before CPU tickets begin.
