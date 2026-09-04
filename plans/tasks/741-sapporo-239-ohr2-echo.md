# 741 — Sapporo 2.39 OHR2 Exact Echo

**Status:** in-progress
**Phase:** 7
**Dependencies:** 417, 418, 729

## Goal

Implement the exact MAIN echo request/reply observed during Sapporo 2.39
startup, then advance authentic execution to the next strict boundary.

## Execution Budget

One model-day for exact-body response completion, refusal coverage, and two
deterministic authentic-firmware runs.

## Required Reading

Tickets 417, 418, 729, and 735 through 739; E-SAP-OHR2-001 and the 2.39 OHR2
evidence entries; `docs/{architecture,execution-model,testing-strategy,
compatibility-policy}.md`; `src/devices/sapporo_{devices,ohr2}*`; their
focused tests; and the read-only trace named below.

## Current Baseline

Ticket 739 completes result command 14 and stops at PC `0x0014e8ea`,
instruction 369,026,992, virtual time 1,890,375,236 ns. The next instruction
submits echo command `0x0006`, sequence six in MAIN state; its unimplemented
body refuses.

## Allowed Files

- `src/devices/sapporo_ohr2_239.c`
- `tests/devices/test_sapporo_ohr2_239.c`
- `tests/integration/test_firmware_sapporo_239_ohr2_echo.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/741-sapporo-239-ohr2-echo.md`

## Frozen Interfaces

Packet framing, CRC, ready timing, selector/read lifecycle, sequence behavior,
snapshot bytes/version, generic command/state rules, existing 2.39 bodies,
legacy 2.22 fixture, profile selection, compatibility descriptors/hit counts,
board wiring, IOM/DMA, and every other device remain unchanged.

## Evidence Inputs

E-SAP-OHR2-ECHO-239-001 must cite read-only native capture
`$FIRMWARE_ROOT/emulator/display/captures/sapporo-239-native-full-ui/trace.log`
SHA-256 `fd5b6208937d1cfe4479405ea5af5635659ecf3163c8f033da6013d3b858ef6e`,
the OHR transport research SHA-256
`105c9835dd6dd6e5375db1665a3f3582b3c1eb4764aea89eef4722ab5b1c93fc`,
and exact application SHA-256
`85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`.
The trace records command `0x0006`, sequence six. Data bytes 0..9 are
`fc 60 e8 83 d5 01 00 00 00 00`; data bytes 10..49 are `ff`. The reply echoes
all fifty data bytes and carries the same CRC `0xd9fc4f30` as the request
payload. The exact ticket-739 snapshot independently records the current
deterministic guest request at SRAM `0x1002fa0c`: data bytes 0..9 are
`00 f4 51 c2 8c 01 00 00 00 00`, the same `ff` tail, and request CRC
`0xef3ce829`.

## Implementation

Extend only the dedicated 2.39 physical provider. In MAIN, accept echo only
when all fifty request data bytes match either the native capture or the
exact current deterministic guest body; return those data bytes unchanged.
Retain all existing bodies unchanged. Refuse wrong state, every other body,
and every later unimplemented command before mutation.

## Tests and Commands

`make test TEST_FILTER=sapporo_ohr2_239`, `make test
TEST_FILTER=sapporo_devices`, `make test TEST_FILTER=sapporo_fixture_providers`,
`make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_ohr2_echo`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=sapporo_ohr2_239`
must pass.

The focused regression must first fail against the current provider. Tests
cover the exact echoed body, prefix/tail mismatch and wrong-state refusal
without response mutation, and retained existing bodies. The private runner
requires the exact external flash hash, two byte-identical runs and snapshots,
unchanged source flash and compatibility count, the full OHR transcript
through echo, and a later explicit checkpoint or fail-closed boundary.

## Acceptance

The exact echo exchange passes through the normal physical path with no
compatibility hit. Mismatched requests remain atomic, existing bodies and
2.22 behavior are unchanged, and two exact-firmware runs reach the same later
boundary without reset.

## Forbidden Scope

No command-2 response or command registry change, new state transition,
snapshot change, compatibility intervention, permissive echo, firmware hook,
IOM/DMA change, firmware byte, profile, renderer, or unrelated change.

## Handoff

Extended only the dedicated profile-selected Sapporo 2.39 physical provider.
Echo command six in modeled MAIN state accepts only the two complete 50-byte
bodies pinned by the native capture and the current deterministic guest
snapshot, and returns the data bytes unchanged. Wrong state, prefix mismatch,
tail mismatch, and every other echo body refuse before mutation; existing
bodies remain unchanged.

Two fresh exact runs and snapshots are byte-identical at PC `0x0014e8ea`,
instruction 369,037,329, virtual time 1,890,385,573 ns (log SHA-256
`2ef900dbf79d08a83e94c2e6d8e642c53b51fa40d3faaca977319ac64f4d59cf`,
snapshot SHA-256
`5168ba1e48997e23553370705d49f4ea0f83c407337576bb3c7a56cacb308686`).
They complete echo without reset, preserve 118 logical-file operations, and
leave source flash unchanged. One-instruction continuation identifies command
`0x0002`, sequence seven as the next strict gate; it refuses at PC
`0x001c0db4` in the generic command registry, before provider dispatch.
