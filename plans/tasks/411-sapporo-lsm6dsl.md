# 411 — Sapporo LSM6DSL Motion Sensor

**Status:** blocked
**Phase:** 4
**Dependencies:** 285, 295, 298

## Goal

Implement verified LSM6DSL SPI transaction/register state and deterministic samples. This supplies motion startup traffic to 420; LSM6DSOX and inferred sensor-hub behavior remain deferred.

## Execution Budget

Two agent-days. Implement the observed SPI command/register/transaction lifecycle and deterministic motion fixture.

## Required Reading

`src/devices/sapporo_devices.c:register_transfer`, `test_sapporo_devices.c:test_accelerometer`, `SapporoLsm6Dsl.cs:Transmit/FinishTransmission/EndNativeTransaction/Reset`, `docs/research/iom0-lsm6dsl-startup.md`, and `emulator/renode/accelerometer/inspect-motion-dma.resc`.

## Current Baseline

The aggregate reports WHO_AM_I `0x6a`, marks all registers readable/writable, and treats any SPI read bit as valid. It does not model native command continuation, auto-increment, transaction end, configuration masks, samples, sensor-hub state, or IRQ.

## Allowed Files

Only `src/devices/{sapporo_lsm6dsl.c,sapporo_lsm6dsl.h}` and `tests/devices/test_sapporo_lsm6dsl.c`.

## Frozen Interfaces

Opaque lifecycle and typed SPI endpoint; explicit end-native-transaction and optional IRQ/sample injection callbacks from 298. Chip select is constructor wiring. Preserve command state across only the continuation forms proven by evidence.

## Evidence Inputs

`E-SAP-LSM6DSL-001` must cite all four C# methods and include byte-exact IOM0 PIO/DMA startup, transaction boundaries, supported registers/masks, and at least one rejected command trace. Block sample/IRQ behavior absent from evidence.

## Implementation

Split command framing, register state, and fixed fixture sampling if needed. Validate read/write direction, auto-increment range, and full payload before mutation; reset command and register state exactly.

## Tests and Commands

`make test TEST_FILTER=sapporo_lsm6dsl` runs only its binary and exits 0; WHO_AM_I, startup config, continued burst, end/reset, unknown register, wrong direction/length/chip-select, sample/IRQ if verified, and repeated transcript pass. `make check` exits 0.

## Acceptance

The IOM0 transcript matches; no all-register mask remains; continuation ends only at evidenced boundary; synthetic sample fields are labeled and deterministic.

## Forbidden Scope

No LSM6DSOX, sensor-hub compatibility guess, random/live motion, IOM/DMA logic, board wiring, all-register storage, or aggregate edit.

## Handoff

Report command/register coverage, boundary rule, evidence hashes, synthetic fields, and endpoint role for 420.
