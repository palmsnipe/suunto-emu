# 410 — Sapporo HSPPAD143 Pressure Sensor

**Status:** blocked
**Phase:** 4
**Dependencies:** 285, 295, 298

## Goal

Replace the permissive pressure branch with a strict HSPPAD143 endpoint and refusal tests. This supplies pressure startup traffic to 420; later pressure chips and random samples remain deferred.

## Execution Budget

One agent-day. Replace the permissive pressure branch with one strict I2C device.

## Required Reading

`src/devices/sapporo_devices.c:initialize_registers/register_transfer`, `tests/devices/test_sapporo_devices.c:test_pressure`, `SapporoHsppad143.cs:Write/Read/FinishTransmission/Reset`, and `docs/research/sapporo-2.22-startup-peripheral-map.md`.

## Current Baseline

Kind `SEMU_SAPPORO_PRESSURE` at address `0x48` returns ID `0x49`, ready `0x11`, value `0xe0`, but marks all 256 registers readable/writable. It has no selected-register lifecycle, reset/refusal coverage, timing, or provenance.

## Allowed Files

Only `src/devices/{sapporo_hsppad143.c,sapporo_hsppad143.h}` and `tests/devices/test_sapporo_hsppad143.c`.

## Frozen Interfaces

Opaque create/destroy/reset plus `semu_serial_endpoint`; I2C address is constructor wiring, not hardcoded device policy. Selected-register state and auto-increment follow evidence; unknown register/direction/length/state refuses before write.

## Evidence Inputs

`E-SAP-HSPPAD143-001` must cite the four C# methods and contain exact startup write/read/reset plus unknown-register trace. Register arrays inferred only from the permissive local model are forbidden.

## Implementation

Implement only traced identity/ready/sample/config fields, explicit masks/defaults, transaction framing, and reset. Fixed sample values must be labeled synthetic configuration, not physical measurements.

## Tests and Commands

`make test TEST_FILTER=sapporo_hsppad143` runs only its binary and exits 0; startup transcript, reset, wrong address/register/length/state, write mask, and repeated transcript pass. `make check` exits 0.

## Acceptance

Byte transcript and refusals match `E-SAP-HSPPAD143-001`; no wildcard readable/writable mask remains in the new module; reset is deterministic.

## Forbidden Scope

No LPS22/later firmware, random pressure, all-register storage, IOM logic, board attachment, guessed timing, or legacy aggregate edit.

## Handoff

Report supported registers/masks, synthetic fields, evidence hash, test output, and endpoint role for 420.
