# 816 — Ulsan 2.44.52 Timer1 Clock

**Status:** done
**Phase:** 7
**Dependencies:** 815

## Goal

Use the observed Timer1 rate in the 2.44.52 reset profile and
reproduce the first MSPI1 access reached after the timer wait.

## Execution Budget

One Timer1 comparator timing correction. Run two cold boots with
caps of 25,000,000 instructions and 4,000,000,000 virtual ns.

## Required Reading

Read tickets 814 and 815, E-ULS-0035, E-ULS-0051, E-ULS-0065,
and E-ULS-0068 through E-ULS-0070. Read the execution model for
virtual time and event scheduling, `include/semu/scheduler.h`,
`src/devices/ulsan_timer0.{c,h}`, the Timer device tests and
the private reset gate.

## Current Baseline

The 2.44.52 comparator inherits a period of 61,035 ns per tick.
At Timer1 control `0xa21` and Compare0 `0x107`, the lane counter
advances 131 ticks over 4 ms. A scratch deadline of
`ceil(compare * 1e9 / 32768)` advances the standalone boot from
PC `0x000d47f0` to an MSPI1 read at PC `0x00124b1e`.

## Allowed Files

- `src/devices/ulsan_timer0.{c,h}`
- `tests/devices/test_ulsan_timer0wake.c`
- `tests/integration/test_firmware_ulsan_244_reset.sh`
- `docs/migration-evidence.md` and this ticket's handoff

## Frozen Interfaces

Preserve the 2.35.36 comparator, CPU cadence, IRQ routing, pulse
deassertion policy, and Timer register access and refusal contracts.

## Evidence Inputs

E-ULS-0070 supplies the measured count rate and the twice-reproduced
diagnostic boundary. E-ULS-0069 preserves the prior two-run pin.

## Implementation

Use the 32,768 Hz Compare0 deadline for the 2.44.52 Timer1 path.
Keep the conversion in integer nanoseconds, rounding up to the
first representable deadline. Preserve the pulse and register
semantics outside that deadline calculation.

## Tests and Commands

- `make test TEST_FILTER=ulsan_timer0`
- `make test TEST_FILTER=ulsan`
- `SEMU_FIRMWARE_MANIFEST=/path/to/firmware.semu make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_reset`
- `make check-lines && make check && make sanitize`

## Acceptance

A focused regression verifies that both Timer1 IRQs stay low before
the new deadline and rise at it. An unsupported access still
refuses and the 2.35.36 tests retain their pins. Two private
firmware runs reproduce the MSPI1 boundary and the manifest
identity refusal.

## Forbidden Scope

Changes to CPU cadence, RTC and Timer1 clear semantics, storage
behavior, firmware patches, and display publication.

## Handoff

Record the Timer1 deadline, both firmware checkpoints and the
first MSPI1 access for separate controller integration.

Integrator review (2026-11-04). The timing regression fails on
the inherited period and passes at the E-ULS-0070 deadline.
All five Timer tests, 69 Ulsan tests, the private two-run firmware
gate, `make check-lines`, `make check`, and `make sanitize`
passed. E-ULS-0071 records the completed standalone checkpoint
and fault PC. Ticket 817 scopes the next controller attachment.
