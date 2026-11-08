# 812 — Ulsan 2.44.52 SystemTimer and Watchdog Startup

**Status:** done
**Phase:** 7
**Dependencies:** 811

## Goal

Advance 2.44.52 through its retained SystemTimer and watchdog startup
transactions, using the existing deterministic timer and register laws.

## Execution Budget

Two shared attachments. Run two cold boots at 20,000,000 instructions and
1,000,000,000 virtual ns. Stop at the next unsupported controller.

## Required Reading

Read ticket 811, E-ULS-0015, E-ULS-0038, E-ULS-0045, E-ULS-0046,
E-ULS-0051, E-ULS-0059 and E-ULS-0060. Read the scheduling, reset and
reproducibility sections of `docs/execution-model.md` and the ownership
section of `docs/architecture.md`. Read `include/semu/{bus,scheduler}.h`,
`src/devices/ulsan_{stimer,wdt}.{c,h}`, `src/boards/ulsan_board.{c,h}`,
`src/boards/machine{.c,_internal.h}`, `tests/devices/test_ulsan_{stimer,wdt}.c`
and `tests/integration/test_firmware_ulsan_244_reset.sh`.

## Current Baseline

The first fault is the retained SystemTimer CONFIG read at `0x40008800`.
Temporary probes attaching the existing timer and watchdog advance to
the RTC block.

## Allowed Files

- `src/devices/ulsan_{stimer,wdt}.{c,h}`, `src/boards/ulsan_board.{c,h}`,
  `src/boards/machine.c`, `src/boards/machine_internal.h`
- `tests/devices/test_ulsan_{stimer,wdt}.c`,
  `tests/integration/test_firmware_ulsan_244_reset.sh`
- `docs/migration-evidence.md` and this ticket's handoff

## Frozen Interfaces

Preserve the public API, deterministic CPU cadence, retained NVRAM
behavior, existing SystemTimer and watchdog register laws, and the
2.35.36 pins.

## Evidence Inputs

E-ULS-0060 records the exact 2.44.52 timer and watchdog access streams,
the shared platform declarations, and the diagnostic RTC boundary.

## Implementation

Attach the two shared controllers with machine-owned 2.44.52 state.
Bind the SystemTimer to the machine scheduler. Preserve the existing
watchdog configuration and reload behavior. Pin the next fault.

## Tests and Commands

- `make test TEST_FILTER=ulsan_stimer`
- `make test TEST_FILTER=ulsan_wdt`
- `make test TEST_FILTER=ulsan`
- `SEMU_FIRMWARE_MANIFEST=/path/to/firmware.semu make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_reset`
- `make check-lines && make check && make sanitize`

## Acceptance

The existing success and refusal cases pass. Test isolated state on two
live buses. Two standalone cold boots pass SystemTimer and watchdog setup
and match the next pinned transcript. Preserve E-ULS-0059.

## Forbidden Scope

RTC behavior, new timer or watchdog laws, CPU cadence changes, and
display publication.

## Handoff

Record the standalone checkpoint and the next observed gap toward
Service Mode.

E-ULS-0062 pins the new two-run boot transcript at the RTC CONFIG
read. E-ULS-0061 records the matching RTC register sequence.

Integrator review (2026-11-04): both focused suites, all 66 Ulsan
tests, the private firmware gate, `make check-lines`, `make check`,
and `make sanitize` passed.
