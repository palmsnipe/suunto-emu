# 765 — Wismar Native Display Gate

**Status:** blocked
**Phase:** 7
**Dependencies:** 760

## Goal

Complete evidenced Wismar gap instances and reproduce exact native display and
interaction goldens deterministically.

## Execution Budget

Two integration days after required gaps complete.

## Required Reading

All Wismar handoffs, frame/input contracts, and Wismar native golden evidence.

## Current Baseline

No Wismar frame publication/goldens exist.

## Allowed Files

Wismar display/board/frontend integration, private tests, checked-in hashes and
replay metadata, coverage/evidence updates.

## Frozen Interfaces

Dimensions, formats, inputs and exact hashes are evidence-defined. Prior product
goldens remain unchanged.

## Evidence Inputs

Native frame/interaction hashes and transcript checkpoint. Missing evidence blocks.

## Implementation

Wire panel publication and semantic inputs, then compare full ordered records
twice under bounded execution.

## Tests and Commands

Wismar `make test-firmware ... TEST_PROFILE=wismar-2.46` runs every declared
golden twice; dummy SDL, `make check`, `make sanitize`, and earlier product
goldens pass.

## Acceptance

All exact hashes/checkpoints match with no unexpected stop or layer hit.

## Forbidden Scope

No visual tolerance, golden changes, later products, or unsupported-command acceptance.

## Handoff

Report hashes, interactions, transcript/layers, regressions, and gate result.

## Blocked-state audit note (2026-07-08, no-device constraint session)

Verified on disk (read-only audit; hashes spot-checked): no Wismar native
display capture exists anywhere in `/Users/cyril/projects/suunto-firmware`.
The audit's verdict for this ticket is not satisfiable today — but NOT
device-bound; a watch would not supply the missing capture; path is new
read-only RE tracing. Goldens are emulator-produced captures by repo
convention, so the missing first-display-request evidence is labeled "evidence
pending RE derivation", never "needs device". The stated blocker is therefore
still valid in class read-only RE derivation.
Status is left unchanged for integrator review.
