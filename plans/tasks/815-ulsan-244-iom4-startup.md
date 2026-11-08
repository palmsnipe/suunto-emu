# 815 — Ulsan 2.44.52 IOM4 Startup

**Status:** done
**Phase:** 7
**Dependencies:** 814

## Goal

Advance the exact 2.44.52 firmware past IOM4 initialization and the first
PMIC DMA sequence. Distinguish a completed transaction from the guest's
later timer wait.

## Execution Budget

One controller integration. Run two cold boots with 200,000,000 instructions
and 4,000,000,000 virtual ns as maximum diagnostic budgets.

## Required Reading

Read ticket 814, E-ULS-0029, E-ULS-0030 and E-ULS-0066 through
E-ULS-0068. Read the cited IOM DMA lane source, the execution-model
memory and DMA section, `src/devices/ulsan_iom4*.{c,h}`,
`src/boards/ulsan_board.{c,h}`, `src/boards/machine.c`,
`include/semu/{bus,apollo4}.h`, the IOM4 device tests, and the
private 2.44.52 reset gate.

## Current Baseline

The standalone first fault is an IOM4 threshold write at `0x40054104`.
The shared-map diagnostic processes 21 PMIC doorbells and reaches a
repeatable Timer1 wait. The lane's completion status is `0x403` in
the observed IRQ path.

## Allowed Files

- `src/devices/ulsan_iom4*.{c,h}`, `src/boards/ulsan_board.{c,h}`,
  `src/boards/machine.c`, `src/boards/machine_internal.h`
- `tests/devices/test_ulsan_iom4*.c`,
  `tests/integration/test_firmware_ulsan_244_reset.sh`
- `docs/migration-evidence.md` and this ticket's handoff

## Frozen Interfaces

Preserve existing 2.35.36 results and the DMA validation, IRQ and refusal
contracts. Keep the CPU and timer laws out of the controller change.

## Evidence Inputs

E-ULS-0067 pins the ordered IOM4 register and endpoint trace.
E-ULS-0068 records the exploratory controller attachment and distinguishes
the next firmware wait from an unmapped controller.

## Implementation

Reuse the evidence-backed register and DMA paths with isolated 2.44.52
state. Match the observed startup completion and IRQ behavior.
Report the first unsupported access or parked state.

## Tests and Commands

- `make test TEST_FILTER=ulsan_iom4`
- `make test TEST_FILTER=ulsan`
- `SEMU_FIRMWARE_MANIFEST=/path/to/firmware.semu make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_reset`
- `make check-lines && make check && make sanitize`

## Acceptance

A valid startup transfer matches the observed completion and IRQ sequence,
and unsupported access or DMA fails closed. Existing Ulsan pins pass.
Two private firmware runs match a new pinned boot transcript.

## Forbidden Scope

Timer or CPU cadence changes, invented endpoint responses, firmware
patches, and display publication.

## Handoff

Record the completed transfer evidence, new standalone checkpoint, and
remaining wait or hardware boundary toward Service Mode.

Integrator review (2026-11-04). E-ULS-0069 records 21 completed
standalone PMIC transfers and the two identical boot transcripts.
The focused IOM4 suite passed 10 tests, the Ulsan suite passed 69,
and the private two-run reset and identity-refusal gate passed.
`make check-lines`, `make check`, and `make sanitize` passed.
The 2.35.36 reset pin still reproduces. The 2.44.52 boot stops
at PC `0x000d47f0` after 19,106,348 instructions and 4,014,847,988 ns.
Ticket 816 takes the separate Timer1 clock discrepancy documented
in E-ULS-0070.
