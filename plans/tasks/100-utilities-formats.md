# 100 — Utilities, Formats, and Test Harness

**Status:** done
**Phase:** 1
**Dependencies:** 000

## Goal

Establish the portable build, dependency-free C99 test harness, allocation/error/endian helpers, CRC32, SHA-256, and strict profile/firmware manifest parsing.

## Allowed Files

`Makefile`, `include/semu/{error,endian,hash,manifest}.h`, `src/{core,formats}/**`, `tests/{support,unit}/**` limited to this functionality.

## Frozen Interfaces

INI grammar is ASCII sections, unique `key=value`, `#` comments, decimal/`0x` integers, no unknown keys. Parser results carry source/line diagnostics. Hash APIs are streaming and return lowercase 64-character SHA-256 hex when formatted.

## Evidence Inputs

Approved build contract and `docs/profile-format.md`; no firmware content required.

## Implementation

Add `make`, `test`, and `check` foundations; make the parser schema-driven so profile and firmware keys remain distinct; reject duplicates, overflow, malformed ASCII, unsafe component paths, and incomplete records.

## Tests and Commands

`make clean && make`; `make test TEST_FILTER=formats`; `make test TEST_FILTER=hash`; `make check-lines`; `CC=clang make check`; `CC=gcc make check` where available.

## Acceptance

Known CRC32/SHA-256 vectors pass; malformed and duplicate manifests fail with stable diagnostics; default build has no SDL link; GNU Make 3.81-compatible syntax is used.

## Forbidden Scope

No bus, scheduler, CPU, firmware loader mapping, SDL, permissive unknown keys, path traversal, or external hash/test libraries.

## Handoff

Report public declarations, parser schemas, command results, and portability gaps.
