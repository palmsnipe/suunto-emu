# 713 — Sapporo 2.39 Reset Generator Configuration

**Status:** done
**Phase:** 7
**Dependencies:** 712

## Goal

Implement only the observed Apollo4 Plus reset-generator CFG register required
by Sapporo `2.39.20.22297-P`, then stop at the next watchdog register access.

## Execution Budget

One model-day for evidence pinning, a bounded RSTGEN module, SoC integration,
strict tests, and two authentic-firmware runs.

## Required Reading

Ticket 712 handoff, E-SAP-0018, `docs/architecture.md`,
`docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`, the exact firmware disassembly at
`0x000e9348..0x000e935a`, Apollo4 Plus PAC 1.0.0 RSTGEN sources at commit
`75e44b7061b5f707907fe33688db46edeef726bb`, and the Apollo4 integration and
snapshot modules named below.

## Current Baseline

After watchdog CFG initialization, the firmware reads RSTGEN CFG at
`0x40000000` and resets at 11,897,254 ns because that block is unmapped. The
pristine code clears bit 1 and rewrites it from the watchdog-reset flag. A
committed 2.39 reference trace records a zero read and write value `0x2`.

## Allowed Files

- `src/soc/apollo4/rstgen.c`, `src/soc/apollo4/rstgen.h`
- `src/soc/apollo4/apollo4.c`, `src/soc/apollo4/apollo4_internal.h`
- `src/soc/apollo4/apollo4_snapshot.c`
- `tests/devices/test_apollo4_rstgen.c`
- `tests/integration/test_firmware_sapporo_239_watchdog.sh`
- `tests/integration/test_firmware_sapporo_239_rstgen.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/713-sapporo-239-rstgen-config.md`

## Frozen Interfaces

Public APIs, snapshot container format, existing SoC modules, profiles, and all
older checkpoints remain unchanged. No software reset, brownout event, status,
interrupt, or watchdog-register behavior is introduced.

## Evidence Inputs

E-SAP-0018 pins the access address, instruction order, runtime write value, and
next register. The Apollo4 Plus PAC 1.0.0 RSTGEN `cfg.rs` pins CFG offset zero,
reset zero, BODHREN bit 0, WDREN bit 1, and no other named fields. Its crate and
source hashes are recorded in E-SAP-0018. The committed 2.39 reference trace
records read zero and write `0x2` at the exact firmware PCs.

## Implementation

Map a dedicated 0x400-byte RSTGEN block, implement only 32-bit CFG read/write,
validate that only bits 0..1 are set before mutation, reset to zero, serialize
CFG state, and refuse every other offset/width. Attach it to ordered Apollo4
reset and snapshot handling. Advance exact firmware only to watchdog INTEN at
`0x40024200`.

## Tests and Commands

`make test TEST_FILTER=apollo4_rstgen`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_rstgen`, `make check-lines`,
`make check`, and `make sanitize TEST_FILTER=apollo4_rstgen` must pass. The
private runner requires two byte-identical logs and the exact advanced reset.

## Acceptance

CFG reset/read/write/snapshot and specified refusals pass; exact firmware
advances to the recorded watchdog INTEN boundary twice identically; no other
RSTGEN behavior or mapped fallback exists.

## Forbidden Scope

No RSTGEN SWPOI/SWPOR/SIMOBODM/interrupt/status behavior, watchdog INTEN or
timer behavior, compatibility hook, permissive zero page, firmware bytes, or
profile change.

## Handoff

Implemented only 32-bit RSTGEN CFG at `0x40000000`: reset zero, BODHREN bit 0,
WDREN bit 1, all other values/offsets/widths refused before mutation, and CFG
included in ordered reset and snapshot state. Focused tests cover reset,
firmware value `0x2`, reserved-bit/offset/width refusal, snapshot round trip,
and malformed snapshot refusal.

Two authentic 20,000,000-instruction runs are byte-identical (SHA-256
`540b62b500147fffa74f44f5ba4d0f1f02c9fa1c7713512a8834c78700fee7b5`).
The first reset occurs at 11,897,266 ns with PC `0x000d2f6e` and zero
compatibility hits; the terminal checkpoint is budget PC `0x000d163e`. Static
control flow and the committed reference trace identify the new fail-closed
boundary as the 32-bit watchdog INTEN read at `0x40024200` from firmware PC
`0x000e9454`, followed by a write of `0x1`. That register requires a separate
evidence-gated gap ticket.
