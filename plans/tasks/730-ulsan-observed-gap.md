# 730 — One Ulsan Observed Hardware Gap

**Status:** in-progress
**Phase:** 7
**Dependencies:** 725

## Goal

Implement one byte-exact observed Ulsan controller/device gap and advance to
the next bounded stop.

## Execution Budget

One to three model-days; split MSPI1, SDIO/eMMC, panel, crown, touch, pairing,
and BLE into separate instances.

## Required Reading

Ulsan reset handoff, exact failing transcript, relevant typed bus/controller
contract, and device guide.

## Current Baseline

The selected Ulsan profile stops at one known unsupported behavior. No listed
future component is implemented by implication.

## Allowed Files

One `src/soc/apollo4plus/` controller or `src/devices/` module, matching tests,
and one isolated board attachment integration file.

## Frozen Interfaces

Typed transactions return OK/WAIT/REFUSE atomically. WAIT schedules deterministic
completion. Unknown command/size/state refuses.

## Evidence Inputs

Exact transcript/evidence ID from 720/725. If native traffic does not require a
component, do not implement it.

## Implementation

Implement only the current gap with reset, success, refusal, bounds, timing and
IRQ coverage as applicable. Repeat ticket 730 for the next gap.

## Tests and Commands

`make test TEST_FILTER=ulsan_<gap>` selects focused positive/refusal tests and
exits 0. Private bounded run reaches a named next stop twice identically. Run
`make check-lines && make check && make sanitize` and legacy product suites.

## Acceptance

One gap is fully tested; the new stop is recorded; no unrelated device or
permissive fallback is added.

## Forbidden Scope

No combined controller/device epic, speculative BLE/pairing, or family defaults.

## Handoff

Report gap, evidence, transactions, timing/IRQ, next stop, and next ticket instance.

## Blocked-state audit note (2026-09-11, no-device constraint session)

Verified on disk (read-only audit; hashes spot-checked): several listed gap
candidates are already documented byte-level in
`/Users/cyril/projects/suunto-firmware` — JEDEC `c2 25 39` @
`0x000f44a2`/`0x00105894`, PIO bytes b7/35/af/05/06/01, CMD8→CMD1 `0xc0ff8080`,
data port `0x40070020` — and the audit marks BLE/pairing unobserved anywhere
yet derivable (class F). The stated blocker is therefore still valid as the
725 chain only. What remains per instance is in-repo implementation plus
read-only RE derivation; no instance is physical-observation-bound.
Status is left unchanged for integrator review.
