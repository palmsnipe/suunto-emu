# 730 — Tianjin, Rostock, and Xiamen Onboarding

**Status:** blocked
**Phase:** 7
**Dependencies:** 720

## Goal

Onboard Tianjin and Rostock as independent exact profiles, then Xiamen only after an exact firmware package and hardware contract exist.

## Allowed Files

New product profiles/boards/devices/tests, narrowly proven shared lower-layer extensions, registries, docs coverage/evidence, Makefile lists, `plans/index.tsv` status only.

## Frozen Interfaces

Apply the standard onboarding sequence and existing fail-closed interfaces. Every profile has exact hashes, isolated memory/wiring, named checkpoints, and product-specific input/display contracts.

## Evidence Inputs

Exact firmware manifests and per-product hardware/traffic ledger entries. Xiamen remains blocked if either exact firmware or hardware contract is absent.

## Implementation

For one product at a time: validate hashes, define memory/wiring, reach bounded reset fault, add observed hardware, obtain native display, then enable interaction. Split separate follow-up tickets if any product exceeds this bounded flow.

## Tests and Commands

`make check`; `make test TEST_FILTER=profiles`; `make test-firmware FIRMWARE_ROOT="$FIRMWARE_ROOT" TEST_PROFILE=tianjin`; repeat for Rostock and, only when eligible, Xiamen; run all prior product gates.

## Acceptance

Each supported profile reaches an independently documented deterministic display/input checkpoint with no unexpected fallback; absent Xiamen evidence leaves it explicitly unsupported rather than guessed; all prior tests pass.

## Forbidden Scope

No placeholder exact hashes, generic “later model” board, copying another product's wiring, compatibility as hardware discovery, partial Xiamen claim, or relaxing existing failures.

## Handoff

Report a product-by-product evidence/readiness matrix, precise unsupported reasons, and future task splits.

