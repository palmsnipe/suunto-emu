# 721 — Sapporo 2.39 USB CLKCTRL

**Status:** done
**Phase:** 7
**Dependencies:** 719

## Goal

Implement only the observed Apollo4 Plus USB CLKCTRL access required by
Sapporo `2.39.20.22297-P`, then stop at the next unsupported access.

## Execution Budget

One model-day for evidence pinning, the bounded USB-register extension, strict
tests, and two authentic-firmware runs.

## Required Reading

Ticket 719 handoff, E-SAP-0023, `docs/architecture.md`,
`docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`, pristine firmware disassembly at
`0x000f8bf8..0x000f8c0a`, the pinned native full-UI trace around PCs
`0x000f8c02..0x000f8c08`, Apollo4 Plus PAC 1.0.0 `usb.rs` and
`usb/clkctrl.rs` at commit `75e44b7061b5f707907fe33688db46edeef726bb`,
and the existing bounded USB-window implementation and test named below.

## Current Baseline

After CTIMER OUTCFG26 initialization, firmware PC `0x000f8c02` reads USB
CLKCTRL at `0x400b2000` and resets because offset `0x2000` is absent from the
strict allowlist for the mapped `0x400b0000` window. The following instruction
sequence inserts PHYREFCLKSEL value 2 and PC `0x000f8c08` writes
`0x02000000` to the same register.

## Allowed Files

- `src/soc/apollo4/adc.c`
- `tests/devices/test_apollo4.c`
- `tests/integration/test_firmware_sapporo_239_timer_outcfg26.sh`
- `tests/integration/test_firmware_sapporo_239_usb_clkctrl.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/721-sapporo-239-usb-clkctrl.md`

## Frozen Interfaces

Public APIs, snapshot format/state, SoC integration, profiles, existing USB
window accesses, and older checkpoints remain unchanged. No USB transport,
endpoint, FIFO, PHY, battery-detection, interrupt, DMA, or host-device behavior
is introduced.

## Evidence Inputs

E-SAP-0023 pins the exact current address, instructions, value, and checkpoint.
The native full-UI trace SHA-256
`fd5b6208937d1cfe4479405ea5af5635659ecf3163c8f033da6013d3b858ef6e`
independently records a zero-returning unimplemented read followed by ignored
write `0x02000000` at those PCs. Apollo4 Plus PAC source hashes are `usb.rs`
`9e201740137a7be0e1c8489562e17a51adca5e60783676263c67ac8818f675a9`
and `usb/clkctrl.rs`
`9657d3fe91761d71c43961ab28ea3c9575fdb6e1cad238940597a2c9448a059f`;
they pin CLKCTRL offset `0x2000`, reset zero, and PHYREFCLKSEL bits 24:25,
where value 2 selects the 24 MHz HFRC reference.

## Implementation

Add only the exact 32-bit CLKCTRL zero read and `0x02000000` no-output write to
the existing trace-derived USB-window allowlists. The write remains stateless,
matching the pinned reference run and avoiding a snapshot-format change. All
other values, widths, offsets, and USB effects continue to refuse or remain
unsupported. Advance exact firmware only to the next distinct boundary.

## Tests and Commands

`make test TEST_FILTER=apollo4`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_usb_clkctrl`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=apollo4` must pass.
The private runner requires two byte-identical logs and exact advanced
checkpoints.

## Acceptance

CLKCTRL reads zero, accepts exact value `0x02000000`, and remains zero after the
no-output write; an unobserved value and wrong width refuse; exact firmware
advances to a newly identified fail-closed boundary twice identically.

## Forbidden Scope

No persistent USB state or snapshot change, other CLKCTRL value, broad USB
register fallback, endpoint/FIFO/PHY/BCDET/IRQ/DMA/host behavior, compatibility
hook, firmware bytes, profile change, or unrelated cleanup.

## Handoff

Implemented only the exact 32-bit USB CLKCTRL zero read and `0x02000000`
no-output write at offset `0x2000` of the existing mapped USB window. Reads
remain zero after the write, matching the pinned native reference behavior;
value `0x01000000` and byte reads retain strict refusal. No state or snapshot
format was added.

Two authentic 100,000,000-instruction runs are byte-identical (SHA-256
`06e69fa86a9034a491bc7381a4b51b6dea538e9d326d29fb81bba10945d79374`).
The first reset advances to instruction 77,220,237 and virtual time 368,259,842
ns with PC `0x000d2f6e` and zero compatibility hits; terminal budget PC is
`0x000a7ab6` at 396,379,500 ns. Debugger inspection pins the new precise fault
to CTIMER observed-pattern offset `0x104`; pristine PC `0x000f7d60` writes
`0x00012300`, clearing bit 0 of the preceding `0x00012301` value before later
restoration. That CTIMER value requires a separate evidence-gated ticket.
