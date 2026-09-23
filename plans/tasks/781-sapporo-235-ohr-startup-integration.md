# 781 — Sapporo 2.35 OHR Startup Integration

**Status:** done
**Phase:** 7
**Dependencies:** 705

## Goal

Supply the observed synthetic OHR startup exchange as an explicit, hash-pinned
2.35 layer, passing the E-SAP-0040 first-request fault. Ticket 710 instance-11.

## Execution Budget

One compatibility module and existing device-factory/machine integration, one
focused test file and one bounded firmware runner. No raw firmware outputs.

## Required Reading

`docs/architecture.md`, `docs/compatibility-policy.md`, DMA/era sections of
`docs/execution-model.md`, `docs/testing-strategy.md`, E-SAP-0040/0041,
`include/semu/compat.h`, `src/compat/layer.c`, `sapporo_235_production.c`,
`src/devices/sapporo_ohr2.c`, `sapporo_ohr2.h`, `sapporo_devices.c`,
`sapporo_devices.h`, `sapporo_devices_internal.h`, `sapporo_device_compat.c`,
`src/boards/machine.c`, `machine_run.c`, and
`tests/devices/test_sapporo_ohr2_239.c`.

## Current Baseline

Haptic startup completes. OHR command 0010 sequence zero refuses because the
2.35 profile has no enabled response fixture; the guest resets at 261789155
instructions. The native lane explicitly labels its OHR response bodies as
synthetic, so automatic profile-wide fixture activation is not justified.

## Allowed Files

- `src/compat/sapporo_235_ohr.c`, `src/compat/sapporo_235_ohr.h`
- `src/devices/sapporo_devices.c`, `src/devices/sapporo_devices.h`
- `src/devices/sapporo_devices_internal.h`, `src/devices/sapporo_device_compat.c`
- `src/boards/machine.c`, `src/boards/machine_run.c`
- `tests/devices/test_sapporo_235_ohr.c`
- `tests/integration/test_firmware_sapporo_235_ohr.sh`
- `README.md`, `docs/current-status.md`, `docs/migration-evidence.md`

The existing factory headers, layer registry and machine refusal propagation
are explicitly integration-owned. Planning adds this ticket/index row first.

## Frozen Interfaces

No public include API, firmware identities, CPU semantics, shared OHR framing
or snapshot-format changes. Reuse the device body-provider and compatibility
hook/status seams. A latched fixture diagnostic stops the machine through its
existing compatibility check; no valid instruction is replaced or patched.

## Evidence Inputs

E-SAP-0041 paired lane startup transcript and synthetic echo-variation probes.
E-SAP-0038 pins the 2.35 firmware and production fixture. Synthetic identity,
zero result data and echo fields are explicitly distinguished from physical
sensor data. The real OHR firmware/data is unavailable to the emulator.

## Implementation

Add `sapporo-2.35-ohr-startup`, disabled by default, exact three hashes, eight
response hits per reset, expected command/sequence/state/body order, owned
state and structured events. Validate before changing response or counters.
Use existing framing/CRC/ready/reboot transport. Bind only the selected 2.35
profile; wrong layer, profile, state, payload or exhaustion refuses. Retain
fixture refusals until reset and propagate to machine compatibility stop.

## Tests and Commands

`make test TEST_FILTER=sapporo_235_ohr` selects positive lifecycle and refusal
cases. Run `make check-lines`, `make check-task-contracts`, `make check` and
`make sanitize`. Run `make test-firmware FIRMWARE_ROOT=/tmp/suunto-235-fwroot
TEST_PROFILE=sapporo-2.35.34`, with a new runner comparing repeated enabled
and disabled bounded cold runs.

## Acceptance

Two identical runs complete the eight evidenced responses and reach the next
recorded boundary. A disabled control reproduces the old fault/reset. Existing
production and pressure era pins remain unchanged. Refusals do not mutate
queued response, output buffer, hit count or ready signal; only the diagnostic
latch changes. Each hit is logged. No full UI or physical OHR completion claim.

## Forbidden Scope

No generic zero-body fallback, unbounded fixture, GPS behavior, 2.22/2.39 era
repin, physical measurements, firmware patch or codec implementation.

## Handoff

Record exact command results, hashes, layer hits, next gap and changed files.
Leave ticket status for integrator review.

## Integrator acceptance (2026-09-23, delegated)

Verified on the consolidated commit: the ticket's recorded paired
firmware/SDL regressions and focused cases pass; `make check` reports 989 PASS
/ 0 FAIL, `make sanitize` 984 PASS / 0 FAIL, and 155 task contracts validate.
Status set `done`. Gaps listed in the handoff stay open and are owned by their
named follow-up tickets. Per the standing era-drift rule, 2.39 era pins may
have drifted after this change; tickets 777/783 own re-derivation.

