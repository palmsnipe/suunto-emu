# 778 — Sapporo 2.35 Negative Pressure Probe Integration

**Status:** done
**Phase:** 7
**Dependencies:** 705

## Goal

Integrate the E-SAP-0039 negative LPS22 identity probes into the exact Sapporo
2.35 IOM2 profile and advance beyond the E-SAP-0038 pressure fault/reset.
This integration scope follows ticket 710 instance-9 lane derivation.

## Execution Budget

One bounded controller module, five attachment/header/codec files, one focused
C test, one new firmware script and the preceding production script update.
No firmware-derived fixture bytes enter Git.

## Required Reading

`docs/architecture.md` ownership/fail-closed rules, `docs/execution-model.md`
whole-operation admission, `docs/testing-strategy.md`, E-SAP-0038 and
E-SAP-0039 in `docs/migration-evidence.md`, `include/semu/bus.h`,
`src/soc/apollo4/iom.h`, `iom_internal.h`, `iom.c`, `iom_snapshot.c`,
`apollo4.c`, `tests/devices/test_apollo4_iom.c` and
`tests/integration/test_firmware_sapporo_235_production.sh`.

## Current Baseline

The explicit 2.35 production-data layer selects normal mode 5. A one-byte
WHO_AM_I probe at I2C 0x5c then triggers an emulator refusal and guest reset.
The unmodified lane also probes 0x5d, with both sensors absent, and continues.
Generic endpoint success would lose the observed FIFO-underflow status.

## Allowed Files

- `src/soc/apollo4/iom_sapporo235.c`
- `src/soc/apollo4/iom.c`
- `src/soc/apollo4/iom.h`
- `src/soc/apollo4/iom_internal.h`
- `src/soc/apollo4/iom_snapshot.c`
- `src/soc/apollo4/apollo4.c`
- `tests/devices/test_sapporo_235_pressure.c`
- `tests/integration/test_firmware_sapporo_235_pressure.sh`
- `tests/integration/test_firmware_sapporo_235_production.sh`
- `docs/migration-evidence.md`, `docs/current-status.md`, `README.md`

The IOM internal header and existing SoC profile attachment are explicitly
integration-owned here. Planning added this ticket/index row before coding;
implementation must not update its status.

## Frozen Interfaces

No public `include/semu/**` API, profile identity/hash, CLI option, persistent
format or shared-profile behavior changes. Extend the existing IOM profile
attachment; no parallel bus or endpoint API. 2.35 snapshots remain unsupported.

## Evidence Inputs

E-SAP-0039: two byte-identical derived lane censuses with full raw-log hashes,
exact native probe shapes and halted-controller readouts. E-SAP-0038 supplies
the explicit production records and preceding normal-mode checkpoint.

## Implementation

Gate only IOM2 under `sapporo-2.35.34`. Recognize command `0x0f000112`, DMA
count 1, receive configuration, and addresses 0x5c/0x5d. Validate the complete
RAM write before mutation. Return zero with the observed DMA-complete,
FIFO-underflow/threshold and trigger state; retain configuration bit 8 in
this scoped controller. Wrong shape/direction/target refuses. Preserve other
profiles and devices. Refuse direct snapshots of the new controller state.

## Tests and Commands

- `make test TEST_FILTER=sapporo_235_pressure`: select the new C test, exit 0;
  exact positive tuple, refusal atomicity, profile isolation and reset.
- `make check-lines && make check && make sanitize`: exit 0.
- `make test-firmware FIRMWARE_ROOT=/tmp/suunto-235-fwroot TEST_PROFILE=sapporo-2.35.34`:
  validate private inputs and run both bounded scripts twice identically.
- Re-derive the obsolete pressure-fault suffix in the production runner only
  after positive/refusal tests and paired authentic runs identify the new
  boundary. Retain the unchanged 30M-instruction prefix and record old/new
  hashes; replace the old fault expectation with stronger no-reset progress.

## Acceptance

Both negative probes complete without fabricating sensor presence. Paired
firmware runs pass the former reset boundary, with the next unsupported
operation documented. No unrelated profile regression or source-image writes.
Full Sapporo release completion is not implied.

## Forbidden Scope

No generic absent-device read-as-zero, 2.39 LPS22 attachment to 2.35, positive
LPS22 samples, command fallback, firmware patch, OHR behavior, CPU change or
unrelated golden updates. Do not relax checks just to obtain a passing run.

## Handoff

Record files, exact commands/results, E-SAP-0039, old/new stop tuples and log
hashes, next gap and snapshot refusal. Leave status to integrator review.

## Integrator acceptance (2026-09-23, delegated)

Verified on the consolidated commit: the ticket's recorded paired
firmware/SDL regressions and focused cases pass; `make check` reports 989 PASS
/ 0 FAIL, `make sanitize` 984 PASS / 0 FAIL, and 155 task contracts validate.
Status set `done`. Gaps listed in the handoff stay open and are owned by their
named follow-up tickets. Per the standing era-drift rule, 2.39 era pins may
have drifted after this change; tickets 777/783 own re-derivation.

