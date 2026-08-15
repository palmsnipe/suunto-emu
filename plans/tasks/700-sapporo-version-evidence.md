# 700 — Later Sapporo Evidence Inventory

**Status:** done
**Phase:** 7
**Dependencies:** 630

## Goal

Produce exact, non-copyrighted contracts for Sapporo 2.33, 2.35, and 2.39
before any profile or compatibility implementation.

## Execution Budget

One to three evidence days depending on locally available packages; no emulator
behavior changes.

## Required Reading

`AGENTS.md`, `docs/migration-evidence.md`, `docs/profile-format.md`, current
Sapporo profile, and extraction/traces in read-only `../suunto-firmware`.

## Current Baseline

Only Sapporo `2.22.60.3383-P` is hash-pinned. Later cache/logical-storage gaps
are roadmap hypotheses, not evidenced layer contracts.

## Allowed Files

`docs/{migration-evidence,hardware-coverage}.md`,
`fixtures/evidence/sapporo/**`, local ignored scripts under `tests/private/`.

## Frozen Interfaces

Each version inventory records product/version, every extracted component role,
size, load address and SHA-256, vector/SRAM/XIP contract, observed startup stop,
and trace hashes. No firmware bytes are committed.

## Evidence Inputs

Exact legally obtained packages. If any version is absent or ambiguous, record
it unavailable and do not unlock its profile ticket.

## Implementation

Validate extraction, compare contracts field-by-field with 2.22, capture bounded
reset/MMIO traces, and identify whether cache/logical-storage interventions are
real, synthetic, recovered, or still unknown.

## Tests and Commands

Run `make test-firmware FIRMWARE_ROOT="$FIRMWARE_ROOT" TEST_PROFILE=<id>` for
each available manifest; validation exits 0. Hash the bounded normalized traces
twice and require equality. `make check-lines && make check` exits 0.

## Acceptance

Every unlocked version has exact hashes and trace provenance; absent evidence
remains explicitly blocked; no compatibility layer is inferred by family.

## Forbidden Scope

No profiles, emulator code, firmware copies, guessed hashes, or wildcard layers.

## Handoff

Report eligible version IDs, evidence IDs/hashes, unavailable inputs, observed
differences, and the first failing native transaction per version.
