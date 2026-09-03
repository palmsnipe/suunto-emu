# 733 — Sapporo 2.39 Haptic Calibration Fixture

**Status:** in-progress
**Phase:** 7
**Dependencies:** 413, 729

## Goal

Expose the exact deterministic calibration-byte fixture read after Sapporo
haptic autotune, then advance authentic 2.39 execution to the next fail-closed
boundary.

## Execution Budget

One model-day for evidence pinning, two strict read-only register values,
positive/refusal tests, and two deterministic authentic-firmware runs.

## Required Reading

Tickets 413, 729, 731, and 732; E-SAP-HAPTIC-001,
E-SAP-IOM4-HAPTIC-239-001, and E-SAP-LPS22-239-001;
`docs/{architecture,execution-model,testing-strategy,compatibility-policy}.md`;
`src/devices/sapporo_haptic.*` and its focused tests; plus every read-only
firmware source named below.

## Current Baseline

Ticket 732 reaches native command `0x23000112` at PC `0x0014e8ea`, instruction
122,457,908, virtual time 1,230,996,595 ns. The strict haptic endpoint accepts
the preceding autotune-complete read at register `0x22` but refuses calibration
register `0x23`, entering the fault vector on the next instruction. Reference
firmware reads `0x23` and `0x24` separately after observing complete bit 1.

## Allowed Files

- `src/devices/sapporo_haptic.c`
- `tests/devices/test_sapporo_haptic.c`
- `tests/integration/test_firmware_sapporo_239_haptic_calibration.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/733-sapporo-239-haptic-calibration.md`

## Frozen Interfaces

IOM, DMA, serial endpoint shape, haptic writable state and snapshot bytes,
profiles, compatibility, and every register other than `0x23`/`0x24` remain
unchanged.

## Evidence Inputs

E-SAP-HAPTIC-CAL-239-001 must cite read-only
`$FIRMWARE_ROOT/docs/research/feedback-startup-haptic.md` SHA-256
`64858799bdfe96e56b918051e05051ea52cc7c4729f11e0c11a9f70219d6a13e`,
`emulator/renode/haptic/SapporoHapticPmic.cs`
`fc7533bbcca2d6cb15d22edf0b40bb884c7df702d58f9f2bac0bbd76b291e968`,
and native startup probe log
`c987d798436da654ab17955e6830fd93f288086e42ceac5c5811ea56f0995357`.
The research pins the separate `0x23`/`0x24` reads after complete bit 1. The
reference endpoint resets its 256-byte register array to zero and neither
register is written in the native transcript, defining zero as the synthetic
fixture without claiming physical calibration.

## Implementation

Add registers `0x23` and `0x24` as zero-valued, read-only calibration fixture
bytes. Accept only the native one-byte read shape for either register. Refuse
writes and multi-byte spans before selector or register-state mutation.

## Tests and Commands

`make test TEST_FILTER=sapporo_haptic`, `make test
TEST_FILTER=apollo4_iom_haptic`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_haptic_calibration`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=sapporo_haptic`
must pass.

The focused regression must first fail against the current endpoint. It covers
both calibration bytes after autotune, write refusal without autotune-state
mutation, multi-byte-span refusal, reset, and repeatability. The private runner
requires the exact external flash hash, two byte-identical runs, unchanged
source flash, absence of the ticket-732 fault/reset, unchanged compatibility
count, and a later explicit checkpoint or fail-closed boundary.

## Acceptance

The exact Sapporo 2.39 firmware reads both calibration bytes and continues
twice identically without the diagnosed fault or a new compatibility hit;
source flash and haptic snapshot bytes remain unchanged; unsupported spans and
writes remain atomic; and execution reaches a later checkpoint or newly
identified fail-closed boundary.

## Forbidden Scope

No physical calibration claim, nonzero calibration value, writable calibration
state, new snapshot byte/version, haptic timing or waveform change, IOM change,
compatibility hook, reset suppression, firmware byte, profile, renderer, or
unrelated change.

## Handoff

Implemented the two calibration registers as stateless, zero-valued,
read-only fixtures. The focused regression covers the native post-autotune
reads, write and multi-byte refusal atomicity, reset, and repeatability; no
snapshot field or format changed. Two fresh authentic runs are byte-identical
at PC `0x000cceb2`, instruction 357,033,113, virtual time 1,878,381,357 ns
(log SHA-256
`3aee5f2f3271f54448ab2ca681908e6dfa766348b4dfbe0e2099add4c7b24ca7`,
snapshot SHA-256
`c287c2c1e256e55c100b083a6db1b35730a646ad9aabeea21600347873a9e95e`).
Both preserve 118 logical-file interventions and leave the exact source flash
unchanged. One-instruction continuation enters the precise fault vector at PC
`0x001c0db4`; LLDB localization identifies the next independent evidence gate
as a word read by guest PC `0x000cceb2` from unsupported GPIO address
`0x40010218` (offset `0x218`). Physical calibration values and that GPIO
register remain unresolved and unsupported.
