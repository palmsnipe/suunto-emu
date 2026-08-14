# 298 — Targeted Test Runner and Phase 3–5 Module Contract

**Status:** ready
**Phase:** 3
**Dependencies:** 120

## Goal

Freeze transcript, lifecycle/signal, DMA-request, and display-backend seams with targeted tests. This unlocks disjoint Phase 3–5 component agents; hardware policy and build-system changes remain deferred.

## Execution Budget

One to two agent-days. Finish when transcript support and shared Phase 3–5 peripheral/display seams are frozen.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `include/semu/{peripheral,display,machine}.h`, `tests/support/test.{c,h}`, and current endpoint/machine call sites found with `rg 'semu_serial_endpoint|semu_machine_options' src tests`.

## Current Baseline

The build already filters test binary basenames with `TEST_FILTER`. Test support lacks transcript comparison. `semu_serial_endpoint` is synchronous with no reset/ready callbacks. Machine options carry frame callback/context; display exposes only `semu_surface` and has no command-submission seam.

## Allowed Files

Only `tests/support/{test,transcript}.{c,h}`, `tests/unit/test_transcript.c`, and `include/semu/{peripheral,display,machine}.h`. Later component tickets may not edit these public headers.

## Frozen Interfaces

Transcript comparison covers direction/address/chip-select/bytes/result and identifies the first mismatch. Keep `semu_serial_endpoint` source-compatible; add separate optional lifecycle/signal callback typedefs for device constructors rather than extending endpoint instances. Freeze `semu_dma_request` as controller ID, direction, guest address, count, endpoint, continuation, and completion callback/context, plus a request-sink typedef. Add opaque display-backend submission fields to `semu_machine_options`; NULL means completion-only/no frames. Backend receives bus, command-ring address/word count, virtual time, frame callback/context and returns `OK/WAIT/REFUSE` without CPU access. Preserve current filtered-test semantics without modifying the build.

## Evidence Inputs

No hardware evidence. Preserve current `semu_serial_transaction`, `semu_frame`, `semu_machine_options`, and zero-initialized caller compatibility.

## Implementation

Add bounded transcript helpers, lifecycle/signal/DMA callback declarations, and append-only display-backend option fields. Existing machine options are already zero-initialized; add no hardware policy or callsite changes.

## Tests and Commands

`make test TEST_FILTER=hash` uses the existing filter, prints exactly one `TEST build/tests/test_hash`, and exits 0. `make test TEST_FILTER=core` and `make test TEST_FILTER=machine` exit 0. A new transcript-helper self-test selected by `make test TEST_FILTER=transcript` exits 0. `make check` exits 0.

## Acceptance

Existing filter semantics remain unchanged; existing tests compile; mismatch diagnostics locate the first byte; NULL callbacks are safe; headless `make` has no SDL link.

## Forbidden Scope

No Makefile/tool/callsite edit, Apollo4/device/display implementation, dependency, wildcard success, mandatory SDL, CLI feature, endpoint layout change, or initializer cleanup.

## Handoff

Report declarations, compatibility notes, command outputs, and the commit downstream agents must use.
