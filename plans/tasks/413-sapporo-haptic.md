# 413 — Sapporo Haptic PMIC

**Status:** done
**Phase:** 4
**Dependencies:** 285, 295, 298

## Goal

Implement verified haptic PMIC trigger/status/reset transitions and observer events. This supplies feedback startup traffic to 420; host vibration and invented waveforms remain deferred.

## Execution Budget

One agent-day. Implement only the startup and feedback status transitions proven for the haptic PMIC.

## Required Reading

`src/devices/sapporo_devices.c` haptic branch, `SapporoHapticPmic.cs:Write/Read/FinishTransmission/Reset`, `docs/research/feedback-startup-haptic.md`, and any startup feedback transcript listed by 295.

## Current Baseline

The aggregate marks all 256 registers readable/writable, initializes `0x22=0x02`, and converts write `0x22=1` immediately back to `2`. Address `0x50`, masks, timing, command lifecycle, and refusal behavior are otherwise guesses.

## Allowed Files

Only `src/devices/{sapporo_haptic.c,sapporo_haptic.h}` and `tests/devices/test_sapporo_haptic.c`.

## Frozen Interfaces

Opaque lifecycle, typed I2C endpoint, and optional haptic-state observer. Address is wiring data. Busy/complete transitions use scheduler only when trace gives a delay; otherwise same-time behavior must be explicitly evidenced.

## Evidence Inputs

`E-SAP-HAPTIC-001` must cite all C# lifecycle methods and include startup/trigger/status/reset/unknown-command transaction hashes. The current `0x22` behavior cannot be promoted without that trace.

## Implementation

Implement explicit register masks and a minimal idle→active→complete state machine; validate whole write before observer/state mutation; reset cancels any scheduled completion.

## Tests and Commands

`make test TEST_FILTER=sapporo_haptic` runs only its binary and exits 0; startup, trigger/status timing, observer, reset-cancel, wrong address/register/value/length/state, and repeat event order pass. `make check` exits 0.

## Acceptance

Status transitions/timing match evidence; no wildcard register access; invalid command is atomic; output observer is deterministic and optional.

## Forbidden Scope

No audio/host vibration, invented waveform effects, all-register retention, IOM/timer implementation, board attachment, or aggregate edit.

## Handoff

Report command/status table, timing/observer events, evidence hash, and endpoint role for 420.
