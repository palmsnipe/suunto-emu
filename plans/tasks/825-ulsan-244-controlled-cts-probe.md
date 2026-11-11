# 825 — Ulsan 2.44.52 Controlled UART3 CTS Probe

**Status:** ready
**Phase:** 7
**Dependencies:** 824

## Goal

Isolate the CTS retry loop in the Race S reference lane. Capture the
first native UART3 radio traffic or identify the next blocking decision.

## Execution Budget

Allow 100 virtual seconds and six host minutes per cold boot. Log only
the first four visits to each selected instruction or event key, plus
counters and the final state.

## Required Reading

Read ticket 824, E-ULS-0101, E-ULS-0102, the execution, testing and
compatibility contracts, and the lane's UART3 platform mapping and PL011
model. Read the existing Hello, Service Mode, and onboarding tests.

## Current Baseline

The passive lane drops UARTCR.CTSEn and reports UARTFR.CTS clear.
The native driver repeatedly closes and reopens UART3, and the BLE
adapter stays in START.

## Allowed Files

- This ticket and `plans/index.tsv`
- `docs/migration-evidence.md`

## Frozen Interfaces

Preserve the standalone runtime, firmware, reference lane, media, and
frame checkpoints. Place temporary probe inputs and logs outside both
repositories.

## Evidence Inputs

Use the E-ULS-0101 pinned application and crown-only route, the
E-ULS-0102 retry trace, and PL011 UARTCR and UARTFR semantics.

## Implementation

Use a disposable UART3 wrapper to preserve the written CTS-enable bit.
Compare cold runs with CTS clear and asserted. Record the native
status reads, driver and adapter transitions, and UART3 data access
and transmit counts through pairing guidance.

## Tests and Commands

- `make test TEST_FILTER=ulsan` with private manifest and eMMC
- `make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_` with private inputs
- `make check-task-contracts && make check-lines && make check`

## Acceptance

The clear-CTS control reproduces the retry. Two cold CTS-asserted
runs reproduce the first native data exchange or the next blocking
decision. Record the trace limits, counts, and normalized digest.
Pass Hello, Service Mode, and both pairing guidance routes.

## Forbidden Scope

Guest patches, radio replies, forced adapter state, runtime changes,
and tracked firmware, frame pixels or raw logs.

## Handoff

E-ULS-0103 records two cold boots per CTS setting. With CTS clear,
UART3 closes 19 times by 11,500 virtual ms. With CTS asserted, it
opens once and remains open through pairing guidance and the final
sample at 37,750 ms.
Both asserted runs record zero UART3 data accesses or outgoing bytes.
The BLE adapter remains in START. The firmware waits for the peer's
initial valid frame. Its bytes and timing are unobserved.

The normalized asserted trace SHA-256 is
`841e5c4ade07248894e2f8096e2a158be0d5059c6092def64002913b98f39ef9`.
The normalized control trace SHA-256 is
`1a0be816f73133c314da71e6aa01fbd9e9e7e8c0160c4185bc2dc94262df6f11`.
