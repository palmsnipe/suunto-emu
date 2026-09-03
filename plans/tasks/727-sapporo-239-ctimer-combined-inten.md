# 727 — Sapporo 2.39 Combined CTIMER Interrupt Enable

**Status:** done
**Phase:** 7
**Dependencies:** 310, 726

## Goal

Implement the exact combined Timer0/Timer7 CMP0 INTEN value exposed by the
Sapporo `2.39.20.22297-P` IRQ21 path, then stop at the next unsupported
boundary.

## Execution Budget

One model-day for an exact-value timer correction, strict tests, and two
authentic-firmware runs with the external production fixture.

## Required Reading

Tickets 310 and 726 handoffs, E-A4-TIMER-001, E-SAP-0028,
`docs/architecture.md`, `docs/execution-model.md`,
`docs/testing-strategy.md`, `docs/compatibility-policy.md`, Apollo4 Plus PAC
1.0.0 `timer.rs` and `timer/inten.rs`, pristine firmware disassembly at
`0x000f7d4c..0x000f7d84`, the captured exception frame, and every exact timer
implementation and test named below.

## Current Baseline

During external IRQ21, firmware instructions at `0x000f7d76..0x000f7d7e`
read INTEN value `0x00004000`, OR bit zero, and write `0x00004001` to
`0x40008060`. The exact-value allowlist refuses that combined value, raising a
precise HardFault with stacked PC `0x000f7d80`. The PAC identifies bits zero
and 14 as Timer0 CMP0 and Timer7 CMP0 respectively.

## Allowed Files

- `src/soc/apollo4/timer.c`, `src/soc/apollo4/timer_snapshot.c`
- `tests/devices/test_apollo4_timer.c`
- `tests/devices/test_apollo4_timer_snapshot.c`
- `tests/integration/test_firmware_sapporo_239_ctimer7_inten.sh`
- `tests/integration/test_firmware_sapporo_239_ctimer_combined_inten.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`
- `plans/tasks/727-sapporo-239-ctimer-combined-inten.md`

## Frozen Interfaces

Keep the existing timer API, snapshot format, virtual-time model, IRQ wiring,
reset values, retained exact-value policy, and all older firmware checkpoints.

## Evidence Inputs

E-SAP-0028 pins the exact address, value, IRQ context, instruction sequence,
exception frame, and post-726 checkpoint. Apollo4 Plus PAC 1.0.0 is pinned by
crate SHA-256
`1820cb448e123a06aa3cabff2427b71138931dfa8d67b86af2c53dd2be02b9e2`;
`timer.rs` SHA-256
`5027c11d25536ffe30caae4991460351f45f3e3a70e635c3df56618521bc1393`
places INTEN at offset `0x60`, while `timer/inten.rs` SHA-256
`52bd21a8c63b7b638032953f471000c7d1ca1bb76ed57b5c8e39c6fd658d3747`
defines bits zero and 14 as the observed CMP0 enables.

## Implementation

Accept and retain only whole-register INTEN value `0x00004001` in addition to
the existing allowlist. Preserve the established channel scheduling and IRQ
gates, and extend only the existing-format snapshot validator.

## Tests and Commands

`make test TEST_FILTER=apollo4_timer`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_ctimer_combined_inten`,
`make check-lines`, `make check`, and `make sanitize
TEST_FILTER=apollo4_timer` must pass. Focused tests must prove exact readback,
neighboring-value refusal without mutation, and snapshot round trip. The
private runner must require the exact external fixture hash, two byte-identical
logs, unchanged source flash, and an advanced checkpoint.

## Acceptance

The exact IRQ21 INTEN read/modify/write no longer faults, round-trips through
the existing snapshot, and advances the exact production run deterministically
to a later checkpoint or a newly identified fail-closed boundary. Ticket 726's
production gate remains stable before this transaction.

## Forbidden Scope

No generic INTEN mask, INTSET, INTCLR expansion, interrupt-mask redesign, new
timer/channel behavior, snapshot-format change, compatibility hook, implicit
manufacturing data, firmware bytes, or unrelated cleanup.

## Handoff

CTIMER INTEN now accepts and retains only the additional observed combined
whole-register value `0x00004001`. The current IRQ model and snapshot format
remain unchanged. Focused tests cover exact readback, `0x00004002` refusal
without mutation, and existing-format snapshot round trip.

Ticket 726's runner is bounded before the combined write at instruction
49,456,350, PC `0x000f7afc`, virtual time 441,085,024 ns. The new fixture-gated
runner verifies immutable source flash and two byte-identical complete one-line
logs (SHA-256
`db1ba19b3e47306cf304b52f32b86db8dd4aa97f6afc6c64d962f7fdf24e43d8`).
The run contains no reset and stops at `stop=halt`, PC `0x00079e1e`,
instruction 72,774,982, virtual time 521,257,564 ns.

The halt follows firmware `BKPT #0` at `0x00079e1c`. A snapshot immediately
before it records LR `0x001248b9`; pristine code there passes line 67 and the
string `StartupClient.cpp` to the fatal path. This is not an unsupported
instruction or device transaction. The exact triggering application state
must be reverse engineered separately before implementing another behavior.
