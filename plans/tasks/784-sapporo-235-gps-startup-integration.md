# 784 — Sapporo 2.35 GPS Startup Integration

**Status:** done
**Phase:** 7
**Dependencies:** 416,705

## Goal

Pass the observed initial GPS timeout through two explicit synthetic UART
responses, preserving native parsing and execution. Ticket 710 instance-13.

## Execution Budget

One version-specific compatibility module, focused tests, existing attachment
seams and a bounded private firmware runner.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`, tickets 416/705, E-SAP-0044/0046,
`include/semu/compat.h`, `src/compat/layer.c`,
`src/compat/sapporo_239_gps.c`, `src/compat/sapporo_235_ohr.c`,
`src/devices/sapporo_cxd5610.c` and `.h`, all existing Allowed Files,
and `tests/unit/test_sapporo_239_gps.c`.

## Current Baseline

With production/OHR layers, initial UART open at 1254ec receives nothing.
Retry three reaches `CXD5610GF-driver.cpp:894`. The interpreter and lane agree.
The prior storage changes are present; this task neither adopts nor modifies
their shared law. Existing two-layer checkpoints must remain unchanged.

## Allowed Files

- `src/compat/sapporo_235_gps.c`, `src/compat/sapporo_235_gps.h`
- `src/devices/sapporo_devices.c`, `src/devices/sapporo_devices.h`
- `src/devices/sapporo_devices_internal.h`, `src/devices/sapporo_device_compat.c`
- `src/boards/machine.c`, `src/boards/machine_run.c`
- `tests/unit/test_sapporo_235_gps.c`
- `tests/integration/test_firmware_sapporo_235_gps_startup.sh`
- `README.md`, `docs/current-status.md`, `docs/migration-evidence.md`

Factory headers and machine layer registry/dispatch are explicitly in scope.
Planning creates this ticket and its index row before implementation.

## Frozen Interfaces

No CPU, UART MMIO, scheduler, storage, public include API, profile metadata or
snapshot-format changes. Existing 2.35 snapshot refusal remains. Reuse the
delayed CXD RX path and existing compatibility status propagation.

## Evidence Inputs

E-SAP-0046: two identical normalized lane runs with synthetic `$PSS0000`
startup and exact `@VER` response, two wrong-prefix controls, pristine parser
and driver predicates. All three firmware hashes are E-SAP-0038.
Status payload fields are synthetic, not physical status, time or location.

## Implementation

Add opt-in `sapporo-2.35-gps-startup`, three exact component hashes, two total
hits per explicit machine reset. At 1254ec validate driver/UART spans,
callback 1250e7, states 4/2 and zero retry at 10058a00; queue ten bytes after
10 ms. Accept one exact `@VER\r\n` only afterward; queue the same line after
10 ms. Log both named effects. Reject other commands, lifecycle or pointers
before RX/counter/log mutation. A fixture refusal latch may change and must
stop the machine before guest reset silently replenishes its budget.

## Tests and Commands

Run `make test TEST_FILTER=sapporo_235_gps`,
`make test TEST_FILTER=sapporo_cxd5610`, `make check-task-contracts`,
`make check-lines`, `make check`, `make sanitize`.
Run `make test-firmware TEST_PROFILE=sapporo-2.35.34
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu`.
Run `sh tools/test_sdl_sapporo_235.sh` to preserve the existing two-layer UI.
Compare two new-layer bounded runs with exact logs and stop tuples.

## Acceptance

Regression fails before implementation. Delayed RX, native states 2/14/15,
zero retry, two hits and next boundary reproduce twice. Wrong hashes/profile,
disabled/busy/exhausted fixture, bad state/pointers/command, schedule/time
failure, isolated machine ownership and reset all have focused coverage.
Old 2.35 gates remain unchanged. Other profiles never activate this fixture.

## Forbidden Scope

No UART reopen reply, awake heartbeat, GPS fix/time, firmware/state patch,
unbounded replay, existing golden changes or 2.39 era repins.

## Handoff

Record evidence, changed files, exact commands/hashes, hits and next gap.
Leave status for integrator review; initial startup is not full GPS support.

## Integrator acceptance (2026-09-23, delegated)

Verified on the consolidated commit: the ticket's recorded paired
firmware/SDL regressions and focused cases pass; `make check` reports 989 PASS
/ 0 FAIL, `make sanitize` 984 PASS / 0 FAIL, and 155 task contracts validate.
Status set `done`. Gaps listed in the handoff stay open and are owned by their
named follow-up tickets. Per the standing era-drift rule, 2.39 era pins may
have drifted after this change; tickets 777/783 own re-derivation.

