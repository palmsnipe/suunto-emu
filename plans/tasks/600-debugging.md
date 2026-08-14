# 600 — Tracing, Reports, Replay, and Snapshots

**Status:** blocked
**Phase:** 6
**Dependencies:** 520

## Goal

Add bounded diagnostics and deterministic reproduction artifacts without changing guest-visible behavior.

## Allowed Files

`include/semu/trace.h`, `src/core/{trace,report,replay,snapshot}*`, CLI trace/replay/snapshot plumbing, `tests/unit/{trace,report,replay,snapshot}*`, Makefile source/test lists.

## Frozen Interfaces

Trace events have stable numeric kind, virtual time, monotonic sequence, and kind-specific normalized fields. Configure bounded instruction/MMIO/device/frame history with explicit stop-or-truncate overflow policy. Snapshot format includes magic/version/profile+firmware hashes and rejects mismatch atomically.

## Evidence Inputs

`docs/execution-model.md`, all existing deterministic checkpoint/transcript formats, first-release runs from 520.

## Implementation

Capture instruction/MMIO and unknown transactions; format CPU/fault/compat reports; record semantic inputs and frames; serialize machine-owned deterministic state while excluding callbacks, host paths, and open handles.

## Tests and Commands

`make test TEST_FILTER=trace`; `make test TEST_FILTER=replay`; `make test TEST_FILTER=snapshot`; `make test TEST_FILTER=fault_report`; `make check`.

## Acceptance

Overflow policies, unknown-transaction context, replay equality, snapshot round trip, corrupt/version/hash mismatch refusal, and stable report tests pass; diagnostics do not alter final guest record.

## Forbidden Scope

No unbounded trace allocation, pointer/timestamp serialization, network debugger, cross-version best-effort snapshot load, silent truncation, or firmware content embedded in reports.

## Handoff

Report trace/snapshot format versions, bounds, normalized fields, and compatibility guarantees.

