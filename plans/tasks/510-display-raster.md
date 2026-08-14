# 510 — Rasterization and Physical Panel Composition

**Status:** blocked
**Phase:** 5
**Dependencies:** 500

## Goal

Render validated Nema operations into deterministic RGB565 surfaces and compose the 240x240 Sapporo physical panel.

## Allowed Files

`src/display/{surface,raster_*,blend,panel}*`, `tests/unit/display_{surface,raster,blend,panel}*`, `fixtures/synthetic/display/**`; no SDL/frontend or command-parser edits.

## Frozen Interfaces

Consume the operation stream and frame callback from 500. Integer pixel rules, clipping, RGB565 packing, blending/rounding, texture sampling, and stride are explicit and host-endian independent. Published frame bytes remain borrowed/immutable during callback.

## Evidence Inputs

Observed Nema operation semantics and synthetic reference cases entered in the ledger; final private hashes `E-SAP-0002`–`0004` are integration gates.

## Implementation

Split primitives by responsibility; validate all surfaces/coordinates; publish exactly 240x240x2 bytes at an explicit presentation command; increment generation once per published frame.

## Tests and Commands

`make test TEST_FILTER=display_surface`; `make test TEST_FILTER=display_raster`; `make test TEST_FILTER=display_panel`; `make test TEST_FILTER=display_determinism`; `make check`.

## Acceptance

Synthetic clipping, texture, blending, overlap, stride, endian, invalid-range, and generation tests pass byte-for-byte on repeated runs and supported compilers.

## Forbidden Scope

No SDL, floating nondeterministic raster math, Nema parser expansion, anti-aliasing not proven by traces, private screenshots/firmware bytes, or golden adjustment.

## Handoff

Report exact pixel rules, synthetic hashes, performance bounds, and any observed operation still unsupported.

