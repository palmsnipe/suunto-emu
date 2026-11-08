# 818 — Ulsan 2.44.52 Post-Storage MMIO

**Status:** done
**Phase:** 7
**Dependencies:** 817

## Goal

Admit the observed post-storage fallback tuples on the 2.44.52
profile and reproduce the first display-controller access.

## Execution Budget

One fallback-table attachment. Run two cold boots with caps of
72,000,000 instructions and 4,000,000,000 virtual ns.

## Required Reading

Read ticket 817, E-ULS-0038, and E-ULS-0073 through E-ULS-0075.
Read the execution model's memory and DMA rules,
`src/devices/ulsan_buszero.{c,h}`, `src/boards/ulsan_board.c`,
`include/semu/bus.h`, the bus-zero device tests and the private
reset gate.

## Current Baseline

The first fault is `W32 0x400b2024=0x80000000`. Two lane captures
record 52 accesses in the post-storage `0x400b` windows. The
diagnostic attachment reaches the first display access at `0x400a8074`.

## Allowed Files

- `src/devices/ulsan_buszero.{c,h}`
- `tests/devices/test_ulsan_buszero.c`
- `tests/integration/test_firmware_ulsan_244_reset.sh`
- `docs/migration-evidence.md` and this ticket's handoff

## Frozen Interfaces

Keep the width, address, direction and write-value rules on
all existing fallback windows. Keep unmapped regions refused.

## Evidence Inputs

E-ULS-0038 describes the existing narrow tables.
E-ULS-0073 confirms the 2.44.52 access sequence and the
diagnostic frontier. E-ULS-0074 pins the current standalone
checkpoint. E-ULS-0075 identifies the lane display startup write.

## Implementation

Reuse table rows whose complete tuples match the 2.44.52
lane evidence. Keep all other accesses refused.

## Tests and Commands

- `make test TEST_FILTER=ulsan_buszero`
- `make test TEST_FILTER=ulsan`
- `SEMU_FIRMWARE_MANIFEST=/path/to/firmware.semu make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_reset`
- `make check-lines && make check && make sanitize`

## Acceptance

Verify the first write, a zero read and a width-specific tuple
on 2.44.52. Verify an unobserved read, write value and width
refuse. Two private firmware boots reproduce the display
boundary and retain the identity refusal check.

## Forbidden Scope

New peripheral semantics, generic fallback handling, firmware
changes, display attachment and frame publication.

## Handoff

Record the matched access census, test results and display
boundary for the separate frame integration.

Integrator review (2026-11-04). All four bus-zero tests, the
Ulsan selection, the private two-run firmware gate,
`make check-lines`, `make check`, and `make sanitize` passed.
E-ULS-0076 records the 24 matched tuples and the production display
boundary. Ticket 819 scopes the display attachment and frame.
