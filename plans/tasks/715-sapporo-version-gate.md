# 715 — One Later Sapporo Release Gate

**Status:** blocked
**Phase:** 7
**Dependencies:** 710

## Goal

Integrate all required gap-ticket instances for one later Sapporo version and
prove native startup, display, and three-button interaction deterministically.

## Execution Budget

Two integration days after the selected version's gap instances complete.

## Required Reading

The selected 700/705 handoffs, every selected-version 710 handoff, Sapporo 2.22
release contracts, and selected-version frame/input evidence.

## Current Baseline

A selected later version has a strict profile and one or more narrow observed
gap implementations. It is not released merely because it passes reset.

## Allowed Files

Selected-version Sapporo integration/private tests, checked-in frame hashes and
replay metadata, profile/board integration, and coverage/evidence status.

## Frozen Interfaces

Existing 2.22 goldens and hardware behavior remain unchanged. Selected-version
compatibility layers remain exact-hash, opt-in, logged, and hit-bounded.

## Evidence Inputs

Exact normal and three-button checkpoint/frame hashes, expected compatibility
counts, and normalized device transcript hash for the selected version. Missing
goldens keep this ticket blocked.

## Implementation

Wire completed gaps into the version profile, define bounded startup/display
checkpoints, replay all three buttons, and compare the full ordered run record
twice.

## Tests and Commands

The instantiated ticket replaces `<id>` with the selected built-in profile.
`make test-firmware FIRMWARE_ROOT="$FIRMWARE_ROOT" TEST_PROFILE=<id>` runs all
selected-version private goldens twice and exits 0. `make test-firmware ...
TEST_PROFILE=sapporo-2.22.60`, `make check-sdl`, `make check`, and `make
sanitize` also exit 0.

## Acceptance

Exact selected-version startup, frame/input hashes, layer counts and device
transcripts match twice; source images are unchanged; 2.22 records are identical.

## Forbidden Scope

No angle-bracket dispatch, tolerance-based frames, golden changes, auto-layers,
another version, or Ulsan work.

## Handoff

Report instantiated version, hashes/checkpoints, layers/transcript, commands,
source before/after hashes, regressions, and release result.

## Blocked-state audit note (2026-07-08, no-device constraint session)

Verified on disk (read-only audit; hashes spot-checked): repo goldens are
emulator-produced captures by convention, and the audit found everything up to
the fifth-GPS-pulse boundary derivable from the Sapporo bundles already on
disk; only past-that-boundary behavior coincides with the 776 physical gap.
The stated blocker is therefore partially stale (class F partial). What
remains is in-repo golden production gated on ticket 710; no device-side
observation is needed for the pre-boundary goldens.
Status is left unchanged for integrator review.
