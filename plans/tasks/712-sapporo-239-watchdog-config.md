# 712 — Sapporo 2.39 Watchdog Configuration Entry

**Status:** done
**Phase:** 7
**Dependencies:** 711

## Goal

Implement the one observed Apollo4 watchdog configuration register required by
Sapporo `2.39.20.22297-P`, then stop at the exact Reset/BoD routing access.

## Execution Budget

One model-day for a bounded watchdog module, SoC integration, strict tests, and
two authentic-firmware runs.

## Required Reading

Ticket 711 handoff, E-SAP-0018, the exact firmware disassembly at
`0x000e930c..0x000e9358`, upstream `AmbiqApollo4_Watchdog.cs` at commit
`1f6dee7643174cd61b6b4bde884c75ff8b69b6b8`, and the Apollo4 integration and
snapshot modules named below.

## Current Baseline

After DSP power initialization, the firmware writes watchdog CFG at
`0x40024000` and resets at 11,897,251 ns because that block is unmapped. A
fully reverted zero-window probe advanced three instructions to precise
`BFAR=0x40000000`, stacked PC `0x000e9350`, at 11,897,254 ns.

## Allowed Files

- `src/soc/apollo4/watchdog.c`, `src/soc/apollo4/watchdog.h`
- `src/soc/apollo4/apollo4.c`, `src/soc/apollo4/apollo4_internal.h`
- `src/soc/apollo4/apollo4_snapshot.c`
- `tests/devices/test_apollo4_watchdog.c`
- `tests/integration/test_firmware_sapporo_239_power.sh`
- `tests/integration/test_firmware_sapporo_239_watchdog.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/712-sapporo-239-watchdog-config.md`

## Frozen Interfaces

Public APIs, snapshot container format, existing SoC modules, profiles, and all
older checkpoints remain unchanged. No timer, interrupt, restart, lock, DSP
watchdog, or reset-routing behavior is introduced.

## Evidence Inputs

E-SAP-0018 pins the access address/order and next fault. The pristine firmware
writes CFG value `0x033c3d06`. Upstream watchdog source SHA-256
`774b72e3230bf59e9dd8ef2a576d345190a66ec583b56c7df469db3dc9035f76`
pins reset `0x00ffff00`, field layout, reserved bits 4..7 and 27..31, and clock
selectors 0..4.

## Implementation

Map a dedicated 0x400-byte watchdog block, implement only 32-bit CFG read/write,
validate reserved bits and clock selector before mutation, reset to
`0x00ffff00`, serialize CFG state, and refuse every other offset/width. Attach
it to ordered Apollo4 reset and snapshot handling. Advance the exact firmware
only to Reset/BoD `0x40000000`.

## Tests and Commands

`make test TEST_FILTER=apollo4_watchdog`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_watchdog`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=apollo4_watchdog`
must pass. The private runner requires two byte-identical logs and first reset
at 11,897,254 ns.

## Acceptance

CFG reset/read/write/snapshot and all specified refusals pass; exact firmware
advances three instructions to the recorded Reset/BoD boundary twice
identically; no other watchdog behavior or mapped fallback exists.

## Forbidden Scope

No Reset/BoD register, watchdog restart/counter/IRQ/timer/DSP registers,
compatibility hook, permissive zero page, firmware bytes, or profile change.

## Handoff

Implemented a dedicated watchdog block with only 32-bit CFG at offset zero.
Reset is `0x00ffff00`; writes validate reserved bits 4..7 and 27..31 plus clock
selectors 0..4 before mutation. Reset, read/write, snapshot round-trip, malformed
snapshot, reserved-field, invalid-selector, wrong-offset, and wrong-width tests
pass. Two exact firmware logs are byte-identical (SHA-256
`c332eee489a7209230192b03e57183fbb4f0db45d3175332daeb0ad4b705ebe5`): first
reset PC `0x000d2f6e` at 11,897,254 ns with zero compatibility hits, terminal
budget PC `0x000d1648`. The precise next gap is Reset/BoD `0x40000000`; a new
observed-gap ticket is required before implementing it.
