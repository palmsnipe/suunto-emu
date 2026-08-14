# 302 — Apollo4 Power Controller

**Status:** blocked
**Phase:** 3
**Dependencies:** 285, 295, 298

## Goal

Implement only Sapporo-verified Apollo4 power request/status transitions. This supplies the power block required by 304/320; device mapping and Ulsan behavior remain deferred.

## Execution Budget

Complete within the stated one-to-three agent-day budget: Implement only the power/status handshakes used by exact Sapporo 2.22.60.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, the ticket-specific evidence row from 295, and every baseline source named below.

## Current Baseline

`apollo4.c` maps `0x40021000/0x400` through a generic regbank allowing `0x04,0x24,0x100,0x108,0x250,0x254`; it omits status at `0x08`, retention `0x2c`, performance control `0x00`, masks, and state transitions.

## Allowed Files

Only `src/soc/apollo4/{power.c,power.h}` and `tests/devices/test_apollo4_power.c`.

## Frozen Interfaces

Opaque create/destroy/reset plus bus ops; create accepts bus/scheduler and optional power-state callback. Map remains `0x40021000/0x400`. Status changes are same-time or scheduled exactly as evidence states; callbacks report named gate state without mapping devices themselves.

## Evidence Inputs

`E-A4-PWR-001` must be verified and cite `SapporoApollo4Extensions.cs:SapporoApollo4PowerController.ReadDoubleWord`, `WriteDoubleWord`, and `Reset`, plus a trace SHA covering performance, device/Nema power, SRAM enable/retention, SIMO buck, and polling order. C# constants without authentic trace remain unsupported.

## Implementation

Implement only evidence-enumerated behavior behind the frozen interface; validate the complete operation before mutation and split any hand-written file approaching 300 lines.

## Tests and Commands

`make test TEST_FILTER=apollo4_power` runs only `test_apollo4_power` and exits 0 after reset, request/status, mask, scheduling, unknown-offset, and repeated-run cases. `make check` exits 0.

## Acceptance

All traced reads/writes and status transitions match in order; reset returns exact evidenced values; unknown bits do not latch unless proven; no host time or global device side effects occur.

## Forbidden Scope

No clock/reset sequencing, actual peripheral mapping, Ulsan masks, guessed readiness, generic read/write retention, or edits to integration/public files.

## Handoff

Report register table, transition timing, callback events, trace hash, and 320 attachment requirements.
