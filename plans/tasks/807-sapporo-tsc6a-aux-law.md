# 807 — Sapporo TSC6A Auxiliary-Region Law

**Status:** ready
**Phase:** 7
**Dependencies:** 504, 513, 793

## Goal

Derive and admit the 21-bit auxiliary-region law of the TSC6A format-17
block so the Control Panel's 20x32 icon renders, clearing the last two
named 2.35 GPU refusals. The owner assigned continuing GPU support on
2026-10-02 (the 805/806 lineage).

## Execution Budget

One offline-RE derivation (the owner-authorized 2026-09-23 class), one
decoder-law extension, one red-first test module, and two attributed
native navigation pin changes. No new sampling, blending, or cache
algorithm.

## Required Reading

README.md, docs/current-status.md, plans/index.tsv; docs/architecture.md
display ownership, docs/execution-model.md atomic rendering,
docs/compatibility-policy.md; E-RE-SAP235-TSC6A-001,
E-EMU-SAP235-ICON20-001, E-RE-SAP235-PXB2LOADER-001, US 9,640,149 B2
(the four-point mode; the prior census confirmed the patent does not
document the aux layout); src/display/nema_tsc6a_expand.c,
nema_tsc6a_raster.c, nema_tsc6a_internal.h; the Allowed Files.

## Current Baseline

The 2.33/2.35 era: `tsc6a_expand_block` fails closed on any nonzero bit
in the 21-bit region (bytes 9[7:3], 10, 11 of each 12-byte block). The
Control Panel 20x32 icon asset (SRAM `0x100a7aac`, a verbatim copy of a
resource-partition PXB2 container) sets auxiliary bits in 37-38 of its 40
blocks — the runtime refuses on block (2,0) (E-EMU-SAP235-ICON20-001).
The 2.35 resting-cache census (TICKTRAIL-002) counts 5,441 such blocks
outside pinned resolve regions.

## Evidence Inputs

The patent documents no aux layout, and the guest has no software decoder
(the PXB2 loader is a descriptor builder). The oracle is therefore
**structural**: the partition carries many PXB2 containers whose decoded
pixels are cross-checkable against the pinned golden decode
(`/tmp/sap235-tex17/decode-1.bin` class) for zero-aux blocks; for aux
blocks, the derivation must (a) survey the aux-value distribution across
all 432 containers (done in the instance-22 census: 14,989 distinct
values, all 21 bits used at 20-36%), (b) test structural hypotheses
(third endpoint at 7-bit/channel = 21 bits exactly; per-pixel 1-bit
modifiers + 5-bit mode; an alpha-scale exponent), and (c) validate the
winning hypothesis by rendering an aux-bearing container's asset through
the derived law and comparing against any available native pixel truth.
If no hypothesis validates against a pixel oracle, the law stays
fail-closed and the refusal stands — record that outcome honestly.

## Implementation

Admit the derived auxiliary-region semantics in `tsc6a_expand_block`'s
aux check only, keeping the bits 0..74 law and the four-point table
untouched; the check must stay pure (no bus, no state) and refuse any
pattern not admitted by the derived law. The 20x32 shape admission and
its law are frozen.

## Allowed Files

- src/display/nema_tsc6a_expand.c (the law only)
- tests/unit/test_nema_tsc6a_aux.c (new)
- tools/test_sdl_sapporo_235_navigation_restore.sh
- docs/migration-evidence.md, docs/current-status.md, README.md

Planning owns this ticket and its index row. Implementation leaves it
ready for integrator review.

## Frozen Interfaces

The zero-aux decode law (bits 0..74), the 4-point color table, the
alpha law, the shape/clip/translation admissions, snapshot layouts, and
every other refusal predicate. The 20x32 admission and its law are
frozen.

## Tests and Commands

Red-first `make test TEST_FILTER=nema_tsc6a_aux`; the full
`make test TEST_FILTER=nema_tsc6a` family; `make check-lines`,
`make check`, `make sanitize`, `make check-task-contracts`; after
`make sdl`, the navigation runner with the pinned watchface snapshot —
the two Control refusals must disappear with the image pins changing
only by attributed GPU/renderer publication counters, and the settled
Control frame CRC `30847819` staying identical. `make check-era` and
report state.

## Acceptance

The derived law renders the Control Panel icon (zero refusals, settled
frame CRC unchanged) or the census proves no oracle exists and the
refusal stands with the derivation recorded. Every admitted aux pattern
is witnessed by the pinned partition; near-miss states refuse. Two-run
equality for any moved pin; no unattributed drift.

## Forbidden Scope

No change to the bits 0..74 law, no general shape admission, no
compressed writeback, no fixture bytes in Git, no permissive fallback,
no silent pin move.

## Handoff

Report the derivation result (law or no-oracle outcome), changed files,
red/green checks, paired hashes, and remaining gaps. Integrator owns
the final status review.
