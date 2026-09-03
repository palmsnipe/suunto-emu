# 731 — Sapporo 2.39 LPS22 Pressure Startup

**Status:** in-progress
**Phase:** 7
**Dependencies:** 315, 410, 729

## Goal

Add the exact Sapporo 2.39-only LPS22HB endpoint and IOM2 register-selector
transfer required by native pressure startup, then stop at the next
fail-closed boundary.

## Execution Budget

One model-day for evidence pinning, strict endpoint and versioned wiring,
snapshot integration, focused tests, and two authentic-firmware runs.

## Required Reading

Tickets 315, 410, and 729; E-A4-IOM-001, E-SAP-HSPPAD143-001, and
E-SAP-COMPAT-FILES-239-001; `docs/{architecture,execution-model,
testing-strategy,compatibility-policy}.md`; `include/semu/peripheral.h`;
`src/soc/apollo4/iom.c`; `src/devices/{sapporo_devices,
sapporo_devices_internal,sapporo_devices_snapshot,sapporo_hsppad143}.*` and
their focused tests; plus every read-only firmware source named below.

## Current Baseline

Ticket 729 reaches a precise HardFault on the IOM2 command register at
`0x40052120`. The PSP exception frame has stacked PC `0x0014e8ee`, LR
`0x000a5d13`, r0 `0x10`, r1 `0x0f000112`, r2 `0x0f`, and r3 `1`. The exact
reference identifies this as a one-byte read of LPS22 register `0x0f` at I2C
address `0x5c`. The common in-tree Sapporo bus exposes only its established
2.22 HSPPAD143 endpoint at address `0x48`, and the IOM adapter does not forward
the command's high-byte register selector for address `0x5c`.

## Allowed Files

- `src/soc/apollo4/iom.c`
- `src/devices/sapporo_lps22.c`, `src/devices/sapporo_lps22.h`
- `src/devices/sapporo_devices.c`, `src/devices/sapporo_devices.h`
- `src/devices/sapporo_devices_internal.h`
- `src/devices/sapporo_devices_snapshot.c`
- `src/boards/machine.c`
- `tests/devices/test_apollo4_iom.c`
- `tests/devices/test_sapporo_lps22.c`
- `tests/devices/test_sapporo_devices.c`
- `tests/devices/test_sapporo_devices_snapshot.c`
- `tests/unit/test_machine_snapshot.c`
- `tests/integration/test_firmware_sapporo_239_logical_files.sh`
- `tests/integration/test_firmware_sapporo_239_lps22.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/731-sapporo-239-lps22.md`

## Frozen Interfaces

CPU, DMA, generic serial transaction, snapshot version/section IDs, profile
syntax, compatibility layer, existing 2.22/2.33 wiring and snapshot bytes,
HSPPAD143 behavior, and every other IOM address remain unchanged. The new
public board-device selector accepts only a known profile ID and must be called
before SoC attachment.

## Evidence Inputs

E-SAP-LPS22-239-001 must cite read-only
`$FIRMWARE_ROOT/docs/research/sapporo-2.39-lps22-pressure.md` SHA-256
`356bf18b1c47a7ca92a3f155a8cbaede850dabaf45fe4db461f8cffde7b5579f`,
`emulator/renode/pressure/SapporoLps22.cs`
`d65ec2bc2978ee4a75c3b35c55a3853891f05257dfd05ea1bb8fc33675eac936`,
its focused test
`cae4f6f4d8692322e4116b607ab046f0e2cdf6d38ad341997002534e66a76182`,
and versioned platform fragment
`8e9f2eabb126be1dd3da015fa6e8d50cfaa7afbf9beee67f6895a8b87bc03de5`.
The exact reference log pins fourteen IOM2 transactions: reads of registers
`0x0f`, `0x11`, `0x10`, and `0x33`; writes `11 80`, `11 04`, `10 02`,
`10 0e`, and `10 1e`; LPS22HB identity `0xb1`; and immediate self-clear of
CTRL_REG2 bits `0x80` and `0x04`.

## Implementation

