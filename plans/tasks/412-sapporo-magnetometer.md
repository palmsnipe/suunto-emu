# 412 — Sapporo TLI493D-W2BW Magnetometer

**Status:** done
**Phase:** 4
**Dependencies:** 285, 295, 298

## Goal

Implement the strict TLI493D-W2BW startup/config/diagnostic I2C protocol. This supplies magnetometer traffic to 420; unverified sampling and calibration behavior remain deferred.

## Execution Budget

One to two agent-days. Implement the strict observed I2C register protocol.

## Required Reading

`src/devices/sapporo_devices.c` wrist-magnetometer branch, `SapporoTli493dW2bw.cs:Write/Read/FinishTransmission/Reset`, `docs/research/iom2-tli493d-w2bw-startup.md`, and related IOM2 trace artifacts.

## Current Baseline

The aggregate permits reads `0..22`, writes `0x10..0x11`, and returns diagnostic `0x44` at register 6. It has no proven reset image beyond that byte, transaction shape, data sample, configuration masks, or negative-length tests.

## Allowed Files

Only `src/devices/{sapporo_tli493d.c,sapporo_tli493d.h}` and `tests/devices/test_sapporo_tli493d.c`.

## Frozen Interfaces

Opaque lifecycle and typed I2C endpoint; address is constructor data; selected-register pointer follows evidence. Optional deterministic sample injection is accepted only if `E-SAP-TLI493D-001` defines the byte encoding.

## Evidence Inputs

`E-SAP-TLI493D-001` must cite the four C# methods and contain exact IOM2 startup/reset/config/diagnostic traces plus invalid-register evidence. Treat the current range masks as hypotheses until verified.

## Implementation

Declare each supported register/mask explicitly; perform atomic multi-byte validation; implement diagnostic/reset and sample/config transitions only from trace.

## Tests and Commands

`make test TEST_FILTER=sapporo_tli493d` runs only its binary and exits 0; startup, diagnostic `0x44` if verified, config write/readback, reset, wrong address/register/length/state, boundary 22/23, and repeat transcript pass. `make check` exits 0.

## Acceptance

All supported bytes cite evidence; unknown register 23 refuses; rejected writes do not change state; deterministic reset/transcript passes twice.

## Forbidden Scope

No random magnetic field, undocumented full range, IOM logic, calibration compatibility, board attachment, inferred IRQ, or aggregate edit.

## Handoff

Report register table, reset values, sample provenance, trace hash, tests, and endpoint role for 420.
