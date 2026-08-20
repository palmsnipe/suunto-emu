# 615 — Versioned Machine Snapshots

**Status:** done
**Phase:** 6
**Dependencies:** 600,610

## Goal

Serialize and restore deterministic machine-owned state atomically using a
versioned, identity-pinned snapshot format.

## Execution Budget

Two to three model-days; split CPU/SoC/device serializers into separate files.

## Required Reading

Machine ownership in `docs/architecture.md`; all state-owning public headers;
ticket 600 schema and 610 identity handoffs.

## Current Baseline

No state serialization exists. Machine internals contain CPU, scheduler, RAM,
SoC, devices, storage overlays, layers, and stop state.

## Allowed Files

`src/core/snapshot*.c`, `tests/unit/test_snapshot.c`, synthetic snapshot fixtures,
and serializer callbacks explicitly reserved by prior integration handoffs.

## Frozen Interfaces

Snapshot includes magic/version/profile+component hashes and lengths. Restore
validates the entire image before mutating a temporary machine, then swaps on
success. Callbacks, streams, paths, and host pointers are excluded.

## Evidence Inputs

No authentic bytes. Complete state ownership inventory from Phase 5/6 is
required; missing serializer callbacks block this ticket.

## Implementation

Use explicit little-endian fields and checked sizes. Serialize pending event
meaning, not callback pointers. Preserve sparse overlays and layer hit counts.

## Tests and Commands

`make test TEST_FILTER=snapshot` selects `test_snapshot` and exits 0 for round
trip, pending equal-time events, corrupt/truncated/overflow input, wrong hashes,
unsupported version, and unchanged target after refusal. Run
`make check-lines && make check && make sanitize`.

## Acceptance

A continued synthetic run matches an uninterrupted run byte-for-byte; all bad
inputs fail atomically.

## Forbidden Scope

No cross-version best effort, pointer serialization, compression dependency,
private firmware bytes, or partial restore.

## Handoff

Report format, state inventory, size bounds, equality hashes, and exclusions.

## Completed Maintenance Notes

The implementation preserves the atomic refusal contract with component-local
candidate state and a machine snapshot rollback boundary; it does not swap a
temporary machine object. Scheduler entries are identity-pinned to their CPU,
Apollo4, Sapporo, or NEMA owner in both directions. Save refuses unsupported
or unowned entries and detached owner events, while restore validates those
links before committing the restored scheduler queue.
