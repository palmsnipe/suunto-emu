# 714 — Sapporo 2.39 Watchdog Interrupt Enable

**Status:** done
**Phase:** 7
**Dependencies:** 713

## Goal

Implement only the observed Apollo4 Plus watchdog interrupt-enable register
required by Sapporo `2.39.20.22297-P`, then stop at the next unsupported access.

## Execution Budget

One model-day for evidence pinning, the bounded watchdog extension, strict
tests, and two authentic-firmware runs.

## Required Reading

Ticket 713 handoff, E-SAP-0018 and E-SAP-0019,
`docs/architecture.md`, `docs/execution-model.md`,
`docs/testing-strategy.md`, `docs/compatibility-policy.md`, the pristine
firmware disassembly at `0x000e9448..0x000e945a`, Apollo4 Plus PAC 1.0.0
`wdt.rs` and `wdt/wdtieren.rs` at commit
`75e44b7061b5f707907fe33688db46edeef726bb`, and the existing watchdog module
and tests named below.

## Current Baseline

After RSTGEN CFG initialization, the firmware reads watchdog offset `0x200` at
absolute address `0x40024200` and resets at 11,897,266 ns because the existing
strict watchdog model implements only CFG. Pristine control flow reads the
register, ORs bit 0, and writes `0x1`; the committed reference trace records
the same access and write value.

## Allowed Files

- `src/soc/apollo4/watchdog.c`, `src/soc/apollo4/watchdog.h`
- `tests/devices/test_apollo4_watchdog.c`
- `tests/integration/test_firmware_sapporo_239_rstgen.sh`
- `tests/integration/test_firmware_sapporo_239_watchdog_inten.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/714-sapporo-239-watchdog-inten.md`

## Frozen Interfaces

Public APIs, snapshot container format, SoC integration, profiles, and older
checkpoints remain unchanged. No watchdog status, clear, set, timer, restart,
lock, DSP watchdog, or IRQ-line behavior is introduced.

## Evidence Inputs

E-SAP-0019 pins the exact access order, firmware PC, and runtime write value.
Apollo4 Plus PAC 1.0.0 `wdt.rs` SHA-256
`aa2fcbd39026abb3ada86172b263eef0973203a57fa41c26202cdefa7265553d`
pins WDTIEREN offset `0x200`; `wdt/wdtieren.rs` SHA-256
`5413778245f4ca19f56f5a3ad5f42a39146337310ad6436c1bbcc26dd71aed31`
pins reset zero, WDTINT bit 0, DSPRESETINT bit 1, and no other named fields.

## Implementation

Extend the existing watchdog block with only 32-bit WDTIEREN read/write at
offset `0x200`, accept only bits 0..1, reset it to zero, serialize and validate
it after CFG, and retain strict refusal for every other offset/width. Advance
the exact firmware only until the next distinct unsupported boundary.

## Tests and Commands

`make test TEST_FILTER=apollo4_watchdog`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_watchdog_inten`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=apollo4_watchdog`
must pass. The private runner requires two byte-identical logs and the exact
advanced reset and bounded-stop checkpoints.

## Acceptance

WDTIEREN reset/read/write/snapshot and specified refusals pass; exact firmware
advances to a newly identified fail-closed boundary twice identically; no
other watchdog behavior or mapped fallback exists.

## Forbidden Scope

No WDTIERSTAT/WDTIERCLR/WDTIERSET, IRQ assertion, timer, restart, lock, DSP
watchdog register, compatibility hook, permissive zero page, firmware bytes,
profile change, or unrelated cleanup.

## Handoff

Implemented only 32-bit WDTIEREN at watchdog offset `0x200`: reset zero,
WDTINT bit 0, DSPRESETINT bit 1, all other values/offsets/widths refused before
mutation, and state serialized and validated after CFG. Focused tests cover
the firmware value `0x1`, both valid bits, reserved-value and width refusal,
reset, snapshot round trip, and malformed snapshot atomicity.

Two authentic 20,000,000-instruction runs are byte-identical (SHA-256
`493e50f23db1149402e2aadb20cbe6108c7e37fec27be6700e23c8821d5c35fa`).
The first reset occurs at 11,897,284 ns with PC `0x000d2f6e` and zero
compatibility hits; the terminal checkpoint is budget PC `0x000d163e`.
Debugger inspection pins the new precise fault to watchdog RSTRT at
`0x40024004`; pristine firmware PC `0x000e93aa` writes key `0xb2`. Restart-key
semantics require another evidence-gated gap ticket.
