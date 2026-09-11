# 760 — One Wismar Observed Gap

**Status:** blocked
**Phase:** 7
**Dependencies:** 755

## Goal

Implement one evidenced Wismar Timer14, MSPI2, device, or panel gap and advance
to the next bounded stop.

## Execution Budget

One to two model-days per gap; repeat this ticket for each distinct behavior.

## Required Reading

Wismar reset handoff, exact failing transcript, and relevant controller/device
contract and tests.

## Current Baseline

Wismar stops fail-closed at one recorded behavior. Named quirks are not yet
implemented unless a prior instance completed them.

## Allowed Files

One Wismar-specific controller/device module, focused tests, and isolated board
attachment integration.

## Frozen Interfaces

Typed transaction and scheduler/IRQ semantics remain deterministic and atomic.

## Evidence Inputs

Byte-exact Wismar evidence from 750/755. Missing evidence blocks the instance.

## Implementation

Add one behavior with reset/success/refusal/bounds/timing/IRQ tests; record the
new stop. Create another 760 instance rather than expanding scope.

## Tests and Commands

`make test TEST_FILTER=wismar_<gap>` selects focused tests and exits 0. Private
bounded run advances twice identically. Run `make check-lines && make check &&
make sanitize` and all earlier product suites.

## Acceptance

One gap is complete and narrow; new stop/evidence recorded; regressions pass.

## Forbidden Scope

No multiple gaps, guessed Timer14/MSPI2 behavior, fallback widening, or display
golden without evidence.

## Handoff

Report behavior, evidence, tests, new stop/hash, and next instance.

## Blocked-state audit note (2026-07-08, no-device constraint session)

Verified on disk (read-only audit; hashes spot-checked): the named controller
contracts (Timer14 edge-mode, MSPI2 `0x9f`→`20 bb 19`, `0xf0` mask) are
already documented under `/Users/cyril/projects/suunto-firmware` (class F; the
panel is the not-evidenced part — no Wismar display descriptor has ever been
observed). The stated blocker is therefore still valid per instance as the
755 chain. What remains for panel gaps is new read-only RE tracing; no
instance is physical-observation-bound.
Status is left unchanged for integrator review.
