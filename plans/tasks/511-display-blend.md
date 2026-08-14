# 511 — RGB565 and A2LE Blend Math

**Status:** blocked
**Phase:** 5
**Dependencies:** 295, 490, 510

## Goal

Implement only evidence-proven RGB565 and A2LE blend equations as pure integer functions. This unlocks sampling 512; ambiguous rounding and raster loops remain deferred.

## Execution Budget

One to two agent-days. Implement only verified straight-alpha tint/mask and opaque RGB565 blend equations as pure integer functions.

## Required Reading

`docs/research/nema-a2le-blend-semantics.md`, `SapporoNemaP.cs:BlendArgb`, corpus A2LE cases, and the negative-evidence section on edge pixels.

## Current Baseline

No local blending exists. Research identifies straight-alpha tinted A2LE behavior but explicitly says available evidence may not prove bit-identical edge pixels. The Renode method is an emulator result, not sufficient hardware proof by itself.

## Allowed Files

Only `src/display/{blend.c,blend.h}` and `tests/unit/test_blend.c`.

## Frozen Interfaces

Pure functions consume unpacked source/destination/coverage/constant color and return RGB565LE value; channel expansion, multiplication, rounding, saturation, and packing are named constants/rules. Unsupported blend ID returns refusal through a checked wrapper; no global state.

## Evidence Inputs

`E-NEMA-A2LE-001` must cite the research note, exact Ambiq builder semantics, and controlled expected vectors or physical before/after evidence for rounding. If rounding is unverified, accept only coverage 0/255 and block intermediate-coverage golden claims.

## Implementation

Implement unpack/pack and verified blend modes with widened unsigned integer math; generate table-driven boundary tests from evidence; keep ambiguous formulas behind refusal.

## Tests and Commands

`make test TEST_FILTER=blend` runs only its binary and exits 0; coverage 0/255, channel extrema, tint alpha, saturation, all verified intermediate vectors, unsupported mode, and exhaustive deterministic 5/6-bit boundary subsets pass. `make check` exits 0.

## Acceptance

Pure vectors match evidence on Clang/GCC; unsupported rounding/modes refuse; no signed overflow/floating point; coverage endpoints are exact.

## Forbidden Scope

No raster loop, texture decoder, guessed intermediate rounding, visual tolerance, host graphics API, gamma correction, or later blend mode.

## Handoff

Report formulas/rounding confidence, supported mode IDs, vector counts, evidence hashes, and blockers for 512/520.
