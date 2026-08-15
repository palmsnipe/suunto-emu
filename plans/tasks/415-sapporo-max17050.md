# 415 — Sapporo MAX17050 Fuel Gauge

**Status:** done
**Phase:** 4
**Dependencies:** 285, 295, 298

## Goal

Extract verified MAX17050 Status/VCell behavior into a standalone endpoint. This unlocks the fuel-gauge startup checkpoint in 420; host battery simulation and persistence compatibility remain deferred.

## Execution Budget

One to two agent-days. Implement verified MAX17050 register protocol and deterministic battery fixture.

## Required Reading

`SapporoApollo4Iom4.cs:SapporoMax17050`, `Apollo4IomDma.cs:Max17050`, `emulator/renode/max17050/inspect.resc`, `sapporo.resc` MAX17050 checkpoint comments, and any battery metadata in `suunto-firmware`.

## Current Baseline

No local fuel-gauge kind/device exists. Renode embeds separate MAX17050 classes inside IOM controllers at address `0x36`, mixing physical register semantics with deterministic host battery values.

## Allowed Files

Only `src/devices/{sapporo_max17050.c,sapporo_max17050.h}` and `tests/devices/test_sapporo_max17050.c`.

## Frozen Interfaces

Opaque lifecycle and typed I2C endpoint; explicit little/big byte order from evidence; constructor accepts encoded deterministic status/voltage/capacity fixture. Address is board wiring. Unknown register/value/shape refuses before state change.

## Evidence Inputs

`E-SAP-MAX17050-001` must cite `SapporoMax17050.Write/Read/Reset` (or exact nested method lines), authentic Status/VCell startup bytes, register pointer lifecycle, reset values, and a refusal trace. Generic MAX17050 documentation alone is insufficient for board defaults.

## Implementation

Extract only verified shared register behavior from nested classes; separate immutable fixture values from mutable configuration/status; make reset exact and deterministic.

## Tests and Commands

`make test TEST_FILTER=sapporo_max17050` runs only its binary and exits 0; Status/VCell checkpoint transcript, reset, config if traced, byte order, wrong address/register/length/state, fixture bounds, and repeat transcript pass. `make check` exits 0.

## Acceptance

Firmware startup bytes match; synthetic battery fields are declared; no IOM dependency remains; unknown registers refuse; repeated reads/reset are deterministic.

## Forbidden Scope

No host battery API, decay simulation, Ulsan defaults, persistence compatibility, all-register map, controller logic, board attachment, or aggregate edit.

## Handoff

Report register/fixture table, provenance, evidence hash, tests, and endpoint role for 420.
