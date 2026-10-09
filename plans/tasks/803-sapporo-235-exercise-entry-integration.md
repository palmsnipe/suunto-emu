# 803 — Sapporo 2.35 Exercise Entry Integration

**Status:** done
**Phase:** 7
**Dependencies:** 705,779,794

## Goal

Support selecting Running from Exercise without replacing the OHR refusal with
a guest reset. This follows the owner's 2026-10-02 navigation recommendations.

## Execution Budget

One bounded protocol/CTIMER integration using E-EMU-SAP235-TIMER8-003 and
the versioned saved-state decision below.
Private probes, snapshots and logs stay outside Git.

## Required Reading

README, current status, index, architecture device ownership, execution-model
DMA/timer/snapshots, testing strategy, compatibility policy; E-SAP-0040,
E-SAP-0041-EXT/TAIL, E-A4-TIMER-001, E-EMU-SAP235-EXERCISE-001/002, E-EMU-SAP235-TIMER8-003; all Allowed
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
- src/soc/apollo4/timer_internal.h
- src/soc/apollo4/timer8.c (new)
- src/soc/apollo4/apollo4_snapshot.c
- tests/devices/test_apollo4_timer8.c (new)
- tests/devices/test_apollo4_snapshot.c
- tools/test_sdl_snapshot_restore.sh
- tools/test_sdl_sapporo_235_restore.sh
- tools/test_sdl_sapporo_235_navigation_restore.sh
- tests/integration/test_firmware_sapporo_235_snapshot.sh
- docs/execution-model.md
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
admission and the Apollo4 codec transition are explicitly integration-owned.

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

## Authorized Integration Decision — 2026-10-02

The owner approved continuing with the recommended state/codec integration.
Persist offset 0xb4, Timer8 fractional phase and limit-stalled state. Prefix
Apollo4 section payload with little-endian A4SC magic and codec version 1;
retain outer machine version 2 and renderer codec 2. Reject old unversioned
SoC images and unknown versions with a recreate-snapshot diagnostic: old images
lost the routing latch and cannot be migrated losslessly. Add success and
atomic refusal coverage and re-derive only affected snapshot pins, comparing
all other sections/logs against the old images. Keep public APIs unchanged.

E-EMU-SAP235-TIMER8-003 resolves the counter discrepancy with 45 paired lane
observations, including fractional compare boundaries, enable transitions,
compare rewrites and wraparound. Support this CPU-visible law without claiming
physical PWM waveform support. Dependencies 705, 779 and 794 are all done.
The integrator owns this planning-only transition to ready; implementation
must leave status unchanged for review.

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
Report the production checkpoint and compare it with the external trial in
E-EMU-SAP235-EXERCISE-002; do not infer completion from that trial.

Integrator review (2026-10-09): acceptance verified — the exercise entry reaches the first-exercise GPS tutorial with deterministic save/restore equal to the uninterrupted state, and the Apollo4 codec 1 section-5 change is attributed (E-EMU-SAP235-EXERCISE-003); the versioned-snapshot concern from the blocked-state note was resolved by the codec landing as designed. Status done.
