# 745 — Ulsan Version Suite Gate

**Status:** blocked
**Phase:** 7
**Dependencies:** 740

## Goal

Require every evidence-eligible Ulsan version from ticket 720 to complete its
own reset, gap, and display instances before Wismar onboarding begins.

## Execution Budget

One integration day; test/registry audit only.

## Required Reading

Ticket 720 eligibility list, every Ulsan 725/730/740 handoff, Ulsan profile
registry, goldens, replay metadata, and legacy product release records.

## Current Baseline

Template tickets can be instantiated separately for 2.35 and 2.44; the static
graph alone cannot prove both eligible versions finished.

## Allowed Files

Ulsan suite integration tests, profile-index integration, normalized suite
metadata, and coverage status.

## Frozen Interfaces

No guest-visible behavior changes. This gate audits registrations, identities,
goldens, compatibility pins, and full regression results.

## Evidence Inputs

Exact eligible list from 720 and one release handoff per eligible version.

## Implementation

Enumerate required versions, reject absent/duplicate/wildcard contracts, run
each private release twice, and compare a normalized suite record.

## Tests and Commands

The instantiated gate lists exact profile IDs. Run `make test-firmware
FIRMWARE_ROOT="$FIRMWARE_ROOT" TEST_PROFILE=<id>` twice for each, `make test
TEST_FILTER=ulsan_profiles`, every Sapporo release test, `make check-sdl`,
`make check`, and `make sanitize`; all exit 0.

## Acceptance

Every eligible Ulsan version has an independent exact release record, both
runs match, and all Sapporo records remain unchanged.

## Forbidden Scope

No angle-bracket dispatch, new behavior, skipped eligible version, shared
wildcard layer, Wismar code, or golden edits.

## Handoff

Report exact IDs, suite hash, per-version results, legacy results, and Wismar readiness.
