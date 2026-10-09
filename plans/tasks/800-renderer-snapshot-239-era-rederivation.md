# 800 — Renderer Snapshot 2.39 Era Re-derivation

**Status:** done
**Phase:** 7
**Dependencies:** 799

## Goal

Re-derive affected 2.39 snapshot pins after renderer codec 2, without changing
execution or rasterization expectations. The verified full-flash input is
currently unavailable; do not report an era pass from profile validation.

## Execution Budget

One private-fixture census, with two bounded identical runs per changed pin.

## Required Reading

Ticket 799, AGENTS.md, README.md, current status, execution/testing contracts,
E-SAP239-REPINSWEEP-002, E-EMU-SAP235-TICKTRAIL-002, and each affected runner
and inspector before editing.

## Current Baseline

Codec 2 deliberately changes serialized renderer bytes. The 43-script era
suite requires SEMU_SAPPORO_239_FULL_FLASH with SHA-256
37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb.
No replacement fixture or expected hash may be guessed.

## Allowed Files

`tests/integration/test_firmware_sapporo_239_*.sh`, associated existing
`tests/support/sapporo_239_*.c` inspectors only for their snapshot hash pins,
`docs/current-status.md`, `docs/migration-evidence.md`.

## Frozen Interfaces

All runtime behavior, profiles, APIs and snapshot encoding are frozen.

## Evidence Inputs

Ticket 799's codec derivation and the era suite's existing checkpoint laws.

## Implementation

Once dependencies are reviewed done and the exact private fixture exists,
run the affected scripts twice. Attribute changes to renderer serialization,
record both old/new hashes and raw-log hashes, and update only proven pins.

## Tests and Commands

`make check-era`, twice, plus `make check-task-contracts` and `make check`.
Every runner keeps explicit instruction/time bounds and fixture validation.

## Acceptance

43/43 era scripts pass, paired deterministic captures agree, and every moved
pin is attributed. Missing private evidence leaves this ticket blocked.

## Forbidden Scope

No fabricated full-flash fixture, relaxed stop expectations, unbounded fixture
extensions, runtime fixes, or changes to unobserved snapshot hashes.

## Handoff

Report per-script results, changed pins, complete derived census and evidence
hashes. Do not change this ticket's status during implementation.

Integrator review (2026-10-09): acceptance verified — the fixture was rebuilt
byte-exactly (37134845…), all 28 red scripts re-derived with two byte-identical
runs per moved pin, 33 pins across 28 runners plus three probe literals moved
with full attribution, and make check-era passes 43/43 twice back-to-back
(E-SAP239-SNAPSHOT-REPIN-003). Status done; the roadmap credit belongs to the
implementation commit db87bdc.
