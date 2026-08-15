# 610 — Versioned Input Recording and Replay

**Status:** done
**Phase:** 6
**Dependencies:** 600

## Goal

Record and replay semantic input at exact virtual-time boundaries with strict
profile and firmware identity validation.

## Execution Budget

One to two model-days; at most four replay/parser/test files.

## Required Reading

`include/semu/{input,machine,manifest,trace}.h`, `docs/profile-format.md`, and
the semantic input integration handoff from Phase 5.

## Current Baseline

Ticket 518 provides the bounded Phase 5 scheduling grammar and ticket 520 uses
it for golden interaction. There is no versioned recording format, exact
profile/firmware identity binding, or general CLI record/replay path.

## Allowed Files

`src/core/replay*.c`, `tests/unit/test_replay.c`, synthetic replay fixtures,
and `include/semu/trace.h` replay declarations reserved by ticket 600.

## Frozen Interfaces

Replay contains format version, exact profile/component hashes, ordered integer
virtual times, and semantic events. Duplicate times preserve file order.

## Evidence Inputs

No private data. Use only semantic inputs already frozen by Phase 5.

## Implementation

Implement strict parser/formatter, complete preflight validation, scheduled
injection, and bounded event count. Reject unknown kinds/codes and time reversal.

## Tests and Commands

`make test TEST_FILTER=replay` selects `test_replay` and exits 0 for round trip,
same-time order, bad hash/version/code, time reversal, and two-run equality.
Run `make check-lines && make check && make sanitize`.

## Acceptance

Two replays produce identical input traces/checkpoints; every invalid replay
fails before guest execution.

## Forbidden Scope

No SDL polling, wall time, snapshots, interactive editing, or best-effort load.

## Handoff

Report format version, bounds, identity fields, equality artifact, and CLI hook.
