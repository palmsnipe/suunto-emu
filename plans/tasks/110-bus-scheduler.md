# 110 — Memory Bus and Deterministic Scheduler

**Status:** done
**Phase:** 1
**Dependencies:** 100

## Goal

Implement fail-closed memory regions and a stable integer-time event scheduler behind frozen core interfaces.

## Execution Budget

Completed Phase 1 ticket; extensions require new focused tickets.

## Required Reading

`include/semu/{bus,scheduler}.h`, `src/core/{bus,scheduler}.c`,
`tests/unit/{test_core,test_machine}.c`, and `docs/execution-model.md`.

## Current Baseline

Exact RAM/ROM/device regions, widths 1/2/4, overlap/bounds refusal, reset
callbacks, stable deadline/insertion ordering, cancellation, and WFI event
advancement are implemented. `src/core/bus.c` is above 300 lines and must be
split before material expansion.

## Allowed Files

`include/semu/{bus,scheduler}.h`, `src/core/{bus,scheduler}.c`,
`tests/unit/{test_core,test_machine}.c`, and integration-owned Makefile changes.

## Frozen Interfaces

Region callbacks receive context, address offset, width, value/result, and access origin. Supported widths are 1, 2, and 4 bytes. Scheduler keys are unsigned deadline plus monotonic insertion sequence; event callbacks cannot observe host time.

## Evidence Inputs

`docs/architecture.md` and `docs/execution-model.md`.

## Implementation

Reject zero/overflowing/overlapping regions; implement little-endian checked reads/writes and reset traversal. Support scheduling, cancellation by stable handle, next-deadline query, and FIFO draining of equal deadlines without recursion.

## Tests and Commands

`make test TEST_FILTER=core` selects `test_core`; `make test
TEST_FILTER=determinism` selects `test_machine`; `make check-lines`; and `make
check` all exit 0. `test_core` must report scheduler and bus cases passing.

## Acceptance

All widths, region edges, overlap/overflow, unmapped accesses, cancellation, same-time insertion, callback insertion, and repeated-run ordering tests pass with bounded execution.

## Forbidden Scope

No global read-as-zero, partial cross-region access, host threads/timers, CPU policy, devices, or platform-specific atomics.

## Handoff

Report callback contracts, ordering vectors, tests, and any integrator-owned Makefile conflict.
