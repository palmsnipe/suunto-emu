# 824 — Ulsan 2.44.52 UART3 Readiness Trace

**Status:** done
**Phase:** 7
**Dependencies:** 823

## Goal

Trace the native Race S UART3 open and readiness sequence. Capture
the first data exchange or identify the condition that keeps the BLE
adapter in START.

## Execution Budget

Bound each cold Renode boot to 100 virtual seconds and six host
minutes. Limit detailed logging to the first instances of each
observed transition and record counts through the stop point.

## Required Reading

Read ticket 823, E-ULS-0101 and its fixture references, the
execution and testing contracts, and the Race S UART3 mapping.
Read the Hello, Service Mode and onboarding regression entry points.

## Current Baseline

Ticket 823 observed successful UART3 open, repeated UART3 control
writes, and no word writes to the UART3 data register. The adapter
remained in START. The reference lane reports an unsupported CTS
enable bit during UART3 setup.

## Allowed Files

- This ticket and `plans/index.tsv`
- `docs/migration-evidence.md`

## Frozen Interfaces

Preserve the pinned firmware, fixtures, lane inputs, and standalone
Race S checkpoints.

## Evidence Inputs

Use E-ULS-0101's cold boot and input route. Keep probe sources and
raw logs in a volatile workspace.

## Implementation

Observe native UART3 control, status and data accesses. Trace the
GPIO, interrupt and driver state transitions that determine readiness,
and link them to the BLE adapter state.

## Tests and Commands

- `make test TEST_FILTER=ulsan` with the private manifest and eMMC
- `make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_` with private inputs
- `make check-task-contracts && make check-lines && make check`

## Acceptance

Two cold Renode runs capture the same first data exchange or
pinpoint the same native condition blocking the adapter. Record the
trace bounds, counts, and matching normalized digest. Pass Hello,
Service Mode, crown-only guidance, and mixed-input guidance.

## Forbidden Scope

Guest patches, manufactured controller replies, fixture mutations,
new runtime behavior, and tracked firmware or frame pixels.

## Handoff

E-ULS-0102 identifies the retry condition. UART3 reads `0x90` from
its flag register, with CTS clear. The UART driver returns to state
1, reads GPIO19 low, and reopens UART3. The BLE adapter stays in
START. No UART3 data access occurs within the confirmation window.

Both cold runs stop at 11,500 virtual ms after 18 recurring timer
retries. Their normalized SHA-256 is
`f845adcbbf98654ee078c2890b72ecf26036d46e8c7d615d0eade0e9e35220ef`.
The lane drops CTS enable from the UART control write. A CTS
assertion and the first radio exchange remain to be evidenced.

All 89 Race S cases pass, including two cold boots each for Hello,
Service Mode and the two pairing guidance routes. Both firmware-era
scripts, task contracts, line checks and `make check` pass.
