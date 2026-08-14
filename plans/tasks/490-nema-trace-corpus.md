# 490 — Sapporo Nema Trace Corpus

**Status:** blocked
**Phase:** 5
**Dependencies:** 295, 420

## Goal

Create a normalized, provenance-checked Nema corpus of synthetic fixtures and authentic hashes. This unlocks framing/state/texture/raster tickets 500–513; renderer code and proprietary bytes remain deferred.

## Execution Budget

One to two agent-days. Produce normalized, provenance-rich command expectations and tiny synthetic fixtures; implement no renderer.

## Required Reading

`native-nema-ring-bootstrap-decode.md`, `native-nema-first-draw-lists.md`, `native-nema-first-frame-lists.md`, `nema-a2le-blend-semantics.md`, `native-panel-transport.md`, Nema capture hooks under `../suunto-firmware/emulator/renode/graphics`, and `SapporoNemaP.cs`.

## Current Baseline

This repository has only three final frame hashes. No command-list/ring/texture corpus or expected refusal manifest exists. `suunto-firmware` contains authentic captures, but some older files are documented prefixes and raw command/texture bytes may be firmware-derived.

## Allowed Files

Only `fixtures/display/nema/**`, `tests/golden/nema-corpus.tsv`, and `tests/unit/test_nema_corpus.c`.

## Frozen Interfaces

Corpus TSV columns are `case evidence_id kind input_path input_sha256 expected_event expected_result`. Committed inputs are researcher-authored synthetic bytes only; authentic rows store size/hash and are optional through `SEMU_NEMA_TRACE_ROOT`. Prefix captures are tagged `prefix` and cannot be replayed as complete lists.

## Evidence Inputs

`E-NEMA-RING-001`, `E-NEMA-LISTS-001`, `E-NEMA-TEXTURE-001`, `E-NEMA-A2LE-001`, and `E-NEMA-PANEL-001` must name exact capture paths/hashes and distinguish bootstrap, complete list, prefix, texture, and physical panel evidence. Block a case whose completeness/provenance is ambiguous.

## Implementation

Create small synthetic ring/list/texture/refusal fixtures from documented formats, each with a provenance sidecar. Record authentic hashes without copying raw captures or pixels. Add structural corpus validation only.

## Tests and Commands

`make test TEST_FILTER=nema_corpus` runs only its binary, prints `nema corpus: valid`, and exits 0. `SEMU_NEMA_TRACE_ROOT=../suunto-firmware/emulator/display/captures make test TEST_FILTER=nema_corpus` additionally validates available authentic hashes and exits 0; absent optional root is an explicit skip. `make check` exits 0.

## Acceptance

All fixtures have provenance/hash/expected result; no raw firmware/texture/frame bytes are copied; prefix cases cannot be selected as complete; malformed/duplicate/missing input rows fail.

## Forbidden Scope

No parser/raster code, proprietary bytes, inferred shader semantics, relabeling staging rows as frames, golden change, or capture regeneration.

## Handoff

Report case IDs/kinds, verified/missing evidence, synthetic fixture hashes, optional skips, and exact cases consumed by 500–513.
