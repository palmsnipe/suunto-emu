# 710 — One Later Sapporo Observed Gap

**Status:** blocked
**Phase:** 7
**Dependencies:** 705

## Goal

Implement one observed later-Sapporo hardware/storage gap or one justified,
hash-pinned compatibility intervention.

## Execution Budget

One to two model-days per gap; one module and focused tests.

## Required Reading

Selected profile handoff, exact failing transcript/evidence entry, relevant
hardware/storage interface, and compatibility policy when applicable.

## Current Baseline

The selected profile stops fail-closed at one recorded behavior. No cache or
logical-storage layer is assumed to be required.

## Allowed Files

One new version-specific device/storage/compat module, its tests and fixture
provenance, plus integration attachment reserved by the selected profile ticket.

## Frozen Interfaces

Prefer real lower-level behavior. A layer must pin exact component hashes,
trigger/state/effect, provenance, and hit count; otherwise refuse.

## Evidence Inputs

Byte-exact transcript or recovered record entry from ticket 700/705. Missing or
ambiguous evidence blocks implementation.

## Implementation

Add one behavior, positive and refusal tests, and rerun until the next distinct
stop. Open a new instance of this ticket for each later gap.

## Tests and Commands

`make test TEST_FILTER=<selected_gap>` selects the focused test and exits 0;
wrong version/state/size/command refuses. Bounded `make test-firmware
FIRMWARE_ROOT="$FIRMWARE_ROOT" TEST_PROFILE=<id>` advances to the declared next
checkpoint twice identically. Run `make check-lines && make check && make sanitize`.

## Acceptance

Only the evidenced behavior changes, old/new profile regressions pass, and the
new stop is recorded without fallback widening.

## Forbidden Scope

No combined gaps, wildcard layers, guessed cache/storage state, or golden edits.

## Handoff

Report gap ID, implementation class, evidence, exact tests, layer hits if any,
next stop, and whether another instance is required.

## Blocked-state audit note (2026-07-08, no-device constraint session)

Verified on disk (read-only audit; hashes spot-checked): the later-Sapporo
material is present — private bundle `tests/private/sapporo-2.35.34.18929` and
`fixtures/evidence/sapporo/sapporo-2.35.contract.semu`; the audit found traces
blocked only for lack of a profile, and E-SAP-0017's un-recovered state is a
deeper-trace/disassembly job against those files, not a missing observation.
The stated blocker is therefore partially stale (class F + stale; dependency
705 is done). What remains — identifying one exact failing
transaction/record — is read-only RE derivation on files already on disk.
Status is left unchanged for integrator review.
