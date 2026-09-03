# 719 — Sapporo 2.39 CTIMER OUTCFG26

**Status:** done
**Phase:** 7
**Dependencies:** 717

## Goal

Implement only the observed Apollo4 Plus CTIMER OUTCFG26 value required by
Sapporo `2.39.20.22297-P`, then stop at the next unsupported access.

## Execution Budget

One model-day for evidence pinning, the bounded CTIMER extension, strict tests,
and two authentic-firmware runs.

## Required Reading

Ticket 717 handoff, E-A4-TIMER-001, E-SAP-0022,
`docs/architecture.md`, `docs/execution-model.md`,
`docs/testing-strategy.md`, `docs/compatibility-policy.md`, pristine firmware
disassembly at `0x000ea540..0x000ea56c`, and Apollo4 Plus PAC 1.0.0
`timer.rs` and `timer/outcfg26.rs` at commit
`75e44b7061b5f707907fe33688db46edeef726bb`.

## Current Baseline

After deterministic chip-identity initialization, firmware PC `0x000ea56a`
writes `0x0000003f` to CTIMER address `0x400080e8` and resets because the
strict timer model accepts only the older observed values `0` and `0x12` at
that offset. Debugger inspection pins the precise fault address and value.

## Allowed Files

- `src/soc/apollo4/timer.c`, `src/soc/apollo4/timer_snapshot.c`
- `tests/devices/test_apollo4_timer.c`
- `tests/devices/test_apollo4_timer_snapshot.c`
- `tests/integration/test_firmware_sapporo_239_chip_identity.sh`
- `tests/integration/test_firmware_sapporo_239_timer_outcfg26.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/719-sapporo-239-ctimer-outcfg26.md`

## Frozen Interfaces

Public APIs, snapshot format, SoC integration, profiles, existing CTIMER values
and reset state, and older checkpoints remain unchanged. No other OUTCFG
register, pad routing, waveform, interrupt, or electrical output behavior is
introduced.

## Evidence Inputs

E-A4-TIMER-001 pins the older `0` and `0x12` values from the exact-hash 2.22
trace. E-SAP-0022 pins the current address, instruction, value, and checkpoint.
Apollo4 Plus PAC source hashes are `timer.rs`
`5027c11d25536ffe30caae4991460351f45f3e3a70e635c3df56618521bc1393`
and `timer/outcfg26.rs`
`04d85c84d589d099aab7d6e8e644d9a09658713ad9bf56325df39857bd0d64b1`;
they identify offset `0xe8` as OUTCFG26 and bits 0:5 as OUTCFG104, where value
`0x3f` means output disabled. The model preserves its existing trace-derived
reset zero rather than broadening this ticket into a reset-policy change.

## Implementation

Extend only the exact 32-bit OUTCFG26 write and snapshot-validation allowlists
with whole-register value `0x0000003f`. Preserve readback and snapshot format.
All other unobserved values, widths, offsets, and routing effects continue to
refuse or remain unsupported. Advance exact firmware only to the next distinct
boundary.

## Tests and Commands

`make test TEST_FILTER=apollo4_timer`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_timer_outcfg26`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=apollo4_timer` must
pass. The private runner requires two byte-identical logs and exact advanced
checkpoints.

## Acceptance

OUTCFG26 accepts and reads back exact value `0x3f`; an unobserved value refuses
without mutation; exact firmware advances to a newly identified fail-closed
boundary twice identically; no generic bitmask or output behavior exists.

## Forbidden Scope

No other OUTCFG register/value, reset-value change, pad routing, timer waveform,
IRQ change, compatibility hook, permissive fallback, firmware bytes, profile
change, snapshot-format change, or unrelated cleanup.

## Handoff

Implemented only the exact OUTCFG26 whole-register value `0x0000003f` at
CTIMER offset `0xe8`, preserving the older `0` and `0x12` values, reset state,
readback, and snapshot format. Invalid value `1` refuses without mutation, and
the snapshot validator and round-trip test now accept the evidenced value.

Two authentic 30,000,000-instruction runs are byte-identical (SHA-256
`8e079f452fdc7f6485d6688746a1db93f0688fe517b01f1ca295ad6db5e8cb23`).
The first reset advances to instruction 24,771,518 and virtual time 30,111,413
ns with PC `0x000d2f6e` and zero compatibility hits; terminal budget PC is
`0x000d1634` at 35,339,895 ns. Debugger inspection pins the new precise fault
to a 32-bit read of USB CLKCTRL at `0x400b2000`; pristine PC `0x000f8c02`
loads the register before PC `0x000f8c08` writes `0x02000000`. That USB
register requires a separate evidence-gated ticket.
