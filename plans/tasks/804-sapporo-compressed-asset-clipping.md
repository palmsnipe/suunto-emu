# 804 — Sapporo Compressed Asset Clipping

**Status:** ready
**Phase:** 7
**Dependencies:** 504,513,793

## Goal

Render supported visible portions of the existing 60x60 compressed-asset quad
when unsupported auxiliary blocks occur only outside its sampled footprint.
The owner requested continuing GPU fidelity work on 2026-10-02.

## Execution Budget

One bounded renderer change and one synthetic test module. Reuse the proven
block decoder, pixel-center mapping and existing clip/transform admissions.

## Required Reading

README.md, docs/current-status.md, plans/index.tsv; architecture display
ownership, execution-model atomic rendering, testing strategy and compatibility
policy. E-RE-SAP235-TSC6A-001, E-SAP-0041-EXT6/EXT7,
E-EMU-TSC6A-CLIP-001; src/display/nema_tsc6a_raster.c,
nema_tsc6a_internal.h, nema_tsc6a.c, nema_tsc6a_expand.c;
tests/unit/test_nema_tsc6a_expand.c and the Allowed Files.

## Current Baseline

The compressed asset resolver validates memory, then expands all 225 blocks
before clipping. One unsupported offscreen block refuses otherwise supported
visible pixels. DRAW_CMD=10 already validates its sampled blocks separately;
that path, its cache, and the semantic-shadow path are unchanged here.

## Allowed Files

- src/display/nema_tsc6a_raster.c
- tests/unit/test_nema_tsc6a_clip.c (new)
- docs/migration-evidence.md
- docs/current-status.md
- README.md

This planning setup owns its new ticket and index row. All dependencies are
done; implementation leaves the status ready for integrator review.

## Frozen Interfaces

Public/internal headers, registries, profiles, Makefile, snapshot formats,
CPU/virtual-time behavior, existing draw-state admissions, block decode law,
blend/sampling laws and all firmware goldens. No new private parallel API.

## Evidence Inputs

E-RE-SAP235-TSC6A-001 supplies independent 4x4 blocks and the decoder law;
E-SAP-0041-EXT6/EXT7 supplies the clipped quad and translated sampling laws.
E-EMU-TSC6A-CLIP-001 records the integrator's narrow composition of those laws.
No new compressed bit semantics or inferred GPU writeback are authorized.

## Implementation

Validate the entire 2700-byte source span as before, including an empty clip.
Use the existing pixel-center mapping for both preflight and drawing. Mark
sampled blocks, decode every marked block before any target write, and shade
only from these decoded blocks. Unknown visible blocks refuse atomically;
unknown unsampled blocks remain uninterpreted. This narrows the original
all-225-block refusal policy only for already-admitted clipped asset draws.

## Tests and Commands

Red-first `make test TEST_FILTER=nema_tsc6a_clip` (at least one test), existing
`make test TEST_FILTER=nema_tsc6a_expand`, `make check-lines`, `make check`,
`make sanitize`, `make check-task-contracts`. Run the 2.35 restored navigation
and exercise scripts and both 2.22/2.35 SDL restore gates with validated current
snapshots. Run `make check-era` or report its exact missing private input and
possible pin drift. Use explicit instruction/virtual-time limits.

## Acceptance

Full pixel expectations for clipped horizontal and fractional vertical cases,
empty-clip behavior, every unknown auxiliary bit in a late visible block
refusing with zero target writes, and missing source memory still refusing.
Existing decoder tests and authentic snapshots/logs retain their pins. No
claim of full TSC6A auxiliary-plane support or hardware capture equivalence.

## Forbidden Scope

No guessed auxiliary decoder, new scale words or descriptor tuples, writeback,
cache changes, silent golden replacement, firmware bytes or new dependencies.

## Handoff

Report changed files, evidence IDs, exact commands/results and preserved pins;
list the remaining auxiliary-plane, writeback and Widgets matrix gaps. Keep
the six authorized README screenshot files unchanged.
