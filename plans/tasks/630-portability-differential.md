# 630 — Portability and Optional Differential Gate

**Status:** blocked
**Phase:** 6
**Dependencies:** 625

## Goal

Run the mandatory compiler/sanitizer matrix and an optional Renode comparison
over committed synthetic CPU vectors without making Renode authoritative.

## Execution Budget

One to two model-days; build scripts, vector adapter, and narrow regressions.

## Required Reading

`Makefile`, `AGENTS.md`, CPU vector manifests, `tools/` portability conventions,
and ticket 625 handoff.

## Current Baseline

Clang/GCC `make check`, SDL dummy smoke, and ASan/UBSan pass locally. The
Makefile exposes a dependency-free optional `test-differential` hook but no
Renode adapter until this ticket.

## Allowed Files

`tools/differential/**`, `tests/**`, `Makefile`, docs coverage/evidence updates,
and narrow fixes with focused regressions.

## Frozen Interfaces

Committed expected CPU results remain authoritative. Renode output is normalized
and compared; it never overwrites expected vectors.

## Evidence Inputs

Synthetic CPU vectors and compiler diagnostics. Renode is caller-provided and
optional.

## Implementation

Add adapter and stable mismatch report, then run macOS/Linux-capable build
commands. Document actual skips without treating missing optional tools as pass
evidence.

## Tests and Commands

`CC=clang make clean check`; `CC=gcc make clean check`; `make sanitize`;
`make check-sdl` when SDL3 exists; `make test-differential` exits 0 with an
explicit skip when `RENODE` is absent; `make test-differential RENODE="$RENODE"`
runs all vectors when supplied; `git diff --check` exits 0.

## Acceptance

Mandatory available matrix has no warnings/errors/sanitizer findings; optional
mismatches are stable and investigated through regressions; line limits remain.

## Forbidden Scope

No Renode runtime dependency, downloads, auto-updated expectations, random
fuzzing, broad suppressions, or private firmware.

## Handoff

Report OS/compiler/SDL/sanitizer matrix, skips, differential vector count,
mismatches, and regression IDs.
