# 513 — Nema Backend and Panel Frame Publication

**Status:** done
**Phase:** 5
**Dependencies:** 404, 500, 502, 504, 506, 510, 511, 512

## Goal

Integrate Nema modules and publish only evidence-complete 240×240 RGB565 panel frames. This unlocks SDL presentation 515 and golden integration 520; input and visual tolerances remain deferred.

## Execution Budget

Two to three agent-days. Integrate parser/state/draw/completion into the frozen backend and publish only completed 240×240 RGB565 physical-panel frames.

## Required Reading

`include/semu/{display,frame}.h`, `src/display/surface.c`, `tests/unit/test_display.c`, all display-module handoffs, `native-panel-transport.md`, and `SapporoNemaP.cs:CommitObservedPanelRegion/PublishChangedFrame` as evidence references.

## Current Baseline

Surface clear/write increments generation per mutation and exposes borrowed pixels. No backend consumes command rings. Renode sometimes assembles two 57,600-byte halves and warns intermediate staging rows are not physical frames.

## Allowed Files

Only `src/display/{surface.c,nema_backend.c,nema_backend.h,panel.c,panel.h}`, `tests/unit/{test_display.c,test_nema_backend.c}`, and `tests/integration/test_panel_publication.c`.

## Frozen Interfaces

Implement the backend callback frozen by 298. Rendering changes an internal canonical RGB565LE surface; generation increments once per physical publication, not per draw. Panel assembly consumes explicit region events from 404 and publishes 115200 bytes only when `E-NEMA-PANEL-001` completion condition is met. Frame bytes are immutable during callback.

## Evidence Inputs

All dependency IDs plus `E-NEMA-PANEL-001` must prove region ordering, source target, physical completion, byte order, and when a frame is publishable. If only staging evidence exists, backend may render diagnostics but must publish zero physical frames.

## Implementation

Wire framing→state→texture/draw→completion with bounded diagnostics; retain dirty regions; separate diagnostic surface from physical panel assembly; make refusal atomic at list level and cancel events on reset.

## Tests and Commands

`make test TEST_FILTER=nema_backend` and `make test TEST_FILTER=panel_publication` each select only matching binaries and exit 0; completion-only NULL path, clear/list draw, refusal/no-partial frame, region order, half-frame-not-published, full 115200-byte publication, generation/callback lifetime, reset, and repeat hash pass under 4096 words/list and 64 events. `make sanitize TEST_FILTER=nema_backend` and `make check` exit 0.

## Acceptance

Only evidenced physical completion publishes; callbacks see exact dimensions/stride/size/generation; unsupported command stops with bounded diagnostic; two backend records match.

## Forbidden Scope

No SDL, button input, golden tolerance, staging-as-frame label, private pixels, unverified TSC6A, command acceptance fallback, or public-header edit.

## Handoff

Report backend lifecycle, publication condition, diagnostic versus physical outputs, synthetic hashes, evidence IDs, and 515/520 setup calls.
