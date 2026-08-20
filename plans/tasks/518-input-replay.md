# 518 — Deterministic Input Replay

**Status:** done
**Phase:** 5
**Dependencies:** 298, 420, 516

## Goal

Parse and schedule a bounded dependency-free semantic button replay format. This supplies repeatable middle/lower navigation to 520; JSON, SDL, and direct GPIO remain deferred.

## Execution Budget

One to two agent-days. Parse and schedule a bounded semantic input replay format shared by headless and SDL integration.

## Required Reading

`include/semu/{input,machine,scheduler}.h`, semantic-input handoff, native `buttons.jsonl` artifacts and `E-SAP-INPUT-REPLAY-001`, plus deterministic scheduling rules in `docs/execution-model.md`.

## Current Baseline

No replay parser/queue exists. Native evidence uses JSONL, but adding a JSON dependency is disallowed. CLI cannot inject events during its blocking run loop.

## Allowed Files

Only `src/frontends/{input_replay.c,input_replay.h}` and `tests/unit/test_input_replay.c`.

## Frozen Interfaces

Strict ASCII format is one line `TIME_NS button CODE VALUE`, with decimal/hex integer grammar matching manifests, unique nondecreasing time, codes `upper|middle|lower`, values `press|release`, `#` comments, maximum 1024 events. Parser returns line diagnostics; scheduler callback invokes a supplied semantic-event sink.

## Evidence Inputs

`E-SAP-INPUT-REPLAY-001` maps native middle/lower evidence to semantic event order/checkpoints; host timestamps are normalized to chosen virtual offsets and labeled synthetic scheduling. No raw JSONL is committed.

## Implementation

Parse into bounded owned array, validate order/state before scheduling, cancel all event IDs on reset/destroy, and emit stable event ordinal/time/result transcript. Sink refusal stops further replay.

## Tests and Commands

`make test TEST_FILTER=input_replay` runs only its binary and exits 0; valid three-button sequence, equal-time stable order, malformed/duplicate/out-of-order/overflow, impossible release, sink refusal, reset cancellation, 1024 boundary, and repeat transcript pass within 1 virtual second. `make check` exits 0.

## Acceptance

Replay is dependency-free, bounded, deterministic, and board-semantic; invalid file schedules nothing; reset/destroy leaves no events; native order is represented without host timestamps.

## Forbidden Scope

No JSON parser/dependency, direct SDL/GPIO, wall-clock sleep, command-line edit, arbitrary sensor events, hidden default replay, or private native file copy.

## Post-completion scope clarification

The deferred SDL/CLI wording describes the parser boundary. Ticket 520 now
consumes this format from both headless and SDL runs, while JSON, arbitrary
sensor events, host timestamps, and direct GPIO replay remain outside scope.

## Handoff

Report grammar/limits, normalization provenance, transcript schema, tests, and CLI/machine pump hooks for 520.
