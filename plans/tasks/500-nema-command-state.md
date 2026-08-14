# 500 — Nema Command Validation and State

**Status:** blocked
**Phase:** 5
**Dependencies:** 420

## Goal

Parse and validate only observed Sapporo Nema commands, producing deterministic rendering operations and explicit unsupported-command diagnostics.

## Allowed Files

`include/semu/display.h`, `src/display/{nema_command,nema_state,texture,display_diag}*`, `tests/unit/display_{command,state,texture}*`, Makefile source lists.

## Frozen Interfaces

Input is the raw panel/GPU command stream from 400. Output is a checked operation stream over explicit state/texture handles; frame publication uses the machine frame callback. An unsupported form stops before partial command effects and records bounded command context.

## Evidence Inputs

Command traces captured during exact Sapporo run, added to the ledger by hash and decoded observation; `E-SAP-0002` through `E-SAP-0004` are later frame gates, not parser evidence.

## Implementation

Split framing, opcode validation, state transitions, texture descriptors/uploads, and diagnostic capture. Validate arithmetic/ranges before allocation or memory access.

## Tests and Commands

`make test TEST_FILTER=nema_command`; `make test TEST_FILTER=nema_state`; `make test TEST_FILTER=nema_refusal`; `make check`.

## Acceptance

Every observed command form has a byte-exact positive test; truncated/unknown/wrong-state/range cases fail atomically with deterministic diagnostics; public display operation contract is frozen for raster work.

## Forbidden Scope

No raster pixels, SDL, guessed unobserved opcodes, raw command-as-host-pointer behavior, shader/external library, or accepting malformed commands to preserve boot.

## Handoff

Report command coverage, trace evidence IDs, operation contract, and refused forms.

