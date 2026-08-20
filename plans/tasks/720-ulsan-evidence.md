# 720 — Ulsan 2.35/2.44 Evidence Inventory

**Status:** blocked
**Phase:** 7
**Dependencies:** 630

## Goal

Establish independent Ulsan 2.35 and 2.44 firmware, Apollo4 Plus, wiring,
display, input, storage, and startup-trace contracts.

## Execution Budget

Up to three evidence days; no emulator implementation.

## Required Reading

Evidence ledger procedure, profile grammar, Apollo4/Sapporo contracts, and
read-only Ulsan research/traces in `../suunto-firmware`.

## Current Baseline

Phase 6 supplies the independent evidence/profile tooling; later-Sapporo
release work is not a prerequisite. No Ulsan profile or verified sharing
contract exists. MSPI1, SDIO/eMMC,
466x466 NemaDC, crown, touch, pairing, and BLE are hypotheses until traced.

## Allowed Files

`docs/{migration-evidence,hardware-coverage}.md`, `fixtures/evidence/ulsan/**`,
local ignored evidence scripts.

## Frozen Interfaces

Record exact component hashes/loads, memory aliases, IRQ mask/priorities,
controller transactions, 466x466 frame contract, crown/touch signals, and first
bounded stop for each version. Sharing needs field-by-field proof.

## Evidence Inputs

Exact packages and traces. Missing packages block that version. Do not assume
pairing/BLE is needed until native traffic proves it.

## Implementation

Extract/hash locally, capture bounded traffic, identify controller/device
attachments and version differences, and add evidence entries only.

## Tests and Commands

Validate each available local manifest with the strict parser; capture two
bounded reset traces and compare normalized hashes. `make check-lines && make
check` exits 0.

## Acceptance

At least one version has a complete independent reset contract before 725 can
start; every unknown is explicit and no firmware bytes are tracked.

## Forbidden Scope

No code/profile, firmware copies, family inheritance, or speculative BLE.

## Handoff

Report eligible versions, evidence IDs, exact contract, trace hashes, sharing
proof, missing packages, and first unsupported behavior.
