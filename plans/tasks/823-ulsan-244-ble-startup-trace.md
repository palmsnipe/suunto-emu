# 823 — Ulsan 2.44.52 Native BLE Startup Trace

**Status:** ready
**Phase:** 7
**Dependencies:** 822

## Goal

Follow the native Race S pairing flow to the first BLE IOM4 transaction
or identify the firmware decision that keeps it from issuing one.

## Execution Budget

Allow at most 100 virtual seconds and six host minutes per cold Renode
run. Bound per-address trace output and record the actual stop point.

## Required Reading

Read ticket 822, E-ULS-0079, E-ULS-0088, E-ULS-0098, E-ULS-0100, the
execution, testing and compatibility contracts, and the reference
lane's Race S full UI checkpoint and IOM4 implementation. Read the
existing Hello, Service Mode, and onboarding regression entry points.

## Current Baseline

The reference lane and standalone emulator reach the pairing guidance
bottom. The reference's earlier provider response trace reports
`BleP` startup success without observed BLE SPI traffic.

## Allowed Files

- This ticket and `plans/index.tsv`
- `docs/migration-evidence.md`

## Frozen Interfaces

Preserve the existing native UI, firmware and fixture identities, and
Hello, Service Mode and onboarding checkpoints.

## Evidence Inputs

Use the hash-pinned 2.44.52 application, the existing production and
storage fixtures, and the E-ULS-0098 and E-ULS-0100 lane input schedule.
Keep raw traces and probe sources in a volatile workspace.

## Implementation

Trace the provider instance, settings access and adapter decision with
passive observations. Record IOM4 command writes and the controller
mode so I2C traffic and BLE SPI activity can be distinguished.

## Tests and Commands

- `make test TEST_FILTER=ulsan` with private manifest and eMMC
- `make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_` with private inputs
- `make check-task-contracts && make check-lines && make check`

## Acceptance

Two cold Renode boots reproduce the same native startup decision and
either capture the first BLE IOM4 transaction or identify the condition
that prevents it. Record the trace boundary and digest, and pass the
standalone Race S regressions.

## Forbidden Scope

Guest patches, settings or fixture mutations, synthetic pairing success,
new BLE transport behavior, and tracked firmware or frame pixels.

## Handoff

E-ULS-0101 identifies the native startup path. The BLE adapter
enters START and opens UART3 at `0x4001f000`. The later pairing
enable request returns at the adapter readiness gate
`0xd75c8` before dispatch. Both cold Renode boots issue 219 IOM4
commands, all I2C, and reach the exact pairing guidance bottom at
32,850 virtual ms. Each retains the same frame through 37,850 ms.
The normalized trace SHA-256 is
`0e9ec54aa64ae2594e6ae7338eaea42f7eefc626e6979f757245af9a02cce052`.
The UART3 exchange and the adapter's START-to-ON transition are the
remaining evidence gap.

All 89 Race S test cases pass, including two cold boots each for
Hello, Service Mode, crown-only guidance, and mixed-input guidance.
Both firmware-era scripts pass. Task contracts, line review, and
`make check` pass.
