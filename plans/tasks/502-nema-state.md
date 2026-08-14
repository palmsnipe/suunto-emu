# 502 — Nema Register and Draw State

**Status:** blocked
**Phase:** 5
**Dependencies:** 490, 500

## Goal

Convert framed writes into complete immutable draw-state snapshots at evidenced DRAW_CMD boundaries. This unlocks raster/backend tickets 510/513; texture reads and pixel mutation remain deferred.

## Execution Budget

Two to three agent-days. Convert framed writes into immutable validated draw-operation snapshots without rasterizing.

## Required Reading

`nema_framing.h`, `native-nema-first-frame-lists.md`, `SapporoNemaP.cs:ExecuteObservedChild/ExecuteObservedDraw`, and the model's `ObservedGraphicsRegisters`/draw-dispatch constants.

## Current Baseline

No graphics state exists. The Renode model stores a dictionary of observed registers and dispatches many firmware/version-specific branches; some command lists inherit shader/texture state, and an unmatched tail register is documented.

## Allowed Files

Only `src/display/{nema_state.c,nema_state.h}` and `tests/unit/test_nema_state.c`.

## Frozen Interfaces

State consumes ordered framed records and emits a complete value snapshot only on evidenced DRAW_CMD. Snapshot includes target format/base/stride, clip, 16.16 quad, colors, blend ID, texture descriptors by value, and source list ID. Missing/unknown/conflicting state refuses before emission. State inheritance/reset rules are explicit by corpus case.

## Evidence Inputs

`E-NEMA-LISTS-001` must enumerate every accepted register and first-frame inheritance/tail rule. Cite `ExecuteObservedChild` and `ExecuteObservedDraw`; branch behavior not tied to a complete 2.22.60 list stays unsupported.

## Implementation

Use an explicit register enum/presence bits rather than a generic map; validate target/clip/geometry arithmetic; snapshot by value; reset list-local state only where evidence requires; emit no draw for preload lists.

## Tests and Commands

`make test TEST_FILTER=nema_state` runs only its binary and exits 0; preload/no-draw, clear draw, inherited A2LE state, multiple draws, missing target/clip/geometry, unknown register/draw, unmatched evidenced tail, reset, and two-run snapshots pass with at most 1024 writes/list. `make check` exits 0.

## Acceptance

Snapshots match corpus values/order; incomplete state emits nothing; unsupported register/draw refuses; no bus/pixel mutation or version heuristic occurs.

## Forbidden Scope

No command framing edit, texture dereference, raster/blend, generic register dictionary, guessed shader meaning, Race S/TSC6A branch, or completion IRQ.

## Handoff

Report register/presence table, inheritance rules, operation struct, evidence cases, refusals, and dependencies for 504/510/513.
