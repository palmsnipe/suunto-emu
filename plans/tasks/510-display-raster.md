# 510 — Integer Raster Primitives and Clipping

**Status:** blocked
**Phase:** 5
**Dependencies:** 298, 490, 502

## Goal

Implement checked integer RGB565 clear/rectangle/triangle primitives and clipping. This unlocks blend/sampling/backend tickets 511–513; texture, alpha, and publication remain deferred.

## Execution Budget

Two to three agent-days. Implement deterministic RGB565 clear/rectangle/triangle coverage and clipping over a checked surface; no texture/blend.

## Required Reading

`src/display/surface.c`, `tests/unit/test_display.c`, first-frame clear/geometry cases in the Nema corpus, and `SapporoNemaP.cs:ExecuteSolidTriangle/ExecuteClear` for evidenced integer rules only.

## Current Baseline

`semu_surface` can clear and copy rectangular RGB565 patches, incrementing generation on every call. It has no pixel accessor, clipping, fixed-point geometry, triangle edge convention, atomic pending writes, or separation between mutation and publication.

## Allowed Files

Only `src/display/{raster.c,raster.h,raster_triangle.c,raster_clip.c}` and `tests/unit/test_raster.c`.

## Frozen Interfaces

Raster target is `{pixels,width,height,stride,RGB565LE}` borrowed for one call. Operations are clear, clipped rectangle, and 16.16 fixed-point triangle with explicit top-left edge rule selected from evidence. Validate target/clip/geometry/stride before writes; return dirty bounds or refusal.

## Evidence Inputs

`E-NEMA-LISTS-001` must prove first-frame target, clip, quad/triangle coordinates, clear color, and edge/coverage expectation. If bit-identical edge rule is not established, support only axis-aligned full-coverage cases and refuse partial-edge triangles.

## Implementation

Use checked integer math and row bounds; stage operation metadata before mutation; split clip and triangle helpers; never increment frame generation or publish here.

## Tests and Commands

`make test TEST_FILTER=raster` runs only `test_raster` and exits 0; full clear/rect, clip empty/partial, winding, axis-aligned triangle, overflow/extreme coordinates, invalid stride/target, unverified partial edge refusal, guard bytes, and repeat hash pass under 240×240. `make sanitize TEST_FILTER=raster` and `make check` exit 0.

## Acceptance

Supported synthetic pixels/hashes match corpus; no out-of-bounds/partial mutation on refusal; integer results match Clang/GCC; unsupported edge coverage is explicit.

## Forbidden Scope

No texture sampling, alpha/blend, TSC6A, SDL, publication/generation, floating point, anti-aliasing guess, or `surface.c` edit.

## Handoff

Report primitive/edge/clip rules, supported/refused geometry, hashes, guard tests, and 511/513 API.
