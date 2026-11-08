# 811 — Ulsan 2.44.52 Early Bus Attachments

**Status:** done
**Phase:** 7
**Dependencies:** 810

## Goal

Pass the DAXI and observed early unbacked reads using shared, evidence-bounded
2.44.52 attachments. Pin the first SystemTimer boundary.

## Execution Budget

One startup integration instance. Two cold boots with 20,000,000
instructions and 1,000,000,000 virtual ns. No controller expansion beyond
the recorded early accesses.

## Required Reading

Read ticket 810, E-ULS-0011 through E-ULS-0013, E-ULS-0038,
E-ULS-0057 and E-ULS-0058 in `docs/migration-evidence.md`,
`docs/adding-a-device.md`, the execution-model memory and DMA section,
`include/semu/bus.h`, `src/devices/ulsan_{daxi,otpinfo,buszero}.{c,h}`,
`src/boards/ulsan_board.c`, `tests/devices/test_ulsan_{daxi,otpinfo,buszero}.c`
and `tests/integration/test_firmware_ulsan_244_reset.sh`.

## Current Baseline

2.44.52 passes the existing power, BootROM and clock attachments. The first
unmapped read is DAXI +0x50 at PC `0x000778aa`. Scratch probes reach the
INFO1, CRYPTO and SystemTimer boundaries in order.

## Allowed Files

- `src/boards/ulsan_board.c`, `src/devices/ulsan_buszero.{c,h}`
- `tests/devices/test_ulsan_buszero.c`,
  `tests/integration/test_firmware_ulsan_244_reset.sh`
- `docs/migration-evidence.md` and this ticket's handoff

## Frozen Interfaces

Keep the public APIs and older Ulsan mappings intact. Unknown transactions
refuse. The missing INFO1 and CRYPTO words represent only lane defaults.

## Evidence Inputs

E-ULS-0058 pins both the DAXI script accesses and the observed exact-width
INFO1, CRYPTO and logger accesses. The DAXI module's two silenced ranges
are declared in the identical platform file.

## Implementation

Reuse the DAXI and INFO1 maps. Select only the proven CRYPTO and logger
tuples from the existing narrow bus-zero model for 2.44.52. Preserve all
other bus faults and pin the next startup checkpoint.

## Tests and Commands

- `make test TEST_FILTER=ulsan_daxi`
- `make test TEST_FILTER=ulsan_otpinfo`
- `make test TEST_FILTER=ulsan_buszero`
- `make test TEST_FILTER=ulsan`
- `SEMU_FIRMWARE_MANIFEST=/path/to/firmware.semu make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_reset`
- `make check-lines && make check && make sanitize`

## Acceptance

The 2.44.52 map serves the observed tuples and refuses an unobserved access.
Two independent cold boots reach the next fault with identical transcripts.
The existing Ulsan pins and older mapping tests pass. Preserve the 810
checkpoint in the ledger.

## Forbidden Scope

SystemTimer behavior, generic read-as-zero, personalization values, extra
fallback windows, and frame publication.

## Handoff

Record the next precise fault and the remaining work toward the Service Mode
frame.

E-ULS-0059 records the byte-identical standalone checkpoint at the
SystemTimer CONFIG read. E-ULS-0060 records the matching SystemTimer and
watchdog observations for the next integration.

Integrator review (2026-11-04): the focused DAXI, INFO1 and bus-zero
tests, all 64 Ulsan tests, the private two-run firmware gate,
`make check-lines`, `make check`, and `make sanitize` passed. The
2.44.52 test verifies that the older unrelated windows are unmapped.
