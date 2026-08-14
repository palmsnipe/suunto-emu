# 718 — Later Sapporo Expansion Gate

**Status:** blocked
**Phase:** 7
**Dependencies:** 715

## Goal

Verify that every evidence-eligible Sapporo version from ticket 700 has its own
completed 705/710/715 instances and that the full cross-version suite is stable.

## Execution Budget

One integration day; no new hardware behavior.

## Required Reading

Ticket 700 eligibility handoff, every instantiated 705/710/715 handoff,
profiles index, compatibility declarations, and all Sapporo private goldens.

## Current Baseline

The generic index can encode template dependencies but not dynamic per-version
instances. This explicit gate prevents Ulsan work from starting after only one
of several eligible Sapporo versions is finished.

## Allowed Files

Sapporo suite integration tests, profile index integration, coverage status,
and this gate's normalized expected-record metadata.

## Frozen Interfaces

No profile, hardware, CPU, display, or compatibility semantics change. This is
an audit/integration gate only.

## Evidence Inputs

Complete eligible-version list from 700 and release records from each 715
instance. An eligible version without a release record blocks the gate.

## Implementation

Enumerate eligible versions, reject missing/duplicate profile registrations,
run each public and private suite, and compare a normalized cross-version record
twice. Confirm layers never wildcard product versions or hashes.

## Tests and Commands

For every eligible `<id>`, run `make test-firmware FIRMWARE_ROOT="$FIRMWARE_ROOT"
TEST_PROFILE=<id>` twice. Run `make test TEST_FILTER=sapporo_profiles`, `make
check-sdl`, `make check`, and `make sanitize`; all exit 0. The instantiated
gate records the exact IDs and record hash before dispatch.

## Acceptance

Every eligible 2.33/2.35/2.39 version is independently released or explicitly
removed from eligibility with evidence; two cross-version records match; 2.22
remains unchanged.

## Forbidden Scope

No angle-bracket dispatch, new behavior, skipped eligible version, wildcard
profile/layer, Ulsan code, or golden changes.

## Handoff

Report exact eligible IDs, per-version gate result, suite record hash, layer
audit, compiler/sanitizer results, and readiness for Ulsan evidence.
