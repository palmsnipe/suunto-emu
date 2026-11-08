# 814 — Ulsan 2.44.52 Timer1 Startup

**Status:** done
**Phase:** 7
**Dependencies:** 813

## Goal

Pass the Timer1 startup sequence and reach the IOM4 boundary with the
observed register behavior and comparator IRQ routes.

## Execution Budget

One controller integration. Run two cold boots at 20,000,000 instructions
and 1,000,000,000 virtual ns. Stop at the next unsupported controller.

## Required Reading

Read ticket 813, E-ULS-0021, E-ULS-0035, E-ULS-0051, E-ULS-0064 and
E-ULS-0065. Read `src/devices/ulsan_timer0.{c,h}`,
`src/boards/ulsan_board.{c,h}`, `src/boards/machine.c`,
`include/semu/{bus,scheduler}.h`, the reset and scheduling sections of
`docs/execution-model.md`, the Timer device tests, and the private reset gate.

## Current Baseline

RTC initialization passes. The first fault is the Timer1 control read
at `0x40008220`. The lane confirms a GlobalEnable reset word of `0x7ff`
and that the Timer1 clear bit does not persist on readback.

## Allowed Files

- `src/devices/ulsan_timer0.{c,h}`, `src/boards/ulsan_board.{c,h}`,
  `src/boards/machine.c`, `src/boards/machine_internal.h`
- `tests/devices/test_ulsan_timer0*.c`,
  `tests/integration/test_firmware_ulsan_244_reset.sh`
- `docs/migration-evidence.md` and this ticket's handoff

## Frozen Interfaces

Preserve the existing 2.35.36 Timer behavior, deterministic virtual time,
IRQ ordering, and unsupported access refusals.

## Evidence Inputs

E-ULS-0065 pins the 2.44.52 Timer1 register and NVIC streams and the
diagnostic IOM4 boundary.

## Implementation

Reuse the Timer1 comparator with machine-owned state. Select the observed
reset word and clear-bit readback for 2.44.52. Wire IRQ 14 and IRQ 68.
Pin the next firmware boundary.

## Tests and Commands

- `make test TEST_FILTER=ulsan_timer0`
- `make test TEST_FILTER=ulsan`
- `SEMU_FIRMWARE_MANIFEST=/path/to/firmware.semu make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_reset`
- `make check-lines && make check && make sanitize`

## Acceptance

Verify the reset word, nonpersistent clear bit, comparator IRQs and
refusal paths. Verify independent controller state. Two cold firmware
runs reproduce the next standalone checkpoint. Preserve E-ULS-0064.

## Forbidden Scope

IOM4 behavior, new CPU timing laws, synthetic boot workarounds, and
frame publication.

## Handoff

Report Timer1 verification and the next exact boot fault.

E-ULS-0066 records the standalone IOM4 boundary and E-ULS-0067
records the corresponding lane IOM4 startup trace.

Integrator review (2026-11-04): all five focused Timer tests, all 68
Ulsan tests, the private two-run gate, `make check-lines`, `make check`,
and `make sanitize` passed. E-ULS-0068 records later IRQ-clear evidence
and the post-IOM4 wait for separate integration.
