# 716 — Sapporo 2.39 Watchdog Restart Key

**Status:** done
**Phase:** 7
**Dependencies:** 714

## Goal

Implement only the observed Apollo4 Plus watchdog restart-key register required
by Sapporo `2.39.20.22297-P`, then stop at the next unsupported access.

## Execution Budget

One model-day for evidence pinning, the bounded watchdog extension, strict
tests, and two authentic-firmware runs.

## Required Reading

Ticket 714 handoff, E-SAP-0018 through E-SAP-0020,
`docs/architecture.md`, `docs/execution-model.md`,
`docs/testing-strategy.md`, `docs/compatibility-policy.md`, pristine firmware
disassembly at `0x000d1ef0..0x000e93b4`, Apollo4 Plus PAC 1.0.0 `wdt.rs` and
`wdt/rstrt.rs` at commit `75e44b7061b5f707907fe33688db46edeef726bb`,
upstream `AmbiqApollo4_Watchdog.cs` at commit
`1f6dee7643174cd61b6b4bde884c75ff8b69b6b8`, and the watchdog implementation
and tests named below.

## Current Baseline

After WDTIEREN initialization, firmware PC `0x000e93aa` writes key `0xb2` to
watchdog RSTRT at `0x40024004` and resets at 11,897,284 ns because the strict
watchdog model does not implement that offset. Debugger inspection pins the
precise fault address, and pristine disassembly pins the instruction and value.

## Allowed Files

- `src/soc/apollo4/watchdog.c`, `src/soc/apollo4/watchdog.h`
- `tests/devices/test_apollo4_watchdog.c`
- `tests/integration/test_firmware_sapporo_239_watchdog_inten.sh`
- `tests/integration/test_firmware_sapporo_239_watchdog_restart.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/716-sapporo-239-watchdog-restart.md`

## Frozen Interfaces

Public APIs, snapshot format/state, SoC integration, profiles, and older
checkpoints remain unchanged. No watchdog counting, expiry, reset generation,
status, clear, set, lock, DSP watchdog, or IRQ-line behavior is introduced.

## Evidence Inputs

E-SAP-0020 pins the exact address, firmware PC, key, and current checkpoint.
Apollo4 Plus PAC `wdt/rstrt.rs` SHA-256
`39763293227a12d6d3d8d849fb9a158f496cf221638b2fbb77b5e29b066fb6db`
pins offset `0x04`, write-only key `0xb2`, and reads/reset to zero. The upstream
watchdog source SHA-256
`774b72e3230bf59e9dd8ef2a576d345190a66ec583b56c7df469db3dc9035f76`
independently accepts `0xb2` as its only reload action.

## Implementation

Extend the watchdog block with only 32-bit RSTRT access at offset `0x04`.
Reads return zero; writes accept exactly key `0xb2` without persistent state;
all other values and widths refuse before mutation. Advance exact firmware
only until the next distinct unsupported boundary.

## Tests and Commands

`make test TEST_FILTER=apollo4_watchdog`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_watchdog_restart`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=apollo4_watchdog`
must pass. The private runner requires two byte-identical logs and the exact
advanced reset and bounded-stop checkpoints.

## Acceptance

RSTRT zero-read, exact-key write, invalid-value/width/offset refusals, and
non-mutation pass; exact firmware advances to a newly identified fail-closed
boundary twice identically; no timer state or other watchdog behavior exists.

## Forbidden Scope

No counter/timer/expiry/reset action, lock, interrupt status/clear/set, DSP
watchdog register, compatibility hook, permissive fallback, firmware bytes,
profile change, snapshot-format change, or unrelated cleanup.

## Handoff

Implemented only 32-bit watchdog RSTRT at offset `0x04`: reads return zero,
writes accept exactly `0xb2`, all other values/widths/offsets refuse, and the
command has no persistent or timer state. Focused tests cover zero readback,
the exact key, invalid key and width, neighboring-offset refusal, and
non-mutation of CFG/INTEN.

Two authentic 20,000,000-instruction runs are byte-identical (SHA-256
`0a092da13d76a589b189bc43a22461bd5e280d68e791927d81dd2193f96958f6`).
The first reset moves to instruction 19,945,598 and virtual time 25,285,493 ns
with PC `0x000d2f6e` and zero compatibility hits; the terminal checkpoint is
budget PC `0x001b4b52` at 25,339,895 ns. Debugger inspection pins the new
precise fault to MCUCTRL CHIPID0 at `0x40020004`; pristine firmware PC
`0x0008a0ba` reads CHIPID0 and then CHIPID1 at `0x40020008`. Those identity
registers require a separate deterministic-policy gap ticket.
