# 755 — Wismar Reset Profile

**Status:** blocked
**Phase:** 7
**Dependencies:** 750

## Goal

Add the exact Wismar 2.46 profile and isolated board/SoC variant through its
evidenced bounded reset stop.

## Execution Budget

Two model-days.

## Required Reading

Ticket 750 handoff, board/profile registry, CPU NVIC contract, Apollo variants,
and all product regression tests.

## Current Baseline

No Wismar code window, SRAM alias, IRQ mask, or board exists.

## Allowed Files

Wismar profile/board files, necessary isolated Apollo variant mapping, registries,
and Wismar reset/profile tests.

## Frozen Interfaces

Core CPU/bus/profile APIs remain. Product-specific maps and wiring do not alter
other products.

## Evidence Inputs

All exact reset-contract evidence IDs from 750.

## Implementation

Map code/SRAM aliases, configure profile IRQ contract, add CLI selection, and
stop at the first unsupported Timer14/MSPI2/device behavior.

## Tests and Commands

`make test TEST_FILTER=wismar_reset` selects valid/invalid profile, alias, IRQ
mask, and map tests. Private run reaches its stop twice identically. Run
`make check-lines && make check && make sanitize` and all earlier product tests.

## Acceptance

Exact firmware reaches recorded stop; aliases are bounds-tested; old profiles
remain identical.

## Forbidden Scope

No Timer14/MSPI2 semantics, display, inferred shared wiring, or compatibility.

## Handoff

Report maps, evidence, stop/trace, regression results, and next gap.

## Blocked-state audit note (2026-09-11, no-device constraint session)

Verified on disk (read-only audit; hashes spot-checked): the Wismar package
(sha256 `c0ec9904…`) and extracted components
`/Users/cyril/projects/suunto-firmware/artifacts/generated/wismar-2.46.14/`
are present with the reset-relevant contracts documented (class F
conditional; the audit notes the Renode translator fault is Renode-specific —
test in-tree, do not assume). The stated blocker is therefore still valid as
the 750 chain only; once 750's stale premise is corrected, nothing physical
stands between that registration and this profile.
Status is left unchanged for integrator review.
