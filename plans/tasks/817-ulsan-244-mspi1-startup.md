# 817 — Ulsan 2.44.52 MSPI1 Startup

**Status:** done
**Phase:** 7
**Dependencies:** 816

## Goal

Attach the evidence-backed MSPI1 controller to the 2.44.52 reset
profile and reach the first post-storage MMIO boundary.

## Execution Budget

One controller attachment. Run two cold boots capped at
30,000,000 instructions and 4,000,000,000 virtual ns.

## Required Reading

Read ticket 816, E-ULS-0031, E-ULS-0033, E-ULS-0034,
E-ULS-0038, and E-ULS-0070 through E-ULS-0072. Read the
execution model's memory and DMA rules, the cited MSPI1 lane
source and wiring, `src/devices/ulsan_mspi1*.{c,h}`,
`src/boards/ulsan_board.{c,h}`, `src/boards/machine.c`,
`include/semu/{bus,apollo4}.h`, the MSPI1 device tests,
and the private reset gate.

## Current Baseline

The first fault is an MSPI1 read at `0x40061090`, PC
`0x00124b1e`. A diagnostic reusing the existing controller
completes 2,131 queues and reaches the lane-observed post-storage
write at `0x400b2024`.

## Allowed Files

- `src/devices/ulsan_mspi1*.{c,h}`
- `src/boards/ulsan_board.{c,h}`, `src/boards/machine.c`,
  `src/boards/machine_internal.h`
- `tests/devices/test_ulsan_mspi1*.c`
- `tests/integration/test_firmware_ulsan_244_reset.sh`
- `docs/migration-evidence.md` and this ticket's handoff

## Frozen Interfaces

Preserve the 2.35.36 MSPI1 path, the reset-retained backing
contract, native descriptor validation, and fail-closed DMA.

## Evidence Inputs

E-ULS-0070 identifies the 2.44.52 startup register and queue
trace. E-ULS-0071 pins the present standalone boundary.
E-ULS-0072 records the post-storage diagnostic, including the
descriptor and completion census.

## Implementation

Give the 2.44.52 board an independent MSPI1 state and attach
the observed IRQ 21 sink. Reuse the existing native response
and lifecycle handling. Keep emulated backing local to that
machine across guest resets.

## Tests and Commands

- `make test TEST_FILTER=ulsan_mspi1`
- `make test TEST_FILTER=ulsan`
- `SEMU_FIRMWARE_MANIFEST=/path/to/firmware.semu make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_reset`
- `make check-lines && make check && make sanitize`

## Acceptance

Verify a 2.44.52 JEDEC queue completion, unsupported descriptor
refusal, and independent controller and backing state. Existing
Ulsan pins pass. Two private boots reproduce the first post-storage
boundary and preserve manifest identity refusal.

## Forbidden Scope

New flash contents, persistence formats, post-storage register
handling, timing changes, and display publication.

## Handoff

Record the completed storage lifecycle and the first post-storage
boundary for separate evidence-backed integration.

Integrator review (2026-11-04). The descriptor tests failed on the
unmapped 2.44.52 board before attachment. All seven MSPI1 tests,
the Ulsan selection, the private two-run firmware gate,
`make check-lines`, `make check`, and `make sanitize` passed.
E-ULS-0074 records the production post-storage boundary, which
ticket 818 addresses separately.
