# 775 — One Eligible Product Reset Profile

**Status:** blocked
**Phase:** 7
**Dependencies:** 770

## Goal

Add exactly one eligible Tianjin, Rostock, or Xiamen profile and isolated board
through its evidenced bounded reset stop.

## Execution Budget

Two to three model-days for one product/version.

## Required Reading

Selected ticket 770 handoff, profile/board/SoC registries, and all legacy
regression contracts.

## Current Baseline

No selected-product profile or board exists. An ineligible intake cannot start
this ticket.

## Allowed Files

One profile directory, one board/SoC variant directory, registry integration,
and selected-product reset/profile tests.

## Frozen Interfaces

Core contracts and previous products remain unchanged. Reuse requires evidence
from ticket 770.

## Evidence Inputs

Complete eligible contract and trace hash from one ticket 770 instance.

## Implementation

Map exact memory/IRQ/wiring, add strict CLI/profile selection, and run to the
first unsupported observed behavior.

## Tests and Commands

`make test TEST_FILTER=<product>_reset` selects valid/invalid/map/input tests.
Private bounded run reproduces its stop twice. Run `make check-lines && make
check && make sanitize` and all prior product suites.

## Acceptance

Exact inputs reach the stop deterministically; invalid inputs fail before map;
legacy checkpoints remain.

## Forbidden Scope

No gap/device/display implementation, another product, or inferred sharing.

## Handoff

Report profile/board contract, evidence, stop/trace, regressions, and next gap.

## Blocked-state audit note (2026-09-11, no-device constraint session)

Verified on disk (read-only audit; hashes spot-checked): intake inputs are
reachable for Tianjin and Rostock — packages hash-verified in
`/Users/cyril/projects/suunto-firmware/artifacts/firmware/` (Rostock
`dd2e8fb3…`, Tianjin `d35ea721…`) (class F conditional). The stated blocker is
therefore still valid only as the 770 chain. What remains is in-repo
profile/board work after an eligible 770 handoff; no physical observation.
Status is left unchanged for integrator review.
