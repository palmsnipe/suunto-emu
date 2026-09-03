# 722 — Sapporo 2.39 CTIMER Pattern Transition

**Status:** done
**Phase:** 7
**Dependencies:** 721

## Goal

Implement only the observed CTIMER pattern-register transition required by
Sapporo `2.39.20.22297-P`, then stop at the next unsupported access.

## Execution Budget

One model-day for evidence pinning, the bounded CTIMER extension, strict tests,
and two authentic-firmware runs.

## Required Reading

Ticket 721 handoff, E-A4-TIMER-001, E-SAP-0024,
`docs/architecture.md`, `docs/execution-model.md`,
`docs/testing-strategy.md`, `docs/compatibility-policy.md`, pristine firmware
disassembly at `0x000f7d4c..0x000f7d76`, and the timer implementation,
snapshot validator, and focused pattern tests named below.

## Current Baseline

After USB clock initialization and later virtual-time progress, firmware PC
`0x000f7d60` writes `0x00012300` to CTIMER offset `0x104` and resets because
that exact value is absent from the trace-derived pattern allowlist. The
routine read `0x00012301`, clears bit 0 temporarily, and restores the original
value at PC `0x000f7d74` after programming the associated state.

## Allowed Files

- `src/soc/apollo4/timer.c`, `src/soc/apollo4/timer_snapshot.c`
- `tests/devices/test_apollo4_timer_patterns.c`
- `tests/devices/test_apollo4_timer_snapshot.c`
- `tests/integration/test_firmware_sapporo_239_usb_clkctrl.sh`
- `tests/integration/test_firmware_sapporo_239_timer_pattern.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/722-sapporo-239-ctimer-pattern.md`

## Frozen Interfaces

Public APIs, snapshot format, SoC integration, profiles, existing CTIMER values
and reset state, and older checkpoints remain unchanged. No interpretation of
offset `0x104`, timer routing, waveform, IRQ, or electrical output is added.

## Evidence Inputs

E-A4-TIMER-001 and ticket 310 pin offset `0x104` as the reference model's
trace-derived pattern-address register and its existing exact-value policy.
E-SAP-0024 pins the current address, instruction, value, prior value, and
checkpoint. Pristine disassembly proves the temporary bit-0 clear and later
restore are part of one bounded routine rather than a generic writable mask.

## Implementation

Extend only the exact 32-bit pattern write and existing-format snapshot
validation allowlists with whole-register value `0x00012300`. Preserve
readback, reset, and all existing values. Every other value, width, offset, and
effect remains refused or unsupported. Advance firmware only to the next
distinct boundary.

## Tests and Commands

`make test TEST_FILTER=apollo4_timer`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_timer_pattern`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=apollo4_timer` must
pass. The private runner requires two byte-identical logs and exact advanced
checkpoints.

## Acceptance

Offset `0x104` accepts and reads back exact value `0x12300`; a neighboring
unobserved value refuses without mutation; snapshots round-trip the evidenced
value; exact firmware advances beyond the prior fault to a byte-identical
bounded idle checkpoint or to a newly identified fail-closed boundary.

## Forbidden Scope

No generic bit mask, other pattern value/offset, routing/waveform/IRQ behavior,
snapshot-format change, compatibility hook, permissive fallback, firmware
bytes, profile change, or unrelated cleanup.

## Handoff

Implemented only the exact CTIMER offset `0x104` whole-register value
`0x00012300`, preserving every older value, reset behavior, readback, and
snapshot format. The existing refusal test confirms neighboring value
`0x00010302` does not mutate the new state, and snapshots round-trip the value.

Two authentic runs with a 200,000,000-instruction and 2,000,000,000 ns budget
are byte-identical (SHA-256
`96e428de7caf01f866ed3a91193a7e45ff2c37d700a63a9deb9764d8f0506890`)
and contain no reset. Both reach the firmware's WFI/ISB idle path and stop only
on the configured time budget at PC `0x000e955a`, instruction 84,856,118, and
virtual time 6,372,873,793 ns. No next unsupported hardware access was observed
within that bounded run.
