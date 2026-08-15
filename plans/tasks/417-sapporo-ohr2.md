# 417 — Sapporo OHR2 Transport

**Status:** done
**Phase:** 4
**Dependencies:** 285, 295, 298

## Goal

Implement OHR2 packet/CRC/sequence/ready transport without built-in synthetic bodies. This unlocks OHR fixtures 418 and headless wiring 420; heart-rate simulation remains deferred.

## Execution Budget

Two agent-days. Implement OHR packet framing, CRC, sequence, ready signaling, and evidenced state transitions without synthetic sensor payloads.

## Required Reading

`src/devices/sapporo_devices.c:ohr_transfer`, `test_sapporo_devices.c:test_ohr_refusal`, `SapporoOhr2Transport.cs:Write/Read/PrepareResponse/Reset`, and `docs/research/ohr2-startup-handshake.md`.

## Current Baseline

The aggregate accepts fixed 59-byte requests at address `0x10`, emits 58-byte replies for commands 0/1/6/13/14, switches to MAIN on command 3, and synthesizes zero bodies. It lacks ready callback, queued-read lifecycle, strict sequence/state validation, and provenance separation.

## Allowed Files

Only `src/devices/{sapporo_ohr2.c,sapporo_ohr2.h}` and `tests/devices/test_sapporo_ohr2.c`.

## Frozen Interfaces

Opaque lifecycle and typed endpoint; packet constants/CRC are explicit; ready callback from 298; optional response-body fixture callback owned by 418. Device validates header/CRC/sequence/state and owns at most one queued response. Missing body fixture refuses before asserting ready.

## Evidence Inputs

`E-SAP-OHR2-001` must cite the four Renode methods and exact request/ready/read/reset traces for each physically modeled command. Synthetic identity/zero bodies require `E-SAP-COMPAT-OHR-001`; current aggregate behavior is hypothesis until verified.

## Implementation

Separate packet validation, transport state, and response-body provider; implement fire-and-forget/Main transition only if traced; clear ready on exact evidenced read/reset boundary.

## Tests and Commands

`make test TEST_FILTER=sapporo_ohr2` runs only its binary and exits 0; CRC/shape, command/sequence, ready assert/clear, queued-read, fire-and-forget, reset, unknown command/state, fixture absent, and repeat transcript pass. `make check` exits 0.

## Acceptance

CRC and ready order match trace; invalid packets never alter state; physical transport passes without built-in synthetic body; no valid command is silently zero-filled.

## Forbidden Scope

No heart-rate simulation, zero-body default, IOM/DMA implementation, hardcoded identity fixture, firmware hook, board attachment, or aggregate edit.

## Handoff

Report command/state table, ready timing, evidence hashes, fixture callback needs, tests, and endpoint role for 418/420.
