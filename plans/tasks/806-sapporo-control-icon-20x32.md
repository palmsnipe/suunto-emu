# 806 — Sapporo Control Panel 20x32 Icon GPU Admission

**Status:** `ready`
**Phase:** 7
**Dependencies:** 504, 513, 793, 794

## Goal

Render the two observed Control Panel 20x32 compressed-icon draws through the
existing TSC6A compressed-asset path, removing the last two named 2.35 GPU
refusals while keeping every other unsupported shape fail-closed. The owner
assigned continuing GPU support on 2026-10-02 (ticket 805 lineage).

## Execution Budget

One shape-family admission in the existing resolver, one synthetic test
module, and two attributed native navigation pin changes (the control log and
image pins; all other five cases unchanged). No new decoder, sampler, or
blending algorithm.

## Required Reading

README.md, docs/current-status.md, plans/index.tsv; docs/architecture.md
display ownership, docs/execution-model.md atomic rendering,
docs/testing-strategy.md, docs/compatibility-policy.md;
E-RE-SAP235-TSC6A-001, E-SAP-0041-EXT6/EXT7, E-RE-SAP235-WIDGET-SCALE-001;
src/display/nema_tsc6a_raster.c, nema_tsc6a_expand.c,
nema_tsc6a_internal.h; tests/unit/test_nema_tsc6a_widgets.c and
test_nema_tsc6a_clip.c as the red-first precedents; the Allowed Files.

## Current Baseline

The restored-navigation control case refuses the same 20x32 fmt-17 asset at
SRAM `0x100a7aac` twice (draw ords 347 and 355 at 39438579302 and
39447038897 ns; child `0x100d0800` offsets 5488 and 3808). Both draws carry
the identical source, quad and matrix and differ only in the screen strip
clip (`clip_min_y` 162 vs 193). Widgets and Browse have zero refusals; the
other four navigation cases keep their pins.

## Witnessed Tuple (twice-reproduced, pair byte-identical, /tmp/ctrl20/run)

- source: fmt 0x17, sampling 1, stride 60, 20x32, base `0x100a7aac`,
  span 60 * 8 = 480 B (5 x 8 blocks of 12 B)
- target: RGB565 240x240, stride 480
- draw 5; tex color `0xffffffff`; draw color `0xffffffff`
- codeptr `0x941e8000`, matmult 0, imem (0, `0x004e0002`, `0x804b1286`)
- matrix: MM00 `3f800000`, MM01 0, MM02 `c3480000` (-200.0),
  MM10 0, MM11 `3f800000`, MM12 `c34b0000` (-203.0) — identity scale,
  MM02 = -rect_x0, MM12 = -rect_y0 (the composer emits MM02 = -dstX,
  MM12 = -dstY per the 0xc1b5e set-matrix RE; the quad is the full asset)
- quad: (200,203)-(220,203)-(220,233)-(200,233) in 16.16
- clips: (0,162)-(240,240) and (0,193)-(240,240); the quad is fully inside
  both, so every sample of the visible pixels decodes exactly the same
  40-block set in both draws

Sample equivalence: identity scale maps x 200..219 -> texel columns 0..19
and y 203..232 -> source rows 0..29 exactly (integer translations, no ULP
class); source rows 30..31 are never sampled. The existing pixel-center
mapping and fixed-point conversion produce this without change.

## Finding that re-scopes this ticket (2026-10-08 census)

The witnessed asset itself carries nonzero auxiliary bits: the runtime
refusal moves to `compressed TSC6A block 2,0 sets the unverified auxiliary
bits` once the shape is admitted. The SRAM asset is a verbatim copy of a
resource-partition PXB2 container — the pinned component-05 partition has
two width-20 containers with 8 block rows (20x30 visual at `0x9b8a00` and
`0xa66000`, span 480, matching the texture's 60x8 layout; two 20x28 at
`0x9b9800`/`0xa65e00`), and 37..38 of each candidate's 40 blocks set bits
75..95. This matches the runtime refusal exactly (block 2,0 aux nonzero in
both candidates) and the TICKTRAIL-002 resting-cache census (aux-bit blocks
outside resolve regions). Rendering the icon therefore REQUIRES the 21-bit
auxiliary-region law — the exact gap the README names — which is a separate
offline-RE evidence instance (owner-authorized class) with no lane oracle.

