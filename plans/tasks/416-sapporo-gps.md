# 416 — Sapporo CXD5610 GPS Transport

**Status:** done
**Phase:** 4
**Dependencies:** 285, 295, 298, 314

## Goal

Implement bounded CXD5610 UART request framing, awake signaling, and fixture injection seams. This unlocks GPS fixtures 418 and headless wiring 420; synthetic replies and location simulation remain deferred.

## Execution Budget

Two agent-days. Implement the physical UART transport state and exact evidenced request assembly; keep synthetic replies outside the device.

## Required Reading

`SapporoCxd5610Transport.cs:WriteChar/TryCompleteExchange/Reset/PulseAwakeAfter`, `sapporo-gps-transport.repl`, `docs/research/cxd5610-gps-boundary.md`, `startup-request-0105000f.md`, and GPS result logs named by 295.

## Current Baseline

No local GPS device exists. The Renode transport is 921600 baud, accumulates bytes, supports awake pulses, manual injection, and configurable exact exchanges; exact replies are fixtures, not physical GPS semantics.

## Allowed Files

Only `src/devices/{sapporo_cxd5610.c,sapporo_cxd5610.h}` and `tests/devices/test_sapporo_cxd5610.c`.

## Frozen Interfaces

Opaque lifecycle and byte-stream endpoint for UART 314; scheduler-backed awake/RX injection; transcript observer; registration of exact exchange fixtures is an optional injected callback owned by 418. Without a fixture, complete unknown requests refuse rather than synthesize.

## Evidence Inputs

`E-SAP-CXD5610-001` must cite the transport methods and contain request boundary/framing, 921600 configuration, awake polarity/timing, reset, and unknown-request trace. Response bytes require separate `E-SAP-COMPAT-GPS-001` and are not implemented here.

## Implementation

Implement bounded request accumulation, exact terminator/length rules, reset/generation cancellation, awake scheduling, and byte transcript; cap buffers to the largest verified request.

## Tests and Commands

`make test TEST_FILTER=sapporo_cxd5610` runs only its binary and exits 0; split/complete request, overflow, unknown request refusal, awake pulse timing/cancel, RX injection order, reset, wrong state, and two-run transcript pass. `make check` exits 0.

## Acceptance

Physical transport behavior matches evidence; no response is fabricated without fixture callback; buffers are bounded; all scheduled callbacks are canceled safely on reset/destroy.

## Forbidden Scope

No NMEA/location simulation, hardcoded synthetic response, host GPS/network, UART registers, wall-clock delay, firmware hook, board attachment, or aggregate edit.

## Handoff

Report framing/buffer/awake contract, trace hashes, missing replies, tests, and UART/fixture attachment points for 418/420.
