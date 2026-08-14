# 100 — Utilities, Formats, and Test Harness

**Status:** done
**Phase:** 1
**Dependencies:** 000

## Goal

Establish the portable build, dependency-free C99 test harness, allocation/error/endian helpers, CRC32, SHA-256, and strict profile/firmware manifest parsing.

## Execution Budget

Completed Phase 1 ticket; no remaining implementation is delegated here.

## Required Reading

`include/semu/{types,hash,manifest}.h`, `src/core/error.c`,
`src/formats/{hash,manifest,manifest_validate}.c`, `tests/support/**`,
`tests/unit/{test_hash,test_manifest}.c`, and `docs/profile-format.md`.

## Current Baseline

The listed source and tests implement the accepted contract. `semu_error_set`,
CRC32/SHA-256, strict profile/manifest parsing, safe relative paths, and
pre-map byte validation are present. Parser implementation remains below the
500-line hard limit but above the review threshold and should be split before
material expansion.

## Allowed Files

`Makefile`, `include/semu/{types,hash,manifest}.h`, `src/core/error.c`,
`src/formats/**`, `tests/support/**`, and the matching unit tests.

## Frozen Interfaces

INI grammar is ASCII sections, unique `key=value`, `#` comments, decimal/`0x`
integers, and no unknown keys. Hash formatting returns lowercase 64-character
SHA-256 hex. Frozen declarations are the current public headers; source/line
diagnostics are a future enhancement, not an undocumented acceptance claim.

## Evidence Inputs

Approved build contract and `docs/profile-format.md`; no firmware content required.

## Implementation

Add `make`, `test`, and `check` foundations; make the parser schema-driven so profile and firmware keys remain distinct; reject duplicates, overflow, malformed ASCII, unsafe component paths, and incomplete records.

## Tests and Commands

`make clean && make` exits 0 without SDL linkage; `make test
TEST_FILTER=formats` selects `test_hash` and `test_manifest`; `make test
TEST_FILTER=hash` selects `test_hash`; `make check-lines`; `CC=clang make check`;
and `CC=gcc make check` where available all exit 0.

## Acceptance

Known CRC32/SHA-256 vectors pass; malformed and duplicate manifests fail with stable diagnostics; default build has no SDL link; GNU Make 3.81-compatible syntax is used.

## Forbidden Scope

No bus, scheduler, CPU, firmware loader mapping, SDL, permissive unknown keys, path traversal, or external hash/test libraries.

## Handoff

Report public declarations, parser schemas, command results, and portability gaps.
