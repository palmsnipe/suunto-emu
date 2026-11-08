# 810 — Ulsan 2.44.52 BootROM and Clock Startup

**Status:** done
**Phase:** 7
**Dependencies:** 809

## Goal

Advance the 2.44.52 startup through its BootROM delay and clock setup using
the existing models whose lane contracts match this version.

## Execution Budget

Reuse two models without adding peripheral laws. Pin two cold boots at
20,000,000 instructions and 1,000,000,000 virtual ns.

## Required Reading

Read ticket 809, E-ULS-0009, E-ULS-0010, E-ULS-0055 and E-ULS-0056,
the execution-model memory and DMA section, and the architecture ownership
and fail-closed sections. Read `src/devices/ulsan_bootrom.{c,h}`,
`src/devices/ulsan_clkgen.{c,h}`, `src/boards/ulsan_board.{c,h}`,
`src/boards/machine.c`, `src/boards/machine_internal.h`,
`include/semu/bus.h`, `tests/devices/test_ulsan_bootrom.c`,
`tests/devices/test_ulsan_clkgen.c` and the 2.44.52 reset regression.

## Current Baseline

The exact 2.44.52 firmware runs through GPIO and power setup. It faults on
the fetch of the reference-lane BootROM delay stub at `0x0800009c`.

## Allowed Files

- `src/boards/ulsan_board.{c,h}`, `src/boards/machine.c`,
  `src/boards/machine_internal.h`, `src/devices/ulsan_clkgen.{c,h}`
- `tests/devices/test_ulsan_clkgen.c`,
  `tests/integration/test_firmware_ulsan_244_reset.sh`
- `docs/migration-evidence.md` and this ticket's handoff

## Frozen Interfaces

Keep the public machine and bus APIs, prior profile behavior, and the
BootROM and CLKGEN register laws. CLKGEN state for a 2.44.52 machine must
have the same lifetime as that machine.

## Evidence Inputs

E-ULS-0056 records the byte-identical BootROM and CLKGEN lane observations,
the matching declarations, and the next unsupported DAXI access.

## Implementation

Attach the existing BootROM and clock generator for 2.44.52. Reuse the
CLKGEN callbacks with machine-owned state. Advance through the observed
startup sequence and pin the next unsupported access.

## Tests and Commands

- `make test TEST_FILTER=ulsan_bootrom`
- `make test TEST_FILTER=ulsan_clkgen`
- `make test TEST_FILTER=ulsan`
- `SEMU_FIRMWARE_MANIFEST=/path/to/firmware.semu make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_reset`
- `make check-lines && make check && make sanitize`

## Acceptance

The existing success and refusal cases pass. The 2.44.52 clock state is
isolated across live machines. Two cold firmware runs pass the BootROM
and CLKGEN boundaries and match a new pinned transcript. Retain the 809
checkpoint in the ledger.

## Forbidden Scope

New BootROM bytes, new clock laws, DAXI behavior, firmware modifications,
display publication, or unproven peripheral inheritance.

## Handoff

Record the two-run transcript and the next startup boundary toward the
reference Service Mode frame.

E-ULS-0057 records the new byte-identical standalone transcript. The
next fault is a DAXI read at `0x48000050` from PC `0x000778aa`.
E-ULS-0058 records the matching DAXI and narrow unbacked-access census.

Integrator review (2026-11-04): the BootROM and CLKGEN cases, all 63
Ulsan tests, the private two-run firmware gate, `make check-lines`,
`make check`, and `make sanitize` passed. The two-instance test verifies
clock state survives a reset on a different bus.
