# 805 — Sapporo Widgets GPU Scale Integration

**Status:** ready
**Phase:** 7
**Dependencies:** 504,513,793,794

## Goal

Render the observed Widgets transition whose compressed-icon matrix carries
MM00=3f7fffff. The owner requested continuing GPU support on 2026-10-02.

## Execution Budget

One renderer predicate, one synthetic test module and three attributed native
navigation pin changes. No new decoding or sampling algorithm.

## Required Reading

README.md, docs/current-status.md, plans/index.tsv; docs/architecture.md display
ownership, docs/execution-model.md atomic rendering, docs/testing-strategy.md
and docs/compatibility-policy.md. E-RE-SAP235-TSC6A-001,
E-SAP-0041-EXT6/EXT7, E-RE-SAP235-WIDGET-SCALE-001;
src/display/nema_tsc6a_internal.h, nema_tsc6a.c, nema_tsc6a_expand.c,
nema_tsc6a_raster.c; tests/unit/test_nema_tsc6a_expand.c and the Allowed Files.

## Current Baseline

Widgets and Browse each refuse the same 60x57 clipped quad once. Control Panel
also encounters that refusal and two distinct 20x32 asset refusals. The existing
fixed-point converter rounds the new scale to exactly 65536; all 3420 samples
match exact rational pixel-center sampling. The scale admission alone is missing.

## Allowed Files

- src/display/nema_tsc6a_raster.c
- tests/unit/test_nema_tsc6a_widgets.c (new)
- tools/test_sdl_sapporo_235_navigation_restore.sh
- docs/migration-evidence.md
- docs/current-status.md
- README.md

Planning maintenance owns this ticket and its plans/index.tsv row. Dependencies
are done. Implementation leaves this ticket ready for integrator review.

## Frozen Interfaces

Public/internal headers, Makefile, profiles, registries, compatibility budgets,
snapshot layouts, CPU/time and non-publication device state, decoder and
sampling/blending laws. GPU publication counters follow the accepted submission.
All existing admissions and unrelated native pins remain unchanged.

## Evidence Inputs

E-RE-SAP235-WIDGET-SCALE-001 supplies the twice-reproduced matrix-writer RE,
native census and exact sample equivalence. E-SAP-0041-EXT6/EXT7 supplies the
existing translation and clipped-quad laws. E-RE-SAP235-TSC6A-001 supplies the
zero-auxiliary block decoder. Stop if the exact witnessed tuple cannot be
reproduced, or if native guest state changes beyond GPU/renderer publication.

## Implementation

Admit MM00=3f7fffff only for the observed quad (171,24)-(231,81), clip
(0,0)-(240,81), MM02=c32b0000, MM11=3f800000, MM12=c1c00000,
draw color ff555555. Preserve every other existing descriptor predicate and
full validation before mutation. No global float tolerance or new scale law.

## Tests and Commands

Red-first `make test TEST_FILTER=nema_tsc6a_widgets` must select and run the new
module. Run `make test TEST_FILTER=nema_tsc6a`, `make check-lines`, `make check`,
`make sanitize`, `make check-task-contracts`. After `make sdl`, run
`sh tools/test_sdl_sapporo_235_navigation_restore.sh`,
`sh tools/test_sdl_sapporo_235_exercise.sh`, and
`sh tools/test_sdl_sapporo_235_restore.sh` with SEMU_SDL_TEST_SNAPSHOT pointing
to a validated current 2.35 watchface snapshot. The scripts validate firmware
and use explicit guest budgets; compare six paired navigation logs/images.
Run `make check-era`; report missing verified 2.39 flash and possible pin drift.

## Acceptance

Check every panel pixel against an independent synthetic colored pattern;
nearby scales, altered witnessed fields and unknown visible blocks refuse
without writes. Before/after guest stop, time, instructions and all snapshot
sections except GPU/renderer are identical. GPU section 8 may change only
frame generation at offsets 500 and 4101 (+1 each); renderer section 10 may
change only generation at offset 20 (+1) and draw count at offset 40 (+11).
All other bytes remain equal. Only the three affected navigation
log/image pins may change, after paired attribution; retain exact full hashes.
Settled frame pixels must remain identical. Widgets/Browse have zero refusals;
the two distinct Control Panel 20x32 refusals remain explicit.

## Forbidden Scope

No general scale support, auxiliary-bit decoder, compressed writeback, cache
or persistence changes, permissive fallback, new dependencies or firmware bytes.
Keep the six README screenshots unchanged. No unrelated pin re-derivation.

## Handoff

Report scope, changed files, evidence IDs, red/green checks, paired hashes,
attribution and remaining GPU gaps. Integrator owns the final status review.
