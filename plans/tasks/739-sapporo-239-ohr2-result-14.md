# 739 — Sapporo 2.39 OHR2 Result 14

**Status:** done
**Phase:** 7
**Dependencies:** 417, 418, 729

## Goal

Implement the exact MAIN result-command 14 request/reply observed during
Sapporo 2.39 startup, then advance authentic execution to the next strict
boundary.

## Execution Budget

One model-day for state-specific response completion, refusal coverage, and
two deterministic authentic-firmware runs.

## Required Reading

Tickets 417, 418, 729, and 735 through 738; E-SAP-OHR2-001 and the 2.39 OHR2
evidence entries; `docs/{architecture,execution-model,testing-strategy,
compatibility-policy}.md`; `src/devices/sapporo_{devices,ohr2}*`; their
focused tests; and the read-only trace named below.

## Current Baseline

Ticket 738 completes result command 13 and stops at PC `0x0014e8ea`,
instruction 368,995,288, virtual time 1,890,343,532 ns. The next instruction
submits result command `0x000e`, sequence five in MAIN state; its unimplemented
body refuses.

## Allowed Files

- `src/devices/sapporo_ohr2_239.c`
- `tests/devices/test_sapporo_ohr2_239.c`
- `tests/integration/test_firmware_sapporo_239_ohr2_result_14.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/739-sapporo-239-ohr2-result-14.md`

## Frozen Interfaces

Packet framing, CRC, ready timing, selector/read lifecycle, sequence behavior,
snapshot bytes/version, generic command/state rules, existing 2.39 bodies,
legacy 2.22 fixture, profile selection, compatibility descriptors/hit counts,
board wiring, IOM/DMA, and every other device remain unchanged.

## Evidence Inputs

E-SAP-OHR2-RESULT14-239-001 must cite read-only native capture
`$FIRMWARE_ROOT/emulator/display/captures/sapporo-239-native-full-ui/trace.log`
SHA-256 `fd5b6208937d1cfe4479405ea5af5635659ecf3163c8f033da6013d3b858ef6e`,
the OHR transport research SHA-256
`105c9835dd6dd6e5375db1665a3f3582b3c1eb4764aea89eef4722ab5b1c93fc`,
and exact application SHA-256
`85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`.
The trace records command `0x000e`, sequence five with all fifty data bytes
`0xff`. The reply echoes command/sequence, has fifty zero data bytes, and
carries CRC `0x28dddf81`.

## Implementation

Extend only the dedicated 2.39 physical provider. In MAIN, accept command 14
only when all fifty request data bytes are `0xff`; return the exact zero body.
Retain all existing bodies unchanged. Refuse wrong state, malformed fill, and
every later unimplemented response before mutation.

## Tests and Commands

`make test TEST_FILTER=sapporo_ohr2_239`, `make test
TEST_FILTER=sapporo_devices`, `make test TEST_FILTER=sapporo_fixture_providers`,
`make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_ohr2_result_14`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=sapporo_ohr2_239`
must pass.

The focused regression must first fail against the current provider. Tests
cover the exact result body, malformed-fill and wrong-state refusal without
response mutation, retained existing bodies, and later-command refusal. The
private runner requires the exact external flash hash, two byte-identical
runs and snapshots, unchanged source flash and compatibility count, the full
OHR transcript through command 14, and a later explicit checkpoint or
fail-closed boundary.

## Acceptance

The exact result-14 exchange passes through the normal physical path with no
compatibility hit. Malformed requests remain atomic, existing bodies and 2.22
behavior are unchanged, and two exact-firmware runs reach the same later
boundary without reset.

## Forbidden Scope

No echo/command-2 response, new state transition, snapshot change,
compatibility intervention, permissive fill, firmware hook, IOM/DMA change,
firmware byte, profile, renderer, or unrelated change.

## Handoff

Extended only the dedicated profile-selected Sapporo 2.39 physical provider.
Result command 14 in modeled MAIN state requires all fifty data bytes `0xff`
and returns the exact all-zero response body. Wrong state, bad fill, and all
later response commands refuse before mutation; existing bodies remain
unchanged.

Two fresh exact runs and snapshots are byte-identical at PC `0x0014e8ea`,
instruction 369,026,992, virtual time 1,890,375,236 ns (log SHA-256
`8e1e68584a5d869f91c07216add9d6d1befbc36aac26718113359b76298fd1c4`,
snapshot SHA-256
`d7c30abd8ff1744c1644b2730953d45012c547977b24c905873b37fa2533b8e0`).
They complete result command 14 without reset, preserve 118 logical-file
operations, and leave source flash unchanged. One-instruction continuation
identifies echo command `0x0006`, sequence six as the next strict gate; it
refuses at PC `0x001c0db4`.
