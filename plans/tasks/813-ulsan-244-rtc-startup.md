# 813 — Ulsan 2.44.52 RTC Startup

**Status:** done
**Phase:** 7
**Dependencies:** 812

## Goal

Use the observed RTC initialization and recurring alarm contract to
advance 2.44.52 to its Timer1 boundary.

## Execution Budget

One shared attachment. Use two independent cold runs with caps of
20,000,000 instructions and 1,000,000,000 virtual ns.

## Required Reading

Read ticket 812, E-ULS-0036, E-ULS-0048, E-ULS-0051, and
E-ULS-0061 through E-ULS-0063. Read the ownership, reset and scheduling
sections of the architecture and execution-model documents.
Read `src/devices/ulsan_rtc.{c,h}`, `src/boards/ulsan_board.{c,h}`,
`src/boards/machine.c`, `include/semu/{bus,scheduler}.h`,
`tests/devices/test_ulsan_rtc.c` and the 2.44.52 private reset gate.

## Current Baseline

The first fault is RTC CONFIG at `0x40004800`. The read-only lane and
temporary standalone probes establish the subsequent Timer1 boundary.

## Allowed Files

- `src/devices/ulsan_rtc.{c,h}`, `src/boards/ulsan_board.{c,h}`,
  `src/boards/machine.c`, `src/boards/machine_internal.h`
- `tests/devices/test_ulsan_rtc.c`,
  `tests/integration/test_firmware_ulsan_244_reset.sh`
- `docs/migration-evidence.md` and this ticket's handoff

## Frozen Interfaces

Preserve the public machine and scheduler API, deterministic CPU cadence,
the 2.35.36 RTC law, and its private test pins.

## Evidence Inputs

E-ULS-0061 records the matching register trace. E-ULS-0063 confirms
one-second alarm repeat, wiring and the next fault.

## Implementation

Attach the existing RTC model using machine-owned 2.44.52 state and wire
it to the scheduler and CPU IRQ sink. Keep unsupported accesses refusing.
Verify the next exact firmware checkpoint.

## Tests and Commands

- `make test TEST_FILTER=ulsan_rtc`
- `make test TEST_FILTER=ulsan`
- `SEMU_FIRMWARE_MANIFEST=/path/to/firmware.semu make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_reset`
- `make check-lines && make check && make sanitize`

## Acceptance

Two live instances have isolated RTC state and alarm events. The existing
success and refusal cases pass. Two standalone cold boots match the new
Timer1 boundary. Preserve the E-ULS-0062 checkpoint.

## Forbidden Scope

Timer1 changes, a new RTC law, synthetic firmware, CPU cadence changes,
and display publication.

## Handoff

Record the exact standalone boot pair and the next evidence-backed gap.

E-ULS-0064 pins the new two-run firmware transcript at the Timer1
configuration read.

Integrator review (2026-11-04): the seven RTC tests, all 67 Ulsan
tests, the private firmware gate, `make check-lines`, `make check`,
and `make sanitize` passed.
