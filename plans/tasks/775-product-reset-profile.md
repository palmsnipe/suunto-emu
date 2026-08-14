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
