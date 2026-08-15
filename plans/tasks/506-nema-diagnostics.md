# 506 — Nema Refusal Diagnostics and Completion Events

**Status:** done
**Phase:** 5
**Dependencies:** 298, 490, 500

## Goal

Implement bounded deterministic refusal records and evidence-backed completion/IRQ events. This supplies backend diagnostics to 513; unsupported command acceptance and rasterization remain deferred.

## Execution Budget

One to two agent-days. Produce bounded deterministic command diagnostics and evidence-backed completion/IRQ events without accepting unsupported draws.

## Required Reading

`include/semu/{display,log}.h`, `SapporoNemaP.cs:LogRefusedDrawState/ScheduleCompletion`, Nema capture hooks, and corpus expected refusal rows.

## Current Baseline

No Nema diagnostic exists. The Renode model logs large state dumps and schedules command-list completion/IRQ; unbounded or host-path-bearing logs are unsuitable for deterministic comparisons.

## Allowed Files

Only `src/display/{nema_diagnostics.c,nema_diagnostics.h,nema_completion.c,nema_completion.h}` and `tests/unit/test_nema_diagnostics.c`.

## Frozen Interfaces

Diagnostic record contains category, list ID, source address/ordinal, register/value, bounded 16-word context, and no host path/pointer. Completion object accepts scheduler/IRQ sink and emits exactly one evidenced completion per accepted list; refused list emits none. Overflow policy is explicit truncate flag.

## Evidence Inputs

`E-NEMA-RING-001` proves bootstrap produces no IRQ; `E-NEMA-LISTS-001` must prove command-list ID/status/IRQ ordering and delay for accepted lists. Cite `ScheduleCompletion` but block timing/flags not present in authentic trace.

## Implementation

Normalize diagnostics, copy bounded context, make formatting stable, schedule/cancel completion deterministically, and separate “parsed but unsupported draw” refusal from malformed framing.

## Tests and Commands

`make test TEST_FILTER=nema_diagnostics` runs only its binary and exits 0; all refusal categories, context truncation, no host data, completion-once, no completion on refusal/bootstrap, reset cancellation, IRQ order, and repeated formatted record pass. Maximum context is 16 words and event budget 64. `make check` exits 0.

## Acceptance

Diagnostics are stable/bounded; unsupported input still refuses; completion flags/timing match evidence; reset/destroy leaves no queued callback.

## Forbidden Scope

No acceptance fallback, raw full command dump, host path/time/pointer, raster/state logic, invented IRQ, or trace-file writer.

## Handoff

Report categories/record schema, completion flags/timing, evidence hashes, tests, and attachment calls for 513/520.
