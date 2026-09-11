# 740 — Ulsan Native Display and Interaction Gate

**Status:** blocked
**Phase:** 7
**Dependencies:** 730

## Goal

Integrate proven Ulsan gaps until native 466x466 NemaDC output and evidenced
crown/touch/button interaction reproduce exact private goldens.

## Execution Budget

Two to three integration days after all required ticket 730 instances finish.

## Required Reading

All Ulsan gap handoffs, display/frame/input contracts, and Ulsan golden evidence.

## Current Baseline

No Ulsan frames or input replay exist. This ticket cannot invent goldens or
complete while any required native transaction remains unsupported.

## Allowed Files

Ulsan board/display/frontend integration, Ulsan private integration tests,
checked-in hashes/replay metadata, coverage/evidence status, and registry glue.

## Frozen Interfaces

466x466 RGB/frame contract, semantic inputs, and exact hashes come only from
evidence. Existing Sapporo goldens remain unchanged.

## Evidence Inputs

Native frame hashes, byte count, input checkpoints, and two-run transcript hash.
Missing goldens keep this ticket blocked.

## Implementation

Wire NemaDC/panel publication and semantic crown/touch/button replay; compare
ordered frames, inputs, layer hits, and device traces.

## Tests and Commands

`make test-firmware FIRMWARE_ROOT="$FIRMWARE_ROOT" TEST_PROFILE=<ulsan-id>` runs
normal and each evidenced input golden twice. `SDL_VIDEODRIVER=dummy make
check-sdl`, all Sapporo goldens, `make check`, and `make sanitize` pass.

## Acceptance

Exact dimensions/byte count/hashes and interaction checkpoints match twice;
no unexpected stop or layer hit occurs.

## Forbidden Scope

No tolerant image comparison, missing-input skip, hash changes, or later products.

## Handoff

Report version, hashes, inputs, transcripts, layer counts, regressions, and gate status.

## Blocked-state audit note (2026-09-11, no-device constraint session)

Verified on disk (read-only audit; hashes spot-checked): Ulsan golden material
exists as emulator-produced captures (the repo convention for goldens): 15
ordered 466x466 screens of 434,312 B each, final SHA `d3014e70…`, and an L4
4-bit text layer (466x42, stride 233, SHA `2d7a037a…`), regenerable via
`run-2.44-full-ui.sh` under `/Users/cyril/projects/suunto-firmware/emulator/`
(the prior `/tmp` capture session is gone but reproducible). The stated
blocker is therefore partially stale. What remains — pairing completion and
the post-pairing watch face — is "evidence pending RE derivation" (new
read-only tracing), not "needs device": a watch would not produce these
emulator-convention goldens.
Status is left unchanged for integrator review.
