# 512 — Texture Sampling and Masked Draw

**Status:** done
**Phase:** 5
**Dependencies:** 504, 510, 511

## Goal

Combine verified texture sampling, fixed-point mapping, raster coverage, and blend into atomic draws. This unlocks Nema backend publication 513; unverified filtering/TSC6A remains deferred.

## Execution Budget

Two to three agent-days. Combine verified texture sampling, fixed-point coordinate mapping, raster coverage, and blend into atomic bounded draw operations.

## Required Reading

`nema_texture.h`, `raster.h`, `blend.h`, `native-nema-first-frame-lists.md`, corpus texture draws, and only the evidenced branches of `SapporoNemaP.cs:ExecuteA2Mask/ExecuteAlphaMask/ExecuteRgba4444Texture`.

## Current Baseline

No combined draw path exists. The Renode model mixes Sapporo, Race S, semantic TSC6A shadows, sampling heuristics, and pending bus writes; copying it wholesale would accept unproven formats and coordinates.

## Allowed Files

Only `src/display/{sampling.c,sampling.h,draw_mask.c,draw_texture.c}` and `tests/unit/test_sampling.c`.

## Frozen Interfaces

Consume immutable draw snapshot, validated texture sampler, blend mode, and raster target. Coordinate transform/filter rule is evidence-selected; stage complete dirty-region pixels before one commit so source/range/format/refusal cannot partially mutate target. Cap operation to target pixels.

## Evidence Inputs

`E-NEMA-LISTS-001`, `E-NEMA-TEXTURE-001`, and `E-NEMA-A2LE-001` must jointly prove geometry/source coordinates, sampling mode, tint/blend, and expected synthetic pixels. Bilinear, RGBA4444, or intermediate A2LE coverage remains refused unless all required evidence exists.

## Implementation

Implement nearest/mask paths individually; preflight source and target; use 64-bit checked fixed-point stepping; stage at most 240×240 RGB565 pixels; invoke only pure helpers from dependencies.

## Tests and Commands

`make test TEST_FILTER=sampling` runs only its binary and exits 0; 1:1 RGB565, translated/clipped A2LE mask, evidenced filtering, source/target boundary, fixed-point overflow, unsupported format/mode/rounding, atomic refusal, guard bytes, and repeat hash pass. `make sanitize TEST_FILTER=sampling` and `make check` exit 0.

## Acceptance

Supported corpus draw hashes match exactly; all preflight failures leave target unchanged; memory is bounded; no semantic TSC6A or visual tolerance enters results.

## Forbidden Scope

No command/state parsing, unverified filtering, full-frame compatibility shadow, direct guest-memory writes, SDL, publication, or later-product branch.

## Handoff

Report supported draw matrix, coordinate/filter rules, operation limits, hashes/refusals, and 513 integration API.
