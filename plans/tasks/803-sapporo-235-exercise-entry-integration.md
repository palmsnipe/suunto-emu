# 803 — Sapporo 2.35 Exercise Entry Integration

**Status:** blocked
**Phase:** 7
**Dependencies:** 705,779,794

## Goal

Support selecting Running from Exercise without replacing the OHR refusal with
a guest reset. This follows the owner's 2026-10-02 navigation recommendations.

## Execution Budget

One bounded protocol/CTIMER integration after the missing Timer8 evidence is
available and the saved routing state has an integrator-owned format decision.
Private probes, snapshots and logs stay outside Git.

## Required Reading

README, current status, index, architecture device ownership, execution-model
DMA/timer/snapshots, testing strategy, compatibility policy; E-SAP-0040,
E-SAP-0041-EXT/TAIL, E-A4-TIMER-001, E-EMU-SAP235-EXERCISE-001/002; all Allowed
Files plus src/devices/sapporo_ohr2.c, sapporo_iom4.c and
src/soc/apollo4/timer_internal.h.

## Current Baseline

Opening Exercise works. Upper at 38 s followed by Middle at 39 s from the
codec-2 watchface refuses command 4 / sequence 21, body byte 4 = a3.
Twice-reproduced external trials of evidenced OHR, haptic and PatternAddress
extensions then reset on CTIMER8 control 0x141 at 0x40008300. The lane stores
that control word but explicitly reports PWM mode unsupported. Immediate
readback alone was not a counter, output waveform or IRQ timing law. The
follow-up E-EMU-SAP235-EXERCISE-002 observes the advancing counter, no IRQ at
the tested compares, disable/clear behavior, three pulse configurations,
PatternAddress 0x12201 and retained output-routing offset 0xb4. Two external
trials now reach the first-exercise GPS tutorial at 44 s without resets or
draw refusals. These trials omit routing state from snapshots and approximate
counter timing; they are not a production implementation. The original OHR
refusal remains in production.

## Allowed Files

- src/compat/sapporo_235_ohr.c
- src/devices/sapporo_iom4_haptic.c
- src/soc/apollo4/timer.c
- src/soc/apollo4/timer_snapshot.c
- tests/devices/test_sapporo_235_ohr.c
- tests/devices/test_sapporo_iom4_haptic.c
- tests/devices/test_apollo4_timer_patterns.c
- tests/devices/test_apollo4_timer_snapshot.c
- tools/test_sdl_sapporo_235_nav.sh
- tools/test_sdl_sapporo_235_exercise.sh (new)
- docs/migration-evidence.md
- docs/current-status.md
- README.md

Planning setup owns this ticket and its index row. Existing timer snapshot
admission is explicitly integration-owned; the persistent layout is frozen.

## Frozen Interfaces

Public headers, profiles, registries, Makefile, CPU behavior, compatibility
budgets and unrelated goldens. Existing OHR prefix and poll-only paths remain
byte-identical; startup synthetic replies never become real measurements.

## Evidence Inputs

E-EMU-SAP235-EXERCISE-001 supplies the exact request and lane reply, two haptic
transactions, PatternAddress value 0x10201, and the unresolved Timer8 boundary.
E-EMU-SAP235-EXERCISE-002 adds paired timed counter/IRQ/readback observations
and a bounded native continuation. Its one-count discrepancies at 759 us and
one 100 ms sample must be explained before claiming an exact counter law.
Physical PWM output remains unsupported by the lane; reaching the tutorial
does not establish waveform or audio fidelity. No larger OHR budget is
authorized. Use twice-reproduced lane observations or authorized hash-pinned
offline RE for remaining behavior; no physical device will be acquired.

## Required Integration Change — 2026-10-02

The existing scope is insufficient: `timer_internal.h` has no 0xb4 latch and
`timer_snapshot.c` serializes none. Production accepts 0x10000000 at this
offset but returns zero and produces an identical 637-byte timer image; the
lane retains 0x10000000 and 0x3f000000 until rewritten/reset. Ignoring this
state, deriving it from control8, or hiding it in an unrelated field is not an
acceptable implementation.

The integrator must explicitly own the extra internal state, a versioned
Apollo4/machine snapshot representation, old-image refusal or lossless
migration policy, and affected snapshot-gate re-derivation before unblocking
this ticket. The timer is embedded without a length/version in
`src/soc/apollo4/apollo4_snapshot.c`; inserting bytes into its current codec
would misalign subsequent devices. Old images did not capture the latch, so
silently initializing it cannot claim lossless restoration. At minimum the
scope needs `src/soc/apollo4/timer_internal.h` and the chosen enclosing codec,
its tests and execution contract. Public interfaces/registries/Makefile stay
out of scope unless that explicit integration decision requires them.

All dependencies remain done. This ticket stays blocked; no Allowed Files or
persistent-layout exception is granted by this evidence update. Proposed
acceptance should distinguish CPU-visible timer behavior from unavailable PWM
output, and must retain the current fail-closed and finite-fixture constraints.

## Implementation

After evidence and the integration decision close the Timer8 gap, add only
the observed command/payload, register and timer-state admissions. Validate full operations before mutation,
keep snapshot admission consistent, and add success plus atomic refusal cases.
Pin a paired exercise-entry continuation with zero unexpected resets. Attribute
any changed old UPPER suffix before re-deriving that one pin.

## Tests and Commands

Red-first OHR/haptic/timer regressions; make check, make sanitize,
make check-lines, make check-task-contracts; all 2.35 firmware gates,
restored navigation, paired exercise continuation and original cold navigation.
Run affected era gates or report exact missing input and possible pin drift.

## Acceptance

No new guest reset in the bounded exercise-entry window; exact pinned input,
log, frame and snapshot pairs. Layer-off/wrong hash/state/payload/budget refuse.
Do not claim a recording session, real OHR data, GPS fix or working PWM without
its own evidence. This ticket cannot pass merely by admitting the OHR reply.

## Forbidden Scope

No generic zero reads, guessed PWM frequency, broad register fallback,
assertion bypass, global budget increase or silent era/golden replacement.

## Handoff

Report observation versus unsupported model behavior explicitly, complete
hashes/checkpoints and commands, next boundary, and requested integrator review.
Evidence/planning maintenance created and refined this blocked ticket; no
production implementation or acceptance is claimed. E-EMU-SAP235-EXERCISE-002
records the tutorial checkpoint and the remaining timing/persistence gaps.
