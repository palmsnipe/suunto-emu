# 711 — Sapporo 2.39 DSP Memory Power

**Status:** done
**Phase:** 7
**Dependencies:** 706

## Goal

Implement the one E-SAP-0018 Apollo4 DSP0/DSP1 memory-power register cluster
required by the exact Sapporo `2.39.20.22297-P` profile, then stop at the next
distinct Reset/BoD routing boundary.

## Execution Budget

One model-day for six register semantics, snapshot coverage, focused refusal
tests, and two authentic-firmware runs.

## Required Reading

Ticket 706 handoff, E-SAP-0018, `docs/architecture.md`,
`docs/execution-model.md`, `docs/testing-strategy.md`, `src/soc/apollo4/power.c`,
and `tests/devices/test_apollo4_power.c`.

## Current Baseline

The exact built-in profile resets at 11,897,027 ns after a precise access fault
at `PWRCTRL.DSP0MEMPWREN` (`0x40021058`). The existing power controller refuses
all six DSP memory offsets.

## Allowed Files

- `src/soc/apollo4/power.c`
- `tests/devices/test_apollo4_power.c`
- `tests/integration/test_firmware_sapporo_239_profile.sh`
- `tests/integration/test_firmware_sapporo_239_power.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/711-sapporo-239-dsp-power.md`

## Frozen Interfaces

Public APIs, snapshot container format, existing power registers/callbacks,
profile registries, and every 2.22/2.33 checkpoint remain unchanged. No new
power-gate callback is exposed because the observed registers gate no modeled
consumer.

## Evidence Inputs

E-SAP-0018 pins DSP0 enable/status/retention offsets `0x58/0x5c/0x60`, DSP1
offsets `0x78/0x7c/0x80`, two enable/status bits, five retention bits, zero
reset values, status mirroring the corresponding enable request, and the exact
firmware access order. It also pins the next precise fault at Reset/BoD routing
after the watchdog base is reached.

## Implementation

Store masked DSP0/DSP1 enable and retention state, expose read-only status that
mirrors each enable request, reset all six registers to zero, serialize the four
writable values, and reject reserved bits, status writes, wrong widths, and
adjacent offsets without mutation. Rerun the exact firmware until the separate
Reset/BoD/watchdog initialization gap.

## Tests and Commands

`make test TEST_FILTER=apollo4_power`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_power`, `make check-lines`,
`make check`, and `make sanitize TEST_FILTER=apollo4_power` must pass. The
private runner requires two byte-identical 20,000,000-instruction logs and the
first reset at 11,897,251 ns.

## Acceptance

All six registers implement only the evidenced masks and access modes; reset,
snapshot, atomic refusal, and deterministic readback tests pass; exact 2.39
firmware advances to the separately recorded Reset/BoD/watchdog boundary twice
identically; existing profiles remain unchanged.

## Forbidden Scope

No Reset/BoD or watchdog mapping, compatibility layer, storage behavior, device
attachment, permissive reserved-bit masking, firmware bytes, or profile edits.

## Handoff

Implemented both DSP memory-power triplets with enable/status mask `0x3`,
retention mask `0x1f`, zero reset, mirrored status, strict reserved-bit and
read-only-status refusal, and snapshot round-trip/format validation. Two exact
firmware runs were byte-identical (SHA-256
`2b0592bdf0bef54da8d48d26f1137af5835395f0cf7dae1863422b1ce8788be7`): first
reset PC `0x000d2f6e` at 11,897,251 ns with zero compatibility hits, terminal
budget PC `0x000d1634`. E-SAP-0018's stacked diagnostic pins the next separate
gap at Reset/BoD `0x40000000`, immediately before watchdog `0x40024000`; another
observed-gap ticket is required.
