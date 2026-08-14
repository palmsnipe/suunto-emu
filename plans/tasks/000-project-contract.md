# 000 — Documentation and Project Contract

**Status:** done
**Phase:** 0
**Dependencies:** none

## Goal

Freeze the architecture, execution, compatibility, testing, evidence, hardware-status, and delegation rules needed for bounded implementation.

## Allowed Files

`docs/**`, `plans/**` only.

## Frozen Interfaces

The contracts in `docs/architecture.md` and the `.semu`/CLI/build contracts in the approved roadmap are inputs to later integration tickets. Later tickets may clarify them but cannot silently weaken fail-closed behavior.

## Evidence Inputs

Approved standalone-emulator plan and seed evidence `E-SAP-0001` through `E-COMPAT-0001`.

## Implementation

Create durable docs, a status index, a reusable template, and dependency-ordered tickets for phases 0-7.

## Tests and Commands

`find docs plans -type f -maxdepth 3 | sort`; `awk -F '\t' 'NR > 1 && NF != 6 { exit 1 }' plans/index.tsv`; `git diff --check -- docs plans`.

## Acceptance

All requested topics exist; every indexed ticket exists and declares dependencies, allowed files, frozen interfaces, evidence, commands, acceptance, and forbidden scope.

## Forbidden Scope

No emulator source, build files, firmware, external dependencies, or guessed device behavior.

## Handoff

Report documentation files, integration assumptions, and validation results.

