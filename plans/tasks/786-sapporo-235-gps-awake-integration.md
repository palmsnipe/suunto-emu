# 786 — Sapporo 2.35 GPS Awake Integration

**Status:** done
**Phase:** 7
**Dependencies:** 416,705

## Goal

Pass the observed periodic GPS awake checks using eight synthetic GPIO24
pulses, retaining native GPIO/IRQ handling and bounded refusal. Ticket710
instance15. Extend the viewable, controllable 2.35 setup window.

## Execution Budget

One version-specific fixture, existing integration seams, focused tests and
one private firmware runner. No generic device behavior changes.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`, tickets416/705/785, E-SAP-0047/0048,
`include/semu/compat.h`, `src/compat/layer.c`,
`src/compat/sapporo_235_gps.c/.h`, `sapporo_235_gps_reopen.c/.h`,
`src/devices/sapporo_cxd5610.c/.h`, all existing Allowed Files,
`tests/unit/test_sapporo_235_gps_reopen.c`.

## Current Baseline

The existing four explicit 2.35 layers pass startup/reopen but refuse GSTP
at about16s. Native state12 consumes a GPIO24 awake indication between polls.
The lane experiment supplies eight explicit pulses, while a zero-pulse
control reaches the GPS assertion. Both must reproduce before implementation.
This task uses existing initial/reopen contracts without changing their
budgets or adopting their ticket statuses.

## Allowed Files

- `src/compat/sapporo_235_gps_awake.c`, `src/compat/sapporo_235_gps_awake.h`
- `src/devices/sapporo_devices_internal.h`, `src/devices/sapporo_devices.c`
- `src/devices/sapporo_device_compat.c`
- `src/boards/machine.c`, `src/boards/machine_run.c`
- `tests/unit/test_sapporo_235_gps_awake.c`
- `tests/integration/test_firmware_sapporo_235_gps_awake.sh`
- `README.md`, `docs/current-status.md`, `docs/migration-evidence.md`

Machine registry/dispatch and private context headers are explicitly in scope.
Planning creates this ticket and its index row before implementation.

## Frozen Interfaces

Reuse existing scheduler-backed CXD awake signals. No CPU, scheduler, GPIO,
UART MMIO, public include API, profile, snapshot or old-layer changes.

## Evidence Inputs

E-SAP-0048: paired zero/eight-pulse lane censuses using the E-SAP-0047 startup,
reopen and controlled-storage prerequisites. Exact firmware hashes E-SAP-0038.
GPIO pulses are synthetic and do not claim physical receiver cadence.

## Implementation

Add `sapporo-2.35-gps-awake`, requiring both explicit GPS dependencies and
their completed two-hit lifecycles. At1259fe validate R8 driver, R4=100589fe,
R5=driver+26c, R6=driver+314, R7=driver+75, driver state12/pending10,
flags1/0/0 and GPIO24 config93. Admit exactly eight pulses, each after100ms
and high for1ms, using the existing transport. Immutable hash-pinned descriptor,
instance-owned counters, logged hits, sticky refusal on bad state/budget or
scheduling error. Ninth admission refuses. Reset clears bindings and pulses.
Dependency binding validates the complete set before mutation in any CLI order.

## Tests and Commands

`make test TEST_FILTER=sapporo_235_gps`; `make test TEST_FILTER=sapporo_cxd5610`;
`make check-task-contracts`; `make check-lines`; `make check`; `make sanitize`.
`make test-firmware TEST_PROFILE=sapporo-2.35.34 SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu`.
`sh tools/test_sdl_sapporo_235.sh` preserves the existing UI checkpoint.
Two bounded cold runs and two SDL setup walks with the new layer must match.

## Acceptance

Regression fails before implementation. Positive pulse timing, eight-hit bound,
wrong hash/profile/state/register/dependency, busy/overflow/scheduler failure,
reset/cancellation and independent machine ownership have focused coverage.
Native GPIO handler restores awake flag through normal IRQ delivery. Old
2.35 pins remain unchanged; next unsupported behavior is explicitly recorded.

## Forbidden Scope

No unbounded heartbeat, GSTP acceptance, fabricated fix/time, CPU/state patch,
firmware mutation, existing golden changes or 2.39 era repins.

## Handoff

Record files, commands, evidence and hashes; leave status for integrator review.
Eight observed synthetic pulses do not establish ongoing physical GPS support.

## Integrator acceptance (2026-09-23, delegated)

Verified on the consolidated commit: the ticket's recorded paired
firmware/SDL regressions and focused cases pass; `make check` reports 989 PASS
/ 0 FAIL, `make sanitize` 984 PASS / 0 FAIL, and 155 task contracts validate.
Status set `done`. Gaps listed in the handoff stay open and are owned by their
named follow-up tickets. Per the standing era-drift rule, 2.39 era pins may
have drifted after this change; tickets 777/783 own re-derivation.