Create a strict address-parameterized endpoint that accepts only the observed
one-byte register reads and two-byte writes, returns `0xb1` at WHO_AM_I,
retains CTRL_REG1/CTRL_REG2 writes, and self-clears only the two observed
CTRL_REG2 command bits. Unknown address, direction, shape, register, or span
refuses before mutation. Add address `0x5c` to IOM command-selector extraction
without changing other address handling. Instantiate and attach LPS22 only
when the board-device factory is explicitly selected for profile
`sapporo-2.39.20`; the common profiles must continue to refuse address `0x5c`.

Serialize the conditional endpoint using explicit existing snapshot helpers.
Current 2.39 snapshots without appended LPS22 state restore its reset state;
2.22/2.33 snapshot bytes remain unchanged. Validate the complete restored
state before replacement.

## Tests and Commands

`make test TEST_FILTER=sapporo_lps22`, `make test TEST_FILTER=apollo4_iom`,
`make test TEST_FILTER=sapporo_devices`, `make test
TEST_FILTER=machine_snapshot`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_lps22`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=sapporo_lps22`
must pass.

Unit coverage includes the exact fourteen-command transcript, identity,
self-clearing bits, reset, repeatability, 2.39-only bus selection, wrong
address/register/direction/shape refusal without mutation, IOM selector
forwarding, snapshot round trip, older-snapshot restore, and malformed-state
refusal. The private runner requires the exact external flash hash, two
byte-identical runs, unchanged source flash, absence of the ticket-729 reset,
and a later explicit checkpoint.

## Acceptance

The exact 2.39 firmware completes native LPS22 discovery and initialization
twice identically; address `0x5c` remains absent from other profiles; no new
compatibility hit is introduced; snapshot continuation preserves the device;
and execution reaches a later checkpoint or newly identified fail-closed
boundary.

## Forbidden Scope

No LPS22HH selection, HSPPAD replacement, generic I2C acknowledgement,
all-register array, fabricated nonzero pressure/temperature, conversion
timing, FIFO engine, IRQ, alternate address, other IOM address/command change,
compatibility hook, firmware byte, snapshot-version, renderer, or unrelated
change.

## Handoff

Implemented the strict Sapporo 2.39-only LPS22HB endpoint and exact IOM2
selector forwarding. The endpoint reproduces the fourteen-command native
startup transcript, returns identity `0xb1`, retains CTRL_REG1, immediately
self-clears evidenced CTRL_REG2 command bits, and refuses unsupported
addresses, registers, directions, and shapes before mutation. Profile
selection wires address `0x5c` only for `sapporo-2.39.20`; 2.22/2.33 continue
to refuse it. Conditional snapshot state round-trips atomically, accepts the
prior shorter 2.39 form as reset state, and leaves common-profile bytes
unchanged.

Evidence: E-SAP-LPS22-239-001 and its five hash-pinned read-only sources. The
exact 32-MiB source flash remained SHA-256
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
Two native runs stopped without reset at PC `0x000a7b2e`, instruction
79,000,000, virtual time 520,829,069 ns; log SHA-256
`fa74015d06b9aa988787724e666f4e37c1223c14fc36f996cd773b2a93d61592`
and snapshot SHA-256
`75f0f534bfc9aae60adabd642f0d4fa982146ed9a743abb7e584aa0e1664e290`.
Snapshot resume advanced to PC `0x000a7b30` at instruction 79,000,001 and
520,829,070 ns. A longer exploratory run reached a distinct firmware reset
request at instruction 122,452,650 and 1,230,991,337 ns; its owner remains
unsupported and is the next evidence gate.

Verification passed: `make test TEST_FILTER=sapporo_lps22`, `make test
TEST_FILTER=apollo4_iom`, `make test TEST_FILTER=sapporo_devices`, `make test
TEST_FILTER=machine_snapshot`, the exact filtered authentic-firmware command
with `SEMU_SAPPORO_239_FULL_FLASH` set to the hash-pinned fixture, the updated
ticket-729 logical-files authentic runner, `make check-lines`, `make check`,
and `make sanitize TEST_FILTER=sapporo_lps22`. The new IOM regression was
first observed failing at the command write before the implementation.

No compatibility hit, LPS22HH identity, alternate address, physical sample,
conversion timing, FIFO, IRQ, or unobserved register span was added. The
integrator owns any future roadmap-status transition.
