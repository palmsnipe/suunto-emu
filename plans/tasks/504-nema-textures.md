# 504 — Nema Texture Descriptor and Decode

**Status:** done
**Phase:** 5
**Dependencies:** 490, 500

## Goal

Validate and decode only evidenced RGB565/A2LE/RGBA4444 texture descriptors into bounded samples. This unlocks masked sampling 512; raster writes and semantic TSC6A remain deferred.

## Execution Budget

Two to three agent-days. Validate observed RGB565/A2LE/RGBA4444 descriptors and decode bounded source texels/coverage into a neutral form.

## Required Reading

`nema_framing.h`, `native-nema-first-frame-lists.md`, `nema-a2le-blend-semantics.md`, and `SapporoNemaP.cs:ExecuteTsc6aSource/ExecuteRgba4444Texture/ExecuteA2Mask` only for cases tied to evidence.

## Current Baseline

No texture code exists. The Renode model contains semantic TSC6A shadows and later-product formats mixed with Sapporo first-frame A2LE/RGB565 paths. Evidence warns private shader/TSC6A semantics are incomplete.

## Allowed Files

Only `src/display/{nema_texture.c,nema_texture.h,nema_a2le.c}` and `tests/unit/test_nema_texture.c`.

## Frozen Interfaces

Validate descriptor `{format,base,stride,width,height,sampling}` and complete bus range before reads. Expose bounded integer `sample(x,y)` returning RGBA/coverage plus refusal. Support only formats/cases named by `E-NEMA-TEXTURE-001`; A2LE decoder rejects malformed run length/row overflow atomically.

## Evidence Inputs

`E-NEMA-TEXTURE-001` must hash complete texture fixtures and prove format/stride/resolution. `E-NEMA-A2LE-001` proves A2LE encoding/sampling boundaries. TSC6A remains unsupported unless the row contains an independently verified decoder, not a semantic shadow.

## Implementation

Separate descriptor/range validation from format decoding; use integer conversions with explicit nibble/bit order; cap dimensions/decoded pixels; never retain guest pointers after the call.

## Tests and Commands

`make test TEST_FILTER=nema_texture` runs only its binary and exits 0; RGB565, A2LE runs/row edge, RGBA4444 only if verified, stride/range/overflow/malformed encoding/unsupported TSC6A, nearest coordinate bounds, and repeat samples pass under 240×240 fixture limits. `make check` exits 0.

## Acceptance

Decoded synthetic texels match provenance expectations; all source ranges are prevalidated; malformed input produces no partial decode; unverified formats refuse by name.

## Forbidden Scope

No semantic TSC6A fabrication, raster target writes, blending, bilinear unless evidenced, host image library, cached raw guest pointer, or later-product format.

## Handoff

Report formats/bit order/limits, unsupported list, evidence hashes, sample API, and 512/513 needs.
