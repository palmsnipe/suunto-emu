# 410 — Sapporo Sensor and Power Devices

**Status:** blocked
**Phase:** 4
**Dependencies:** 400

## Goal

Model the observed pressure sensor, LSM6DSL, wrist magnetometer, haptic PMIC, ambient-light sensor, and fuel gauge as small deterministic physical devices.

## Allowed Files

`src/devices/{pressure,lsm6dsl,magnetometer,haptic,ambient_light,fuel_gauge}*`, matching `tests/devices/**`; no board registry/public headers.

## Frozen Interfaces

Use board-provided typed I2C/SPI attachment contexts from 400. Each device validates address, register, direction, length, and state before mutation. Samples are fixed board configuration or explicitly injected semantic sensor input.

## Evidence Inputs

Per-device ledger entries containing exact startup transcripts, reset observations, addresses/chip selects, and refusal cases migrated from `suunto-firmware`.

## Implementation

Implement only observed identity/config/status/sample/IRQ operations; separate protocol parsing from state where files approach 300 lines; make unobserved commands refuse.

## Tests and Commands

`make test TEST_FILTER=device_pressure`; `make test TEST_FILTER=device_imu`; `make test TEST_FILTER=device_power`; `make test TEST_FILTER=device_refusal`; `make check`.

## Acceptance

Every device passes reset, byte-exact startup, wrong command/length/state refusal, deterministic IRQ/sample, and repeated transcript tests; coverage matrix advances only with cited IDs.

## Forbidden Scope

No GPS, OHR, compatibility fixture, random/live host sensor data, permissive register arrays, board wiring changes, or invented commands needed only to continue boot.

## Handoff

Report per-device supported operations, tests, evidence IDs, and remaining refused native transactions.

