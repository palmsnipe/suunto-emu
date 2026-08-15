# 600 — Bounded Trace Contract

**Status:** done
**Phase:** 6
**Dependencies:** 520

## Goal

Freeze normalized instruction, MMIO, device, compatibility, input, and frame
trace records with explicit capacity and overflow behavior.

## Execution Budget

One to two model-days; at most four trace source/header/test files.

## Required Reading

`AGENTS.md`, `docs/execution-model.md`, `include/semu/{log,machine}.h`,
`src/core/log.c`, and the Phase 5 integration handoff.

## Current Baseline

`semu_log_write` emits readable structured text, and CLI `--trace` redirects
that log. There is no stable typed event schema, sequence number, ring bound,
or overflow policy.

## Allowed Files

`include/semu/trace.h`, `src/core/{trace,trace_format}.c`,
`tests/unit/test_trace.c`.

## Frozen Interfaces

Each record contains schema version, kind, integer virtual time, monotonic
sequence, and normalized kind-specific fields. Capacity and stop-or-truncate
policy are explicit; host pointers, absolute paths, and wall time are excluded.

## Evidence Inputs

No private data is required. Field selection comes from the stable Phase 5
checkpoint/transcript handoff. If that handoff is absent, remain blocked.

## Implementation

Implement allocation, append, bounded retrieval, stable formatting, and both
overflow policies. Trace storage must not change guest-visible execution.

## Tests and Commands

`make test TEST_FILTER=trace` selects `test_trace` and exits 0; vectors cover
all kinds, same-time ordering, capacity edges, stop and truncate overflow, and
format stability. Then run `make check-lines && make check && make sanitize`.

## Acceptance

Two identical event sequences format byte-identically; overflow is never
silent; no record contains unstable host state.

## Forbidden Scope

No CLI wiring, snapshots, replay, unbounded allocation, or device behavior.

## Handoff

Report schema version, fields, capacity rules, tests, and integration hooks.
