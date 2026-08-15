# 414 — Sapporo OPT3007 Ambient-Light Sensor

**Status:** done
**Phase:** 4
**Dependencies:** 285, 295, 298

## Goal

Implement verified OPT3007 configuration/result framing with deterministic encoded samples. This supplies ALS startup traffic to 420; live lux conversion and unproven timing remain deferred.

## Execution Budget

One agent-day. Implement observed OPT3007 configuration and deterministic sample reads.

## Required Reading

`src/devices/sapporo_devices.c` ambient-light branch, `SapporoOpt3007.cs:Write/Read/FinishTransmission/Reset`, `docs/research/iom3-opt3007-als.md` if present, and exact IOM3 traces recorded by 295.

## Current Baseline

The aggregate allows reads `0..3` and writes `2..3` at `0x45`, with all-zero reset bytes. It lacks proven register byte order, conversion-ready state, sample encoding, masks, timing, and refusal coverage.

## Allowed Files

Only `src/devices/{sapporo_opt3007.c,sapporo_opt3007.h}` and `tests/devices/test_sapporo_opt3007.c`.

## Frozen Interfaces

Opaque lifecycle and typed I2C endpoint; address is constructor wiring. Optional lux fixture injection accepts an already encoded evidenced register value, avoiding host floating-point conversion unless documentation proves it.

## Evidence Inputs

`E-SAP-OPT3007-001` must cite the four C# methods and exact startup/config/result/reset/unknown-register IOM3 traces. If the research file is absent or sample byte order is unproven, mark those operations missing and block them.

## Implementation

Use explicit 16-bit register byte order/masks from evidence, deterministic ready behavior, atomic config writes, and strict selected-register framing.

## Tests and Commands

`make test TEST_FILTER=sapporo_opt3007` runs only its binary and exits 0; startup config, result/ready, reset, wrong address/register/length/order, masked bits, and repeated transcript pass. `make check` exits 0.

## Acceptance

Config/result bytes match trace; unknown register refuses; no host time/random lux; rejected writes preserve previous configuration.

## Forbidden Scope

No approximate lux math, live host sensor, all-register storage, conversion timing guess, IOM logic, board attachment, or aggregate edit.

## Handoff

Report byte order/register masks, fixture encoding, evidence hash, tests, and endpoint role for 420.
