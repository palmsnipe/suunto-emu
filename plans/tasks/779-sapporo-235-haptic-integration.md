# 779 — Sapporo 2.35 Haptic Integration

**Status:** done
**Phase:** 7
**Dependencies:** 705

## Goal

Complete the observed IOM4 address 0x50 startup transactions under E-SAP-0040,
advancing past the measurement task's haptic wait. This is ticket 710 instance-10.

## Execution Budget

One bounded endpoint module, existing IOM4 attachment, focused tests and paired
bounded firmware checks. No firmware-derived output enters Git.

## Required Reading

`docs/architecture.md`, DMA and era sections of `docs/execution-model.md`,
`docs/testing-strategy.md`, E-SAP-0036/0038/0039/0040 in
`docs/migration-evidence.md`, `include/semu/bus.h`,
`src/devices/sapporo_iom4.c`, `sapporo_iom4.h`, `sapporo_iom4_internal.h`,
`sapporo_iom4_regs.c`, `sapporo_iom4_gauge.h`, and
`tests/devices/test_sapporo_iom4_dma.c`.

## Current Baseline

The 2.35 production layer passes negative pressure probes but IOM4 rejects
0x50, leaving the measurement task retrying haptic writes. The lane completes
105 startup haptic transactions and proceeds to OHR.

## Allowed Files

- `src/devices/sapporo_iom4.c`
- `src/devices/sapporo_iom4_regs.c`
- `src/devices/sapporo_iom4.h`
- `src/devices/sapporo_iom4_internal.h`
- `src/devices/sapporo_iom4_haptic.c`
- `tests/devices/test_sapporo_iom4_haptic.c`
- `tests/integration/test_firmware_sapporo_235_production.sh`
- `tests/integration/test_firmware_sapporo_235_pressure.sh`
- `docs/migration-evidence.md`, `docs/current-status.md`, `README.md`

Existing IOM4 headers/attachment are explicitly integration-owned here.
Planning creates this ticket and index row before implementation.

## Frozen Interfaces

No public API, profile identity, snapshot format, CPU or other profile changes.
The existing profile-specific IOM4 owns endpoint state. No parallel bus API.

## Evidence Inputs

E-SAP-0040 paired native startup census and halted synthetic DMA readouts.
E-SAP-0036 supplies the existing wrapper/controller law; E-SAP-0038 supplies
exact firmware identity and opt-in production fixture.

## Implementation

Admit only observed DMA command shapes, register selectors and complete memory
ranges before any mutation. Retain lane chunking (4+1 on five-byte writes),
configuration readbacks and synchronous autotune completion. Refuse unsupported
shape/state/register atomically. Reset owns all endpoint state.

## Tests and Commands

`make test TEST_FILTER=sapporo_iom4_haptic` must select positive and refusal
cases. Run `make check-lines`, `make check`, `make sanitize` and
`make check-task-contracts`. Run `make test-firmware
FIRMWARE_ROOT=/tmp/suunto-235-fwroot TEST_PROFILE=sapporo-2.35.34`.

## Acceptance

Paired firmware executions advance past haptic to a documented next boundary.
Re-derive affected 2.35 suffix pins only after attributing changes to the new
haptic completion; preserve the 30M prefix, strict reason/hash checks and reset
refusal. Record both old and new tuples/hashes. Other profile pins stay fixed.

## Forbidden Scope

No generic register fallback, physical vibration claim, OHR implementation,
magnetometer change, firmware patch, or unrelated golden update.

## Handoff

Report exact tests, evidence/log hashes, next unsupported boundary and changed
files. Leave status to integrator review; this is not full release completion.

## Integrator acceptance (2026-09-23, delegated)

Verified on the consolidated commit: the ticket's recorded paired
firmware/SDL regressions and focused cases pass; `make check` reports 989 PASS
/ 0 FAIL, `make sanitize` 984 PASS / 0 FAIL, and 155 task contracts validate.
Status set `done`. Gaps listed in the handoff stay open and are owned by their
named follow-up tickets. Per the standing era-drift rule, 2.39 era pins may
have drifted after this change; tickets 777/783 own re-derivation.

