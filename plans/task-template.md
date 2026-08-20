# NNN — Short Title

**Status:** `blocked | ready | in-progress | done`
**Phase:** N
**Dependencies:** exact ticket IDs or `none`

## Goal

One bounded outcome and the checkpoint it unlocks. State what is intentionally deferred.

## Execution Budget

Target one to three model-days. State the maximum expected handwritten files,
tests, and fixture size. If larger, split the ticket before dispatch.

## Required Reading

List exact docs, public headers, source files, dependency handoffs, and evidence
ledger IDs. Architecture references include document title/version and section.

## Current Baseline

List existing files, functions, tests, and known deficiencies. Say whether code
must be extended, split mechanically first, or created. Existing code never
implies acceptance.

## Allowed Files

List exclusive path globs. Public headers, Makefile, registries, profiles, and `plans/index.tsv` are integration-owned unless explicitly listed.

## Frozen Interfaces

Name the public headers, types, callbacks, manifest keys, CLI behavior, or trace formats consumed by this task. The implementer may not change them; interface gaps go back to the integrator.

## Evidence Inputs

List architecture references, evidence IDs from `docs/migration-evidence.md`, trace hashes, source symbols, and firmware hashes. Label hypotheses.

If a required entry or trace is absent, state the exact stop condition and the
evidence ticket that must complete first. “Inspect prior emulator” is not an
adequate evidence input.

## Implementation

List only decisions required to complete the bounded goal. New hand-written
C/header/test files should remain below 300 lines and every hand-written file
must remain at or below 500. Avoid materially growing existing files above the
review threshold without splitting by responsibility.

## Tests and Commands

Give exact non-interactive commands. Every hang-prone test needs an instruction or virtual-time budget. Include positive and negative/refusal coverage.

For each command, state the expected selected test binary, exit status, stop
reason, checkpoint, hash, or diagnostic. Do not name a Make target or variable
that does not exist in the current `Makefile`.

## Acceptance

Use observable pass/fail conditions: tests, stop reason, hashes, transcript, zero source-image mutation, and documentation updates.

## Forbidden Scope

List adjacent features and shortcuts that must not be added, including permissive fallbacks and unrelated refactors.

## Handoff

Report changed files, commands/results, evidence used, deterministic artifacts, and unresolved gaps. The integrator updates status after review.
