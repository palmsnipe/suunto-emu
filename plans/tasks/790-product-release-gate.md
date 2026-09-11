# 790 — One Future-Product Release Gate

**Status:** blocked
**Phase:** 7
**Dependencies:** 780

## Goal

Integrate all required selected-product gap instances and prove its exact native
display and physical interaction record without regressing earlier products.

## Execution Budget

Two to three integration days after gap work.

## Required Reading

All selected-product handoffs, golden/input evidence, compatibility policy, and
legacy release gates.

## Current Baseline

The selected product is not released until exact private goldens exist and every
required observed transaction is supported.

## Allowed Files

Selected-product integration/private tests, checked-in hashes/replay metadata,
registries, and coverage/evidence status.

## Frozen Interfaces

Exact formats/dimensions/hashes/inputs derive from evidence. Prior goldens and
layer contracts cannot change.

## Evidence Inputs

Complete normal and interaction frame/checkpoint hashes plus two-run device and
compatibility transcript expectations.

## Implementation

Wire final publication/input paths and compare the full ordered record across
two bounded runs.

## Tests and Commands

Selected `make test-firmware ... TEST_PROFILE=<id>` runs all goldens twice;
dummy SDL, `make check`, `make sanitize`, and every earlier product release gate
pass.

## Acceptance

Exact records match with no unexpected stop/layer hit/source mutation and no
legacy changes.

## Forbidden Scope

No visual tolerance, skipped interaction, hash weakening, or another product.

## Handoff

Report product/version, hashes, inputs, transcripts, layers, full regression
matrix, and release status.

## Blocked-state audit note (2026-09-11, no-device constraint session)

Verified on disk (read-only audit; hashes spot-checked): the audit classifies
this gate as an F conditional chain gated on 775/780, not on a device.
Goldens here are emulator-produced captures by repo convention, so any
endpoint evidence still unavailable is "evidence pending RE derivation"
(new read-only tracing), never "needs device". The stated blocker is therefore
still valid only as the chain above it. What remains is in-repo integration
work once the product chain lands.
Status is left unchanged for integrator review.