Re-scoped outcome of this ticket: the shape admission, its law, the red-first
tests and the attributed control-log pin move land here; the two Control
Panel refusals remain (now naming block 2,0), zero writes either way, the
control snapshot pin byte-identical. Rendering completion is blocked on the
auxiliary-bit law and tracked for a new ticket.

## Allowed Files

- src/display/nema_tsc6a_raster.c
- tests/unit/test_nema_tsc6a_icon20.c (new)
- tools/test_sdl_sapporo_235_navigation_restore.sh
- docs/migration-evidence.md
- docs/current-status.md
- README.md

Planning maintenance owns this ticket and its plans/index.tsv row.
Implementation leaves this ticket ready for integrator review.

## Frozen Interfaces

Public/internal headers, Makefile, profiles, registries, compatibility
budgets, snapshot layouts, CPU/time and non-publication device state, the
decoder and sampling/blending laws. All existing admissions (793, 788/EXT6,
EXT7, 804, 805) and unrelated native pins remain unchanged.

## Evidence Inputs

The twice-reproduced control refusal pair above (full diagnostics in the
draw-refused lines of the retained control logs). No lane probe is possible
for this shape (the compressed asset path is lane-unmodeled per
E-RE-SAP235-TSC6A-001); the offline-RE class is not required because the
admission needs no new decode law — only the already-pinned block law
applied to the witnessed shape, translations and clips.

## Implementation

Add a second witnessed shape family to the compressed-asset resolver:
20x32/stride-60/480-B source with the pinned quad, identity-scale matrix,
`0xffffffff` colors and the two observed clips. The translation law for this
family is MM02 = -rect_x0 and MM12 = -rect_y0 (exact integers; the one-ULP
slack classes do not apply). Validate the full 480-B source span, preflight
and decode only sampled blocks (ticket 804 law), refuse unknown visible
auxiliary bits with zero writes, and keep the 60x60 family's predicates,
the 480x480 semantic-shadow branch and the fallback diagnostic unchanged.
Rect bounds for this family: full 20x32 (the only witnessed rect), with the
existing clip-cut rule available for sub-height/sub-width rects exactly as
the 60x60 family uses it.

## Tests and Commands

Red-first `make test TEST_FILTER=nema_tsc6a_icon20` must select and run the
new module (positive render on a synthetic pattern, per-pixel panel check,
near-miss refusals: altered quad/matrix/clip/color/src-span/aux-bit states).
Run `make test TEST_FILTER=nema_tsc6a`, `make check-lines`, `make check`,
`make sanitize`, `make check-task-contracts`. After `make sdl`, run
`sh tools/test_sdl_sapporo_235_navigation_restore.sh` with
`SEMU_SDL_TEST_SNAPSHOT` pointing at a validated current 2.35 watchface
snapshot (pinned sha `c56057a9…`): the control log/image pins change only by
the two removed draw-refused lines and the attributed GPU/renderer
publication counters; all other five cases and the settled Control Panel
frame CRC `30847819` stay identical. Run `make check-era` and report state.

## Acceptance

Control Panel reaches zero GPU refusals; the settled control frame CRC32
`30847819` and every other settled frame are byte-identical; guest stop,
instructions, virtual time and all snapshot sections except GPU/renderer
publication counters are unchanged. Only the two affected navigation pins may
change, after paired attribution with exact full hashes retained. Unknown
shapes, aux-bit blocks and writeback stay fail-closed.

## Forbidden Scope

No general shape support, no auxiliary-bit decoder, no compressed writeback,
no cache or persistence changes, no permissive fallback, no new dependencies
or firmware bytes. Keep the six README screenshots unchanged. No unrelated
pin re-derivation.

## Handoff

Report scope, changed files, evidence IDs, red/green checks, paired hashes,
attribution and remaining GPU gaps. Integrator owns the final status review.
