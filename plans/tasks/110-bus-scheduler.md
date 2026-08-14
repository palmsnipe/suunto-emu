# 110 — Memory Bus and Deterministic Scheduler

**Status:** done
**Phase:** 1
**Dependencies:** 100

## Goal

Implement fail-closed memory regions and a stable integer-time event scheduler behind frozen core interfaces.

## Allowed Files

`include/semu/{bus,scheduler}.h`, `src/core/{bus,scheduler}*`, `tests/unit/{bus,scheduler}*`, plus Makefile source-list additions only.

## Frozen Interfaces

Region callbacks receive context, address offset, width, value/result, and access origin. Supported widths are 1, 2, and 4 bytes. Scheduler keys are unsigned deadline plus monotonic insertion sequence; event callbacks cannot observe host time.

## Evidence Inputs

`docs/architecture.md` and `docs/execution-model.md`.

## Implementation

Reject zero/overflowing/overlapping regions; implement little-endian checked reads/writes and reset traversal. Support scheduling, cancellation by stable handle, next-deadline query, and FIFO draining of equal deadlines without recursion.

## Tests and Commands

`make test TEST_FILTER=bus`; `make test TEST_FILTER=scheduler`; `make test TEST_FILTER=determinism`; `make check`.

## Acceptance

All widths, region edges, overlap/overflow, unmapped accesses, cancellation, same-time insertion, callback insertion, and repeated-run ordering tests pass with bounded execution.

## Forbidden Scope

No global read-as-zero, partial cross-region access, host threads/timers, CPU policy, devices, or platform-specific atomics.

## Handoff

Report callback contracts, ordering vectors, tests, and any integrator-owned Makefile conflict.
