# 750 — Wismar 2.46 Evidence Inventory

**Status:** blocked
**Phase:** 7
**Dependencies:** 630

## Goal

Pin Wismar 2.46 firmware, code window, `0xf0` NVIC mask, SRAM alias, Timer14,
MSPI2, wiring, startup, and display contracts independently.

## Execution Budget

Up to three evidence days; no emulator code.

## Required Reading

Evidence procedure, Apollo4 coverage, Wismar research/traces in read-only
`../suunto-firmware`, and prior product onboarding handoffs.

## Current Baseline

Phase 6 supplies the independent evidence/profile tooling; Ulsan release work
is not a prerequisite. No Wismar profile or verified behavior exists; all
named quirks are roadmap leads requiring exact observations.

## Allowed Files

`docs/{migration-evidence,hardware-coverage}.md`, `fixtures/evidence/wismar/**`,
and ignored local extraction/trace scripts.

## Frozen Interfaces

Record exact components/hashes/loads, aliases, IRQ mask, Timer14/MSPI2
transactions, board wiring, reset stop, display stream and frame hashes.

## Evidence Inputs

Exact 2.46 package and bounded traces. Absence blocks further Wismar tickets.

## Implementation

Hash components, capture two normalized reset traces, prove each special
contract, and label every unknown rather than inheriting Ulsan/Sapporo behavior.

## Tests and Commands

Strict local manifest validation exits 0; two bounded trace hashes match;
`make check-lines && make check` passes.

## Acceptance

Complete non-copyrighted reset contract and first unsupported transaction are
recorded with evidence IDs.

## Forbidden Scope

No code/profile, firmware bytes, guessed quirk semantics, or product inheritance.

## Handoff

Report hashes/contracts/evidence, trace hash, unknowns, and first gap.

## Blocked-state audit note (2026-07-08, no-device constraint session)

Verified on disk (read-only audit; hashes spot-checked), FW =
`/Users/cyril/projects/suunto-firmware`: the package-absence premise is stale —
`FW/firmware/catalog.json`, `FW/firmware/known-packages.json`, and
`FW/emulator/devices/registry.json` all claim no Wismar package, yet
`Wismar-fw_2.46.14.26212-U` (sha256 `c0ec9904…`, matching
`external-package-candidates.json`) and the extracted components
`FW/artifacts/generated/wismar-2.46.14/` have been on disk since Aug 12;
intake must hash files, not trust registries. Documented there already: `0xf0`
NVIC mask, SRAM alias `0x2000_0000`, Timer14 edge-mode, MSPI2 `0x9f`→
`20 bb 19`, MAX20360 IOM4. The stated blocker is therefore stale. What
remains — a display-submission trace and the `0x00534900` UNDEFINSTR boundary —
is read-only RE derivation.
Status is left unchanged for integrator review.
