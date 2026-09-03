# 726 — Sapporo 2.39 CTIMER7 Interrupt Enable

**Status:** done
**Phase:** 7
**Dependencies:** 310, 724

## Goal

Implement the exact CTIMER7 CMP0 interrupt-enable value exposed by the Sapporo
`2.39.20.22297-P` production path, then stop at the next unsupported boundary.

## Execution Budget

One model-day for primary-source pinning, a bounded timer correction, strict
tests, and two authentic-firmware runs with the external production fixture.

## Required Reading

Tickets 310 and 724 handoffs, E-A4-TIMER-001, E-SAP-0027,
`docs/architecture.md`, `docs/execution-model.md`,
`docs/testing-strategy.md`, `docs/compatibility-policy.md`, Apollo4 Plus PAC
1.0.0 `timer.rs` and `timer/inten.rs`, pristine firmware disassembly at
`0x000cb84c..0x000cb854`, the captured exception frame, and every exact timer
implementation and test named below.

## Current Baseline

After ticket 724 clears CTIMER7 CMP0's pending interrupt, firmware instructions
at `0x000cb84c..0x000cb852` read `0x40008060`, OR the result with
`r4=0x00004000`, and write it back. The existing exact-value allowlist refuses
that value, raising a precise HardFault with BFAR `0x40008060`, stacked PC
`0x000cb854`, and stacked LR `0x000cb84b`. Apollo4 Plus PAC 1.0.0 identifies
offset `0x60` as read/write INTEN and bit 14 as `TMR70INT`, Timer7 CMP0.

## Allowed Files

- `src/soc/apollo4/timer.c`, `src/soc/apollo4/timer_snapshot.c`
- `tests/devices/test_apollo4_timer.c`
- `tests/devices/test_apollo4_timer_snapshot.c`
- `tests/integration/test_firmware_sapporo_239_ctimer7_intclr.sh`
- `tests/integration/test_firmware_sapporo_239_ctimer7_inten.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/726-sapporo-239-ctimer7-inten.md`

## Frozen Interfaces

Keep the existing timer API, snapshot format, virtual-time model, IRQ wiring,
reset values, retained exact-value policy, and all older firmware checkpoints.
Do not widen INTEN to a generic mask or redesign the established IRQ model.

## Evidence Inputs

E-SAP-0027 pins the exact address, value, instruction sequence, exception
frame, and post-INTCLR checkpoint. Apollo4 Plus PAC 1.0.0 is pinned by crate
SHA-256
`1820cb448e123a06aa3cabff2427b71138931dfa8d67b86af2c53dd2be02b9e2`;
`timer.rs` SHA-256
`5027c11d25536ffe30caae4991460351f45f3e3a70e635c3df56618521bc1393`
places read/write INTEN at offset `0x60`, while `timer/inten.rs` SHA-256
`52bd21a8c63b7b638032953f471000c7d1ca1bb76ed57b5c8e39c6fd658d3747`
defines bit 14 as Timer7 CMP0 and reset value zero.

## Implementation

Accept and retain only whole-register INTEN value `0x00004000` in addition to
the existing allowlist. Preserve the established channel scheduling and IRQ
gates: the captured firmware transaction proves this register value and its
readback, but does not justify reinterpreting older trace-derived model state.
Extend only the existing-format snapshot validator.

## Tests and Commands

`make test TEST_FILTER=apollo4_timer`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_ctimer7_inten`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=apollo4_timer` must
pass. Focused tests must prove exact readback, neighboring-value refusal
without mutation, reset zero, and snapshot round trip. The private runner must
require the exact external fixture hash, two byte-identical logs, unchanged
source flash, and an advanced checkpoint.

## Acceptance

The exact INTEN read/OR/write sequence no longer faults, round-trips through
the existing snapshot, and advances the exact production run deterministically
to a later checkpoint or a newly identified fail-closed boundary. Ticket 724's
production gate remains stable before this transaction.

## Forbidden Scope

No generic INTEN mask, INTSET, INTCLR expansion, interrupt-mask redesign, new
timer/channel behavior, snapshot-format change, compatibility hook, implicit
manufacturing data, firmware bytes, or unrelated cleanup.

## Handoff

CTIMER INTEN now accepts and retains only the additional observed
whole-register value `0x00004000`. Offset `0x60` is named INTEN internally;
the existing state field and snapshot layout remain unchanged. Focused tests
cover reset zero, exact readback, neighboring `0x00008000` refusal without
mutation, and existing-format snapshot round trip.

Ticket 724's runner is bounded immediately after INTCLR and before INTEN: two
41,435,600-instruction runs stop at PC `0x000cb84c`, virtual time 278,677,209
ns. The new fixture-gated production runner verifies immutable source flash and
two byte-identical complete logs (SHA-256
`5e0d8dd23c863aaa00b44489d9235b4967183d44d26d49f538f094e55e6affe0`).
The first reset advances to instruction 49,456,422 at 441,085,096 ns; the final
halt is PC `0x00079e1e`, instruction 165,500,890, virtual time 885,004,292 ns.

The next exact refusal is the same INTEN register with combined value
`0x00004001`. During external IRQ21, pristine PC `0x000f7d76..0x000f7d7e`
reads INTEN, ORs Timer0 CMP0 bit zero, and writes it back. A pre-reset snapshot
pins CFSR `0x00008200`, HFSR `0x40000000`, BFAR `0x40008060`, stacked LR
`0x000f7d59`, and stacked PC `0x000f7d80`. That value remains refused for the
next evidence-gated ticket.
