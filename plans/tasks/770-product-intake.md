# 770 — Tianjin, Rostock, or Xiamen Intake

**Status:** blocked
**Phase:** 7
**Dependencies:** 630

## Goal

Determine whether one future product is eligible for implementation by creating
an exact independent firmware and hardware contract.

## Execution Budget

Up to three evidence days per product; dispatch separately for Tianjin,
Rostock, and Xiamen.

## Required Reading

Evidence/profile procedures, all prior product onboarding lessons, and the
selected product's read-only research/package sources.

## Current Baseline

Phase 6 supplies the independent intake tooling; earlier product releases are
lessons, not scheduling prerequisites. No product is eligible by name alone.
Xiamen explicitly lacks an implementation contract until an exact package and
hardware evidence are available.

## Allowed Files

Evidence/coverage docs, `fixtures/evidence/<product>/**`, and ignored local
extraction/trace scripts.

## Frozen Interfaces

Eligibility requires exact product/version/components/hashes/loads, memory/IRQ
contract, board wiring, first bounded stop, display/input contract, and source
provenance. Sharing requires proof.

## Evidence Inputs

Exact legally obtained package and trace/research sources. If missing, record
the product as ineligible and stop successfully without implementation tickets.

## Implementation

Validate/hash locally, capture repeatable bounded trace, enumerate unknowns,
and decide eligible/ineligible from explicit required fields.

## Tests and Commands

Strict manifest validation for eligible inputs and two equal normalized trace
hashes; `make check-lines && make check` passes. Ineligible handoff lists exact
missing fields rather than manufacturing placeholders.

## Acceptance

One product receives a complete evidence contract or a precise blocked record;
no firmware bytes or guessed fields enter Git.

## Forbidden Scope

No profile/code, several products per dispatch, family inheritance, or invented package.

## Handoff

Report product/version, eligibility, evidence IDs/hashes, contract/unknowns,
first stop, and whether ticket 775 may start.
