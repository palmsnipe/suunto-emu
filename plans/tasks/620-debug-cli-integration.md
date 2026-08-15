# 620 — Debugging CLI Integration

**Status:** done
**Phase:** 6
**Dependencies:** 605,610,615

## Goal

Expose bounded trace, report, replay, and snapshot operations through stable
headless CLI options without changing normal run behavior.

## Execution Budget

One to two model-days; frontend glue and CLI integration tests only.

## Required Reading

`src/frontends/cli.c`, `include/semu/{machine,trace}.h`, tickets 605/610/615
handoffs, and CLI contract in `docs/architecture.md`.

## Current Baseline

`--trace` opens the readable log only. Replay and snapshot options do not exist;
fault output is one summary line.

## Allowed Files

`src/frontends/cli*.c`, `tests/integration/test_debug_cli.c`, `Makefile` source
integration, and CLI documentation corrections.

## Frozen Interfaces

Core formats and semantics from dependency tickets cannot change. All output
paths are host-side artifacts opened only after guest inputs validate.

## Evidence Inputs

Synthetic fixtures and dependency handoffs only.

## Implementation

Add explicit bounded trace configuration, report destination, replay input,
snapshot load/save, incompatible-option diagnostics, and stable exit codes.

## Tests and Commands

`make test TEST_FILTER=debug_cli` selects `test_debug_cli` and exits 0 for each
operation plus invalid option combinations. `make check` retains old CLI smoke
behavior. Run `make check-lines && make check && make sanitize`.

## Acceptance

All operations are bounded, invalid combinations fail before execution, and a
normal run summary is unchanged when no debug option is supplied.

## Forbidden Scope

No network debugger, UI, source mutation, implicit output, or format changes.

## Handoff

Report options, exit codes, artifact paths, test results, and remaining UI gaps.
