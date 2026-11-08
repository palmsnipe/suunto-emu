# 809 — Ulsan 2.44.52 Startup Power Control

**Status:** done
**Phase:** 7
**Dependencies:** 808

## Goal

Advance the pristine 2.44.52 profile past its first unsupported power
transaction using the observed 2.44.52 PWRCTRL contract.

## Execution Budget

One device instance. Use two cold runs with a 20,000,000-instruction
and 1,000,000,000 ns cap. Stop at the next unsupported block.

## Required Reading

Read ticket 808, E-ULS-0052 and E-ULS-0053, the execution-model memory
and DMA section, `docs/adding-a-device.md`, `include/semu/bus.h`,
`src/devices/ulsan_pwrctrl.{c,h}`, `src/boards/ulsan_board.{c,h}`,
`src/boards/machine.c`, the private `src/boards/machine_internal.h`,
`tests/devices/test_ulsan_pwrctrl.c`, and
`tests/integration/test_firmware_ulsan_244_reset.sh`. Read the pinned
lane power-model declaration and wrapper named in E-ULS-0053.

## Current Baseline

The 2.44.52 reset profile passes GPIO pad setup and first faults on the
32-bit read of PWRCTRL +0x08 at PC 0x00076eca. The 2.35.36 controller
serves its own version's law, which differs at the graphics power bits.

## Allowed Files

- `src/devices/ulsan_pwrctrl*.{c,h}`, `src/boards/ulsan_board.{c,h}`,
  `src/boards/machine.c`, `src/boards/machine_internal.h`
- `tests/devices/test_ulsan_pwrctrl*.c`,
  `tests/integration/test_firmware_ulsan_244_reset.sh`
- `docs/migration-evidence.md`, this ticket's handoff

## Frozen Interfaces

Keep public APIs and the 2.35.36 law intact. Validate width, register, and
supported values before mutation. Unknown transactions refuse.

## Evidence Inputs

E-ULS-0053 pins the 2.44.52 startup access stream and the lane wrapper.
E-ULS-0052 pins the standalone first fault and base checkpoint.

## Implementation

Share the proven common power-control behavior and select the evidenced
2.44.52 behavior at board attachment. Observe the next distinct refusal
before expanding scope.

## Tests and Commands

- `make test TEST_FILTER=ulsan_pwrctrl`
- `make test TEST_FILTER=ulsan`
- `SEMU_FIRMWARE_MANIFEST=/path/to/firmware.semu make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_reset`
- `make check-lines && make check && make sanitize`

## Acceptance

A first read at +0x08 returns the observed reset value. Any modeled writes
and their readbacks match the 2.44.52 lane with success and refusal coverage.
Two standalone runs advance to an identical new checkpoint. Preserve the
808 checkpoint in the ledger and attribute any updated test pin to this gap.

## Forbidden Scope

Other controller laws, timing changes, flash fixtures, frame publication,
and unrelated 2.35.36 changes.

## Handoff

Record covered transactions, refusals, checkpoint hashes, and the next
boundary on the path to the E-ULS-0054 Service Mode frame.

The implementation and its new standalone boot boundary are recorded in
E-ULS-0055. The next fault is the BootROM delay fetch at `0x0800009c`.
E-ULS-0056 confirms its sharing contract and the following CLKGEN access.

Integrator review (2026-11-04): six focused power tests, all 62 Ulsan
tests, the private two-run gate, `make check-lines`, `make check`, and
`make sanitize` passed. The refusal tests verify unchanged power state
after a rejected access, and the two-bus test verifies 2.44.52 isolation.
