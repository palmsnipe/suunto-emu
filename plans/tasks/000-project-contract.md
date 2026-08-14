# 000 — Documentation and Project Contract

**Status:** done
**Phase:** 0
**Dependencies:** none

## Goal

Freeze the architecture, execution, compatibility, testing, evidence, hardware-status, and delegation rules needed for bounded implementation.

## Execution Budget

Completed scaffolding ticket; documentation and validation scripts only.

## Required Reading

Approved standalone-emulator plan and all files under `docs/` and `plans/`.

## Current Baseline

This root ticket created the original documentation set. Later plan hardening
added `AGENTS.md`, `plans/agent-prompt.md`, granular tickets, and executable
contract checks without reopening the implementation scope.

## Allowed Files

`AGENTS.md`, `docs/**`, `plans/**`, and task-validation scripts only.

## Frozen Interfaces

The contracts in `docs/architecture.md` and the `.semu`/CLI/build contracts in the approved roadmap are inputs to later integration tickets. Later tickets may clarify them but cannot silently weaken fail-closed behavior.

## Evidence Inputs

Approved standalone-emulator plan and seed evidence `E-SAP-0001` through `E-COMPAT-0001`.

## Implementation

Create durable docs, a status index, a reusable template, and dependency-ordered tickets for phases 0-7.

## Tests and Commands

`make check-task-contracts` prints the indexed ticket count and exits 0;
`make check-lines` exits 0; `git diff --check -- AGENTS.md docs plans` exits 0.

## Acceptance

All requested topics exist; every indexed ticket exists and declares dependencies, allowed files, frozen interfaces, evidence, commands, acceptance, and forbidden scope.

## Forbidden Scope

No emulator source, build files, firmware, external dependencies, or guessed device behavior.

## Handoff

Report documentation files, integration assumptions, and validation results.
