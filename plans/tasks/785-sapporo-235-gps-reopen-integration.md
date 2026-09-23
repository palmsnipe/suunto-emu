# 785 — Sapporo 2.35 GPS Reopen Integration

**Status:** done
**Phase:** 7
**Dependencies:** 416,705

## Goal

Pass the later UART-reopen timeout through the two E-SAP-0047 synthetic
responses while preserving native parsing. Ticket 710 instance-14.

## Execution Budget

One version-specific compatibility module, focused tests, existing attachment
seams and a bounded private firmware runner.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`, tickets 416/705/784, E-SAP-0046/0047,
`include/semu/compat.h`, `src/compat/layer.c`,
`src/compat/sapporo_235_gps.c` and `.h`,
`src/devices/sapporo_cxd5610.c` and `.h`, all existing Allowed Files,
and `tests/unit/test_sapporo_235_gps.c`.

## Current Baseline

The existing production/OHR/initial-GPS layers reach the UART reopen, then
refuse a second startup at 1254ec after timeout. E-SAP-0047 reproduces the
missing reopen response and exact GSR reply twice in the lane. This task uses
the existing initial fixture without changing or adopting ticket784's status.
Its frozen two-hit contract and all old checkpoints remain available.

## Allowed Files

- `src/compat/sapporo_235_gps_reopen.c`, `src/compat/sapporo_235_gps_reopen.h`
- `src/devices/sapporo_devices.c`, `src/devices/sapporo_devices.h`
- `src/devices/sapporo_devices_internal.h`, `src/devices/sapporo_device_compat.c`
- `src/boards/machine.c`, `src/boards/machine_run.c`
- `tests/unit/test_sapporo_235_gps_reopen.c`
- `tests/integration/test_firmware_sapporo_235_gps_reopen.sh`
- `README.md`, `docs/current-status.md`, `docs/migration-evidence.md`

Factory headers and machine layer registry/dispatch are explicitly in scope.
Planning creates this ticket and its index row before implementation.

## Frozen Interfaces

No CPU, UART MMIO, scheduler, storage, public include API, profile metadata or
snapshot-format changes. Existing 2.35 snapshot refusal remains. Reuse the
delayed CXD RX path and existing compatibility status propagation.

## Evidence Inputs

E-SAP-0047 supplies exact reopen registers, driver flags and native state
7/8/9/10/12 dispatch. E-SAP-0046 supplies the initial dependency. All three
firmware hashes are E-SAP-0038. The status lines are explicitly synthetic.

## Implementation

Add `sapporo-2.35-gps-reopen`, requiring the explicitly selected startup layer.
At 125664 validate R4/R5/R6, UART callback1250e7, driver+74/+7b/+7f values
15/0/2, state/pending4/7, seen1/retry0 at100589ff/10058a00. Inject the
synthetic ten-byte status after10ms once, then answer exact `@GSR\r\n` once
with the same delayed status. Two total hits, instance-owned, logged and
hash-pinned. Check dependency completion before each action. Every other
command/lifecycle refuses before RX/hits/log changes, with a sticky diagnostic
propagated before another guest instruction. Reset clears bindings/events.
Preserve initial-only routing and allow either CLI order for both layers.

## Tests and Commands

Run `make test TEST_FILTER=sapporo_235_gps`,
`make test TEST_FILTER=sapporo_cxd5610`, `make check-task-contracts`,
`make check-lines`, `make check`, `make sanitize`.
Run `make test-firmware TEST_PROFILE=sapporo-2.35.34
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu`.
Run `sh tools/test_sdl_sapporo_235.sh` to preserve the existing two-layer UI.
Compare two new-layer bounded runs with exact logs and stop tuples.

## Acceptance

Regression fails before implementation. Delayed RX, native states 7/8/9/10/12,
zero retry, two reopen hits and next boundary reproduce twice. Wrong hashes/profile,
disabled/busy/exhausted fixture, bad state/pointers/command, schedule/time
failure, isolated machine ownership and reset all have focused coverage.
Old 2.35 gates remain unchanged. Other profiles never activate this fixture.

## Forbidden Scope

No initial-layer budget change, GSTP reply, awake heartbeat, GPS fix/time, firmware/state patch,
unbounded replay, existing golden changes or 2.39 era repins.

## Handoff

Record evidence, changed files, exact commands/hashes, hits and next gap.
Leave status for integrator review; reopen startup is not full GPS support.

## Integrator acceptance (2026-09-23, delegated)

Verified on the consolidated commit: the ticket's recorded paired
firmware/SDL regressions and focused cases pass; `make check` reports 989 PASS
/ 0 FAIL, `make sanitize` 984 PASS / 0 FAIL, and 155 task contracts validate.
Status set `done`. Gaps listed in the handoff stay open and are owned by their
named follow-up tickets. Per the standing era-drift rule, 2.39 era pins may
have drifted after this change; tickets 777/783 own re-derivation.

