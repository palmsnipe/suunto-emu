# 802 — Compressed Resolve Refusal Integration

**Status:** ready
**Phase:** 7
**Dependencies:** 504,513,761,793

## Goal

Prevent unsupported compressed baseline blocks from being published as stale
cached pixels. The owner assigned the GPU correctness recommendations on
2026-10-02. This slice adds a refusal, not an invented auxiliary-bit decoder.

## Execution Budget

One bounded rendering integration; focused synthetic regressions followed by
existing 2.22/2.35 firmware gates. Keep all private artifacts external.

## Required Reading

README, current status, index, architecture display ownership, execution-model
transactions/snapshots, testing strategy and compatibility policy;
E-RE-SAP235-TSC6A-001, E-EMU-SAP235-TICKTRAIL-002,
E-EMU-NEMA-CACHE-LIFECYCLE-001, E-EMU-RENDERER-SNAPSHOT-002;
all Allowed Files, src/display/nema_tsc6a.h, include/semu/display.h,
src/display/nema_backend_transaction.c, and tests/unit/test_renderer_snapshot.c.

## Current Baseline

A known block rewritten with nonzero auxiliary bits retains historical decoded
pixels. DRAW_CMD=10 can sample them despite the existing codec refusing that
block. The snapshot intentionally preserves this cache history. A bounded
resolve must not publish an unsupported baseline sample as decoded data.

## Allowed Files

- src/display/nema_tsc6a_sync.c
- src/display/nema_tsc6a_expand.c
- src/display/nema_tsc6a_internal.h
- src/display/nema_tsc6a_raster.c
- src/display/nema_backend_internal.h
- src/display/nema_backend_draw.c
- tests/unit/test_nema_tsc6a_lifecycle.c
- docs/execution-model.md
- docs/migration-evidence.md
- docs/current-status.md
- README.md

Planning setup owns this ticket and its index row. Internal rendering seams
are explicitly in scope; no new public API or duplicate device interface.

## Frozen Interfaces

Public headers, profiles, registries, Makefile, compatibility budgets, snapshot
encoding and all existing firmware log/snapshot/frame pins. Leave status ready.

## Evidence Inputs

The existing bit-75..95 refusal law and transactional display contract support
refusing sampled unknown blocks. They supply no replacement pixel values.
Synthetic failures demonstrate stale publication; do not claim hardware truth.

## Implementation

Validate the complete DRAW_CMD=10 source footprint before destination writes,
using exactly the resolve matrix, pixel centers, floor and clipping rules.
Blocks outside that footprint remain untouched. Preserve cache rollback and
serialized history. Reuse the decoder's supported-block predicate. Leave the
separate semantic mask/quad model and auxiliary decoding/writeback unchanged.

## Tests and Commands

Red-first stale-rewrite and cold-unknown regressions; test valid neighbors,
translated/fractional footprints, and atomic refusal. Run make test
TEST_FILTER=nema_tsc6a_lifecycle, make check-lines, make check,
make sanitize, make check-task-contracts, make check-sdl, all Sapporo 2.35
firmware runners and restored navigation. Run make check-era or report its
missing exact input and possible drift explicitly.

## Acceptance

The new refusals preserve full committed snapshots, publication and generation;
known samples and unknown blocks outside the footprint still succeed. Existing
firmware pins must pass unchanged. If supported paths depend on unknown
baseline samples, stop integration, record the exact evidence gap, and preserve
the production runtime instead of weakening the gate.

## Forbidden Scope

No guessed clearing/codec, compressed writeback, snapshot version, peripheral
admission, old-pin replacement, or claim of complete GPU fidelity.

## Handoff

Report regression and firmware evidence, exact commands/results, unchanged
pins, unsupported cases and any integration blocker. Integrator owns promotion.
