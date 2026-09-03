# 734 — Sapporo 2.39 GPIO WT1 Readback

**Status:** in-progress
**Phase:** 7
**Dependencies:** 305, 729

## Goal

Expose the exact Apollo4 GPIO bank-1 output-state read used by Sapporo 2.39,
then advance authentic execution to the next deterministic boundary.

## Execution Budget

One model-day for register evidence and guest attribution, one strict readback,
positive/refusal tests, and two deterministic authentic-firmware runs.

## Required Reading

Tickets 305, 729, and 733; E-A4-GPIO-001 and
E-SAP-HAPTIC-CAL-239-001; `docs/{architecture,execution-model,
testing-strategy,compatibility-policy}.md`; `src/soc/apollo4/gpio*` and its
focused tests; plus every read-only source named below.

## Current Baseline

Ticket 733 reaches guest PC `0x000cceb2`, instruction 357,033,113, virtual
time 1,878,381,357 ns. The instruction is `LDR r1, [r1]`, a word read from
GPIO address `0x40010218`, and one-instruction continuation enters the precise
fault vector.

## Allowed Files

- `src/soc/apollo4/gpio.c`
- `tests/devices/test_apollo4_gpio.c`
- `tests/integration/test_firmware_sapporo_239_gpio_wt1.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/734-sapporo-239-gpio-wt1.md`

## Frozen Interfaces

GPIO public headers, direct WT writes, WT banks 0/2/3, pin configuration,
inputs, set/clear behavior, interrupts, output observers, snapshot bytes and
version, board wiring, profiles, compatibility, and every other device remain
unchanged.

## Evidence Inputs

E-SAP-GPIO-WT1-239-001 must cite Apollo4 Plus PAC 1.0.0 crate SHA-256
`1820cb448e123a06aa3cabff2427b71138931dfa8d67b86af2c53dd2be02b9e2`,
`gpio.rs` `a73c178c00164f03a17ccb192364d306fe7fb61922ae524e660bbe5bd8817c03`,
and `gpio/wt1.rs`
`37bb353d62d8094028dffaa60a9ec3101a78d4495d0f76abcdb013fb5ec71410`;
the read-only native GPIO trace SHA-256
`d8e7c7a7d73583525e6a10a7e794bce3698050889c3b1060754fbbad110fe2c6`;
and the exact application/disassembly pinned by E-SAP-0011. The PAC identifies
offset `0x218` as 32-bit read/write WT1, reset zero, whose reads reflect output
status including WTS/WTC effects. Pristine `0x000cce98..0x000ccebe` selects
the WT bank for operation 1, reads one bank word, and extracts the requested
pin; the faulting invocation queries pin 53. At the boundary, existing traced
WTS/WTC behavior leaves bank state `0x00040000`, hence pin 53 is low.

## Implementation

Accept an aligned 32-bit read at WT1 offset `0x218` and return existing output
bank 1 state. Do not add state or snapshot bytes. Continue to refuse direct
WT writes and all other WT banks.

## Tests and Commands

`make test TEST_FILTER=apollo4_gpio`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gpio_wt1`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=apollo4_gpio` must
pass.

The focused regression must first fail against the current GPIO model. It
covers reset-zero WT1, WTS1/WTC1 reflected readback and observer behavior,
direct-write refusal without mutation, wrong-width refusal, and continued
refusal of WT0/WT2/WT3. The private runner requires the exact external flash
hash, two byte-identical runs, unchanged source flash, absence of the ticket-
733 fault/reset, unchanged compatibility count, and a later explicit
checkpoint or fail-closed boundary.

## Acceptance

The exact Sapporo 2.39 firmware completes its pin-53 output-state query and
continues twice identically without the diagnosed fault, reset, or a new
compatibility hit; source flash and GPIO snapshot bytes remain unchanged; all
frozen WT accesses remain refused; and execution reaches a later checkpoint
or newly identified strict boundary.

## Forbidden Scope

No direct WT write, additional WT/input/enable/interrupt register, guessed
pin level, new state or snapshot byte/version, observer change, compatibility
hook, reset suppression, firmware byte, profile, renderer, or unrelated
change.

## Handoff

Implemented one aligned 32-bit WT1 read at offset `0x218`, returning existing
output bank 1 state. Focused tests pin reset zero, WTS1/WTC1 reflected
readback, unchanged observer transitions, direct-write and wrong-width
refusal atomicity, and continued WT0/WT2/WT3 refusal. No state or snapshot
field changed. Two fresh authentic runs are byte-identical at PC `0x0014e8ea`,
instruction 359,772,704, virtual time 1,881,120,948 ns (log SHA-256
`48c514ba4504a25122c60e71e2b3966fba9463edc9a3641b4ba3ab5854ff9e02`,
snapshot SHA-256
`20febdf889a8d8baf7d146f6a1f1bcac6182d009b4ad3ed4b4ace4bd9f28d81a`).
Both preserve 118 logical-file interventions and leave the source flash
unchanged. One-instruction continuation emits the next strict refusal: OHR2
command `0x0010`, sequence zero, BSL state, then enters the fault vector at PC
`0x001c0db4`. That command's identity, response, ready behavior, and state
transition remain unresolved and unsupported.
