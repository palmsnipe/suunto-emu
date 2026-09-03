# 732 — Sapporo 2.39 IOM4 Haptic Selector

**Status:** in-progress
**Phase:** 7
**Dependencies:** 315, 413, 729

## Goal

Correct the exact IOM4 register-selector transfer used by Sapporo 2.39 haptic
autotune polling, then advance authentic execution to the next fail-closed
boundary.

## Execution Budget

One model-day for fault attribution, evidence pinning, one address-scoped IOM
correction, focused positive/refusal coverage, and two deterministic firmware
runs.

## Required Reading

Tickets 315, 413, 729, and 731; E-A4-IOM-001, E-SAP-HAPTIC-001, and
E-SAP-LPS22-239-001; `docs/{architecture,execution-model,testing-strategy,
compatibility-policy}.md`; `src/soc/apollo4/iom.c` and its focused tests;
`src/devices/sapporo_haptic.*` and its focused tests; plus every read-only
firmware source named below.

## Current Baseline

The post-ticket-731 Sapporo 2.39 run reaches a forced HardFault followed by a
firmware `SYSRESETREQ` at instruction 122,452,650. The precise bus fault is the
IOM4 command write at `0x40054120`; its PSP frame has stacked PC `0x0014e8ee`,
LR `0x000a5d13`, r1 `0x22000112`, r2 `0x22`, and r3 `1`. IOM4 is configured
for a one-byte P2M transfer at I2C address `0x50`, but the adapter recovers a
stale selector byte `0xa0` from `dma_target - 8`, so the strict haptic endpoint
correctly refuses the unsupported span.

## Allowed Files

- `src/soc/apollo4/iom.c`
- `tests/devices/test_apollo4_iom_haptic.c`
- `tests/integration/test_firmware_sapporo_239_haptic.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/732-sapporo-239-iom4-haptic-selector.md`

## Frozen Interfaces

CPU, DMA, serial endpoint, haptic PMIC, snapshot, profile, compatibility, and
all non-`0x50` IOM address behavior remain unchanged. In particular the
fuel-gauge selector recovery at address `0x36` remains intact.

## Evidence Inputs

E-SAP-IOM4-HAPTIC-239-001 must cite read-only
`$FIRMWARE_ROOT/emulator/results/sapporo-apollo4-iom-dma-boundary.trace`
SHA-256 `f9a02ffdc2ac41bab2cf32b58c5ee1723fa5468de01e3ee9b03f14b5075d95ea`,
its summary `69fbd57f07e350b67d25f846b730b5a85f28c36313b23dc9d9464aa4d6e2daed`,
`docs/research/feedback-startup-haptic.md`
`64858799bdfe96e56b918051e05051ea52cc7c4729f11e0c11a9f70219d6a13e`,
the haptic endpoint `fc7533bbcca2d6cb15d22edf0b40bb884c7df702d58f9f2bac0bbd76b291e968`,
and IOM wrapper `b1d1dc8ee41ed84a52d22e3dd510ffb88950619638c93fc57cb43facfe8f1be7`.
The trace pins address `0x50`, one-byte P2M direction, command `0x22000112`,
and distinct non-fixed offset descriptors while the haptic research pins
register `0x22` as the native autotune poll.

## Implementation

For P2M commands at the evidenced haptic address only, forward the command's
high byte as the one-byte endpoint selector. Preserve the existing address
allowlist and all other selector sources. Refuse through the existing endpoint
and DMA error path when the command-encoded selector is unsupported.

## Tests and Commands

`make test TEST_FILTER=apollo4_iom_haptic`, `make test
TEST_FILTER=apollo4_iom`, `make test TEST_FILTER=sapporo_haptic`, `make
test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_haptic`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=apollo4_iom_haptic`
must pass.

The focused regression must first fail against the current adapter. It covers
the exact `0x50`/`0x22000112` success with contradictory stale adjacent SRAM,
plus a non-`0x50` case proving the selector rule was not widened. The private
runner requires the exact external flash hash, two byte-identical runs,
unchanged source flash, absence of the diagnosed reset, and a later explicit
checkpoint or fail-closed boundary.

## Acceptance

The exact Sapporo 2.39 firmware completes the haptic `0x22` read twice
identically without the diagnosed HardFault or a compatibility hit; the
source flash remains unchanged; non-haptic IOM behavior and haptic endpoint
semantics remain unchanged; and execution reaches a later checkpoint or newly
identified fail-closed boundary.

## Forbidden Scope

No generic register-selector inference, non-`0x50` IOM behavior change,
haptic register/state change, compatibility hook, reset suppression, firmware
byte, profile, snapshot, renderer, or unrelated change.

## Handoff

Implemented the exact address-`0x50` P2M selector correction: command high byte
`0x22` now reaches the existing strict haptic endpoint, while address `0x36`
retains its prior selector recovery and all other addresses remain unchanged.
The focused regression first failed at the command write with contradictory
adjacent SRAM byte `0xa0`, then passed after the correction; its non-haptic
scope case passed before and after.

Evidence: E-SAP-IOM4-HAPTIC-239-001 and its five hash-pinned read-only sources.
The exact 32-MiB source flash remained SHA-256
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
Two fresh runs stop without reset at PC `0x0014e8ea`, instruction 122,457,908,
virtual time 1,230,996,595 ns; log SHA-256
`f1c41ec3d40617174d8cbb299883c69b028a6bfb445b44a0bb7aaf2622915354`
and snapshot SHA-256
`629ba604acfbb1eb1265a755283c6133b9650d45dcf2c44d9439752365e22247`.
One-instruction continuation enters the next fault vector at PC `0x001c0db4`.

The next boundary is command `0x23000112`, a haptic calibration-register
`0x23` read supported by the native research but intentionally absent from the
strict in-tree endpoint. Register `0x24` follows in the reference path; neither
value, haptic state, generic selector rule, nor compatibility behavior was
invented here. The integrator owns any roadmap-status transition.
