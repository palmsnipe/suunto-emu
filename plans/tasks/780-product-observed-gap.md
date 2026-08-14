# 780 — One Future-Product Observed Gap

**Status:** blocked
**Phase:** 7
**Dependencies:** 775

## Goal

Implement one evidenced controller/device/display/input gap for the selected
future product and advance to the next checkpoint.

## Execution Budget

One to three model-days per behavior; repeat separately.

## Required Reading

Selected reset handoff, exact transcript/evidence entry, and relevant frozen
controller/device/display interface.

## Current Baseline

The selected product stops at one known unsupported observation.

## Allowed Files

One product-specific or proven-generic module, focused tests, and isolated
attachment integration.

## Frozen Interfaces

Fail-closed typed transactions, deterministic time, and all older product
contracts remain.

## Evidence Inputs

Exact byte/register/frame observation. Missing evidence blocks this instance.

## Implementation

Implement one behavior with positive/refusal/bounds/timing tests, rerun bounded,
and record the next stop. Repeat until native display/interaction gate is ready.

## Tests and Commands

`make test TEST_FILTER=<product>_<gap>` selects focused tests; private bounded
run advances twice identically; `make check-lines && make check && make sanitize`
and all prior suites pass.

## Acceptance

One narrow behavior is complete; no fallback widened; next checkpoint recorded.

## Forbidden Scope

No combined epic, guessed behavior, legacy golden changes, or automatic layers.

## Handoff

Report gap/evidence/tests/checkpoint and whether another instance or final gate follows.
