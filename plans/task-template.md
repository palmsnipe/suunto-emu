# NNN — Short Title

**Status:** `blocked | ready | in-progress | done`
**Phase:** N
**Dependencies:** exact ticket IDs or `none`

## Goal

One bounded outcome and the checkpoint it unlocks. State what is intentionally deferred.

## Allowed Files

List exclusive path globs. Public headers, Makefile, registries, profiles, and `plans/index.tsv` are integration-owned unless explicitly listed.

## Frozen Interfaces

Name the public headers, types, callbacks, manifest keys, CLI behavior, or trace formats consumed by this task. The implementer may not change them; interface gaps go back to the integrator.

## Evidence Inputs

List architecture references, evidence IDs from `docs/migration-evidence.md`, trace hashes, source symbols, and firmware hashes. Label hypotheses.

## Implementation

List only decisions required to complete the bounded goal. Keep hand-written C/header/test files below 500 lines and normally below 300.

## Tests and Commands

Give exact non-interactive commands. Every hang-prone test needs an instruction or virtual-time budget. Include positive and negative/refusal coverage.

## Acceptance

Use observable pass/fail conditions: tests, stop reason, hashes, transcript, zero source-image mutation, and documentation updates.

## Forbidden Scope

List adjacent features and shortcuts that must not be added, including permissive fallbacks and unrelated refactors.

## Handoff

Report changed files, commands/results, evidence used, deterministic artifacts, and unresolved gaps. The integrator updates status after review.

