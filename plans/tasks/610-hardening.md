# 610 — Sanitizers and Optional Differential Hardening

**Status:** blocked
**Phase:** 6
**Dependencies:** 600

## Goal

Run portability, sanitizer, fuzz-like bounded corpus, and optional differential CPU checks; convert every defect found into a focused regression.

## Allowed Files

`tests/**`, `fixtures/synthetic/**`, `tools/differential/**`, Makefile check/sanitize targets, narrow source fixes with matching regression, docs evidence/coverage updates, `plans/index.tsv` status only.

## Frozen Interfaces

Normal tests remain dependency-free. Renode comparison is opt-in, uses a caller-provided executable, consumes committed synthetic vectors only, and never changes authoritative expected results automatically.

## Evidence Inputs

All prior test suites; compiler/sanitizer diagnostics; optional Renode CPU observations labeled as differential rather than normative evidence.

## Implementation

Exercise parsers/decoders with deterministic malformed corpora; audit arithmetic/ranges/lifetimes; run Clang/GCC builds and supported ASan/UBSan; compare selected CPU vectors when Renode is configured.

## Tests and Commands

`CC=clang make clean check`; `CC=gcc make clean check`; `make sanitize`; `make test TEST_FILTER=malformed_corpus`; `make test-differential RENODE="$RENODE"` when available; `git diff --check`.

## Acceptance

Mandatory matrix passes without sanitizer findings or hangs; absent optional tools skip clearly; each fixed defect has a focused test; source line limits and first-release goldens remain unchanged.

## Forbidden Scope

No Renode runtime requirement, downloading tools, randomized non-reproducible fuzzing, suppressing real sanitizer findings, broad refactor without regression, or updating authoritative vectors from differential output.

## Handoff

Report compiler/OS matrix, skips, sanitizer results, differential mismatches, and regression IDs.

