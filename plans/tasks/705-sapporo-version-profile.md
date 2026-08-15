# 705 — One Later Sapporo Profile

**Status:** done
**Phase:** 7
**Dependencies:** 700

## Goal

Add exactly one evidence-eligible later Sapporo profile and reach its bounded
native reset stop without changing existing 2.22 behavior.

## Execution Budget

One model-day per version; dispatch a separate instance for 2.33, 2.35, or 2.39.

## Required Reading

Ticket 700 handoff for the selected version, profile grammar, board registry,
and Sapporo 2.22 profile/integration tests.

## Current Baseline

The profile registry knows only `sapporo-2.22.60`. Board creation currently
recognizes that exact ID.

## Allowed Files

One `profiles/sapporo/<version>/**` directory, profile registry, board registry
integration, and one version-specific integration test.

## Frozen Interfaces

Manifest grammar, Sapporo physical wiring proven identical by evidence, and
all 2.22 behavior remain unchanged. Version-specific differences are explicit.

## Evidence Inputs

Exact selected-version component and memory evidence IDs from ticket 700. No
entry means this ticket stays blocked.

## Implementation

Add metadata, CLI listing/selection, strict validation, and bounded reset run.
Stop at the first unsupported observed behavior; do not implement it here.

## Tests and Commands

`make test TEST_FILTER=sapporo_profile_<version>` selects one test and exits 0
for valid/wrong-hash/wrong-version cases. `make test-firmware
FIRMWARE_ROOT="$FIRMWARE_ROOT" TEST_PROFILE=<id>` validates and runs only its
declared bounded reset check. Run all 2.22 tests and `make check`.

## Acceptance

Exact inputs reach the recorded stop deterministically; invalid inputs fail
before mapping; no 2.22 checkpoint changes.

## Forbidden Scope

No compatibility behavior, new devices, several versions in one dispatch, or
inherited unproven wiring.

## Handoff

Report selected version, files, evidence, stop/PC/time, two-run hash, and next
unsupported observation.
