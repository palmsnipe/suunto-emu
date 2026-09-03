# 723 — Sapporo 2.39 NOR Page-Program Semantics

**Status:** done
**Phase:** 7
**Dependencies:** 402, 722

## Goal

Match the evidenced NOR page-program behavior required by the Sapporo
`2.39.20.22297-P` production boot, then stop at the next unsupported boundary.

## Execution Budget

One model-day for evidence pinning, a bounded flash-endpoint correction,
strict tests, and two authentic-firmware runs with an external synthetic
manufacturing fixture.

## Required Reading

Tickets 402 and 722 handoffs, E-SAP-FLASH-001, E-SAP-0025,
`docs/architecture.md`, `docs/execution-model.md`,
`docs/testing-strategy.md`, `docs/compatibility-policy.md`, the exact
`SapporoApollo4Mspi2.CompleteObservedDma` reference implementation, pristine
firmware disassembly at `0x001026c8..0x0010274e`, and the flash implementation
and focused tests named below.

## Current Baseline

With the exact 2.39 components and the explicitly synthetic 4 KiB
manufacturing sector, firmware issues MSPI2 DMA control `0x17`, instruction
`0x12`, device address `0x008d0000`, SRAM source `0x100407f8`, and count 256.
The reference model completes page program by storing `old & requested` for
each byte. The in-tree endpoint forwards the requested page directly to the
strict generic storage API, which rejects the first requested 0-to-1 bit at
`0x008d0000`. That refusal raises a HardFault at stacked PC `0x0010272a`,
causes three identical reset requests, and enters a later recovery reset and
firmware assertion instead of advancing production startup.

## Allowed Files

- `src/devices/sapporo_flash.c`
- `tests/devices/test_sapporo_flash.c`
- `tests/integration/test_firmware_sapporo_239_timer_pattern.sh`
- `tests/integration/test_firmware_sapporo_239_nor_program.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/723-sapporo-239-nor-program.md`

## Frozen Interfaces

The public storage API remains strict and continues to refuse attempts to set
cleared bits. Flash capacity, sector/page geometry, opcodes, address framing,
write-enable lifecycle, immutable source ownership, reset behavior, controller
registers, snapshots, profiles, and existing no-fixture checkpoints remain
unchanged.

## Evidence Inputs

E-SAP-FLASH-001 pins the page-program opcode and geometry and cites the
reference model's explicit per-byte `ReadFlashByte(address + i) & bytes[i]`
operation. The new 2.39 observation pins the exact transaction and failing
exception frame. The production sector is the existing explicit synthetic
fixture with SHA-256
`c08816067aed620fb8c3a074f5f0e3a8ceb398416d6f9c33d1f6c13df5619a53`;
it authorizes the bounded production-path probe but is not physical-watch
data or a default profile input.

## Implementation

After validating the complete program shape, range, page boundary, and
write-enable state, read the old page bytes, merge each requested byte with
bitwise AND, and pass only the merged candidate to `semu_storage_program`.
Keep allocation bounded by the exact 256-byte page size. Consume write-enable
once for an accepted page program. Preserve every existing malformed,
missing-enable, cross-page, and range refusal.

## Tests and Commands

`make test TEST_FILTER=sapporo_flash`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_nor_program`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=sapporo_flash` must
pass. The unit regression must prove a requested 0-to-1 bit completes without
setting that bit and that a subsequent program without write-enable refuses.
The private runner must require the exact external fixture hash, two
byte-identical logs, and an advanced checkpoint.

## Acceptance

The flash endpoint implements bitwise one-to-zero page programming without
weakening generic storage validation; the exact 2.39 transaction no longer
causes the `0x0010272a` HardFault/reset loop; existing no-fixture 2.39 idle
remains unchanged; and the production run reaches either a deterministic
advanced checkpoint or a newly identified fail-closed boundary.

## Forbidden Scope

No generic storage semantic change, implicit manufacturing data, committed
firmware/flash bytes, logical filesystem adapter, compatibility hook, page
wrap, unobserved opcode, controller relaxation, frame fabrication, or
unrelated cleanup.

## Handoff

The Sapporo flash endpoint now validates the entire page-program request,
reads at most one 256-byte page into a fixed buffer, applies bitwise AND with
the requested bytes, and passes only the merged candidate to the unchanged
strict storage API. The write-enable latch is consumed once for the accepted
operation. Focused coverage proves a request that would set cleared bits
completes without changing those bits, and that repeating it without a fresh
WREN refuses.

The exact external full-flash fixture remains opt-in and is hash-pinned to
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`;
the runner also verifies that the source image remains immutable. Two
production runs are byte-identical (log SHA-256
`21c415dee3c52ef4f77f0da42c3ea4020e0c7dc0e7092bf3bcab08d2c6661cc5`)
and no longer reach the old `0x0010272a` page-program HardFault. They first
reset at instruction 41,435,661 and 278,677,270 ns, then halt at PC
`0x00079e1e`, instruction 139,587,697, virtual time 398,188,886 ns.

Debugger inspection pins the new, separate precise fault to firmware PC
`0x000cb882` writing `0x00004000` to CTIMER address `0x40008068` (stacked PC
`0x000cb884`, LR `0x0012334f`). The MSPI2 DMA has completed before this fault.
That CTIMER value is intentionally not accepted here and requires its own
evidence-gated ticket.
