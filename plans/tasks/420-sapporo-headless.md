# 420 — Sapporo Headless Phase 4 Gate

**Status:** blocked
**Phase:** 4
**Dependencies:** 410, 415

## Goal

Integrate exact Sapporo firmware validation, board devices, and optional layer to reach production/startup completion headlessly.

## Allowed Files

Board/profile/compat registries, `src/boards/sapporo*` integration, `src/frontends/headless*`, `tests/integration/sapporo_headless*`, `tests/golden/sapporo_2_22*`, Makefile test lists, `docs/{hardware-coverage,migration-evidence}.md`, `plans/index.tsv` status only.

## Frozen Interfaces

CLI implements `list`, `show-profile`, `validate`, `list-layers`, and `run` options from the approved plan. `--profile` accepts a built-in ID or an explicit profile path parsed with the strict schema. `--until` uses named checkpoints; firmware runs require explicit instruction or virtual-time bound. Compatibility remains named and opt-in.

## Evidence Inputs

All Phase 4 device/profile ledger entries and locally supplied firmware matching the exact profile hashes.

## Implementation

Wire devices and layer; define stable startup checkpoint; normalize transcripts/summaries; add private-firmware test discovery through `FIRMWARE_ROOT`; distinguish absent private data skip from functional failure.

## Tests and Commands

`make check`; `build/suunto-emu list`; `build/suunto-emu validate --profile sapporo-2.22.60.3383-P --firmware "$FIRMWARE_ROOT/sapporo-2.22.semu"`; `make test-firmware FIRMWARE_ROOT="$FIRMWARE_ROOT" TEST_FILTER=sapporo_headless`.

## Acceptance

Exact firmware reaches the declared checkpoint with no unmapped access, unsupported instruction, refused unexpected transaction, assertion, or unexpected layer hit; source hashes remain unchanged; two runs have identical summaries.

## Forbidden Scope

No frame hash claim, renderer/SDL, permissive boot hacks, embedding private bytes, unbounded run, later firmware, or changing CPU/device semantics without a regression.

## Handoff

Publish checkpoint name, bounded run settings, normalized transcript hash, layer counts, and Phase 4 coverage status.
