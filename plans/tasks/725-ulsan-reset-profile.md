# 725 — One Ulsan Reset Profile

**Status:** done
**Phase:** 7
**Dependencies:** 720

## Goal

Add one evidence-eligible Ulsan version with isolated Apollo4 Plus memory/wiring
and reach its recorded bounded reset stop.

## Execution Budget

Two model-days; one version only.

## Required Reading

Ticket 720 selected-version handoff, machine/board/profile integration, Apollo4
interfaces, and existing product regression tests.

## Current Baseline

Machine creation is Sapporo-specific; no Apollo4 Plus variant or Ulsan board
registry exists.

## Allowed Files

One Ulsan profile directory, `src/boards/ulsan*`, Apollo4 Plus variant mapping,
registries, and Ulsan reset/profile tests.

## Frozen Interfaces

Generic core/CPU contracts do not change. Reuse Apollo4 blocks only when ticket
720 proves register behavior identical; board wiring remains isolated.

## Evidence Inputs

Exact selected-version evidence IDs and trace hash from 720.

## Implementation

Add strict profile, board map/attachments, semantic input declarations known at
reset, and a bounded run that stops at the next unsupported transaction.

## Tests and Commands

`make test TEST_FILTER=ulsan_reset` selects Ulsan profile/map/wrong-input tests
and exits 0. Private bounded run validates hashes and reproduces its stop twice.
Run `make check-lines && make check && make sanitize` plus all Sapporo tests.

## Acceptance

Independent profile reaches recorded stop deterministically; invalid metadata
fails before mapping; Sapporo behavior is unchanged.

## Forbidden Scope

No device-gap implementation, display output, another Ulsan version, pairing,
BLE, or unproven shared wiring.

## Handoff

Report version, evidence, maps/wiring, stop/trace, regression results, and next gap.

## Blocked-state audit note (2026-09-11, no-device constraint session)

Verified on disk (read-only audit; hashes spot-checked): nothing physical is
required for this ticket — its evidence inputs are ticket 720's outputs, and
the audit verified the Ulsan `.sof` pair (sha256 `276ca7e6…` / `52cea276…`)
plus extracted components under `/Users/cyril/projects/suunto-firmware` are
already on disk awaiting registration (class F conditional). The stated
blocker is therefore still valid as an in-repo chain on 720, not as an
evidence absence. What remains is profile/board work in this repository.
Status is left unchanged for integrator review.
