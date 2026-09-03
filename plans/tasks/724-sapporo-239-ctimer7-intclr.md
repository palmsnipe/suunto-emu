# 724 — Sapporo 2.39 CTIMER7 Interrupt Clear

**Status:** done
**Phase:** 7
**Dependencies:** 310, 723

## Goal

Implement the exact CTIMER7 CMP0 interrupt-clear write exposed by the Sapporo
`2.39.20.22297-P` production path, then stop at the next unsupported boundary.

## Execution Budget

One model-day for primary-source pinning, a bounded timer correction, strict
tests, and two authentic-firmware runs with the external production fixture.

## Required Reading

Tickets 310 and 723 handoffs, E-A4-TIMER-001, E-SAP-0026,
`docs/architecture.md`, `docs/execution-model.md`,
`docs/testing-strategy.md`, `docs/compatibility-policy.md`, Apollo4 Plus PAC
1.0.0 `timer.rs` and `timer/intclr.rs`, pristine firmware disassembly at
`0x000cb870..0x000cb884` and `0x0012333e..0x00123354`, and every exact timer
implementation and test named below.

## Current Baseline

After ticket 723 completes the MSPI2 page program, firmware instruction
`str r0,[r1]` at PC `0x000cb882` writes `r0=0x00004000` to
`r1=0x40008068`. The current CTIMER allowlist refuses that value, raising a
precise HardFault with stacked PC `0x000cb884` and LR `0x0012334f`. Apollo4
Plus PAC 1.0.0 identifies offset `0x68` as INTCLR, write-one-to-clear, and bit
14 as `TMR70INT`, the Timer7 CMP0 interrupt.

## Allowed Files

- `src/soc/apollo4/timer.c`, `src/soc/apollo4/timer_snapshot.c`
- `tests/devices/test_apollo4_timer.c`
- `tests/devices/test_apollo4_timer_snapshot.c`
- `tests/integration/test_firmware_sapporo_239_nor_program.sh`
- `tests/integration/test_firmware_sapporo_239_ctimer7_intclr.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/724-sapporo-239-ctimer7-intclr.md`

## Frozen Interfaces

Keep the existing timer API, snapshot format, virtual-time model, IRQ wiring,
reset values, retained exact-value policy, and all older firmware checkpoints.
Do not widen any register to a generic mask.

## Evidence Inputs

E-SAP-0026 pins the exact address, value, instruction, exception frame, and
post-flash checkpoint. Apollo4 Plus PAC 1.0.0 is pinned by crate SHA-256
`1820cb448e123a06aa3cabff2427b71138931dfa8d67b86af2c53dd2be02b9e2`;
`timer.rs` SHA-256
`5027c11d25536ffe30caae4991460351f45f3e3a70e635c3df56618521bc1393`
places INTCLR at offset `0x68`, while `timer/intclr.rs` SHA-256
`b82119d005e0ba5f231ec456df38d7af458443fb51945335c290114acf483519`
defines bit 14 as Timer7 CMP0 and documents write-one-to-clear behavior.

## Implementation

Accept only whole-register value `0x00004000` in addition to the existing
INTCLR allowlist. Translate that physical interrupt bit to the timer model's
channel-7 pending/IRQ state and clear it. Retain the observed write for current
readback and snapshot compatibility, extending only the existing-format
snapshot validator.

## Tests and Commands

`make test TEST_FILTER=apollo4_timer`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_ctimer7_intclr`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=apollo4_timer` must
pass. Focused tests must prove Timer7 pending/IRQ clear, exact readback,
neighboring-value refusal without mutation, and snapshot round trip. The
private runner must require the exact external fixture hash, two byte-identical
logs, unchanged source flash, and an advanced checkpoint.

## Acceptance

The exact INTCLR write no longer faults, clears modeled Timer7 pending/IRQ
state, round-trips through the existing snapshot, and advances the exact
production run deterministically to a later checkpoint or a newly identified
fail-closed boundary. Ticket 723's production and no-fixture gates remain
unchanged.

## Forbidden Scope

No generic INTCLR mask, INTSET, new interrupt mode, interrupt-mask redesign,
new timer/channel behavior, snapshot-format change, compatibility hook,
implicit manufacturing data, firmware bytes, or unrelated cleanup.

## Handoff

CTIMER INTCLR now accepts only the additional observed whole-register value
`0x00004000`, translates physical TMR70INT bit 14 to the model's channel-7
pending bit, and deasserts the channel-7 IRQ. The existing readback and
snapshot format are preserved. Focused tests cover pending/IRQ clear, exact
readback, neighboring `0x00008000` refusal without mutation, and snapshot
round trip. Every source and test file remains below 500 lines.

The prior NOR runner is now bounded at the stable pre-timer checkpoint: two
40,000,000-instruction runs produce identical one-line logs with PC
`0x000dac46` and virtual time 160,176,520 ns, after the former page-program
fault time. The new production runner keeps the external full-flash fixture
opt-in and hash-pinned, verifies source immutability, and requires two complete
byte-identical logs. Their SHA-256 is
`32a5bc1df226ca20da0c94aa90dc121fea14125f45890397f3671d6cb95c0b33`;
the first reset moves to instruction 41,435,683 at 278,677,292 ns and the run
halts at PC `0x00079e1e`, instruction 139,585,840, virtual time 398,187,029 ns.

The next exact refusal is CTIMER INTEN at `0x40008060`: pristine instructions
at `0x000cb84c..0x000cb852` read zero, OR firmware-held `0x00004000`, and write
the result. A pre-reset snapshot pins BFAR `0x40008060`, stacked PC
`0x000cb854`, and stacked LR `0x000cb84b`. INTEN is intentionally unchanged by
this ticket and requires separate evidence-gated work.
