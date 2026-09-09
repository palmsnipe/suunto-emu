# 737 — Sapporo 2.39 OHR2 MAIN Identity

**Status:** done
**Phase:** 7
**Dependencies:** 417, 418, 729

## Goal

Implement the exact MAIN identity request/reply observed during Sapporo 2.39
startup, then advance authentic execution to the next strict boundary.

## Execution Budget

One model-day for state-specific response completion, refusal coverage, and
two deterministic authentic-firmware runs.

## Required Reading

Tickets 417, 418, 729, 735, and 736; E-SAP-OHR2-001,
E-SAP-OHR2-BOOT-239-001, and E-SAP-OHR2-ID-BSL-239-001;
`docs/{architecture,execution-model,testing-strategy,compatibility-policy}.md`;
`src/devices/sapporo_{devices,ohr2}*`; their focused tests; and the read-only
trace named below.

## Current Baseline

Ticket 736 completes BSL identity, fire-and-forget reboot, and the second
boot-mode exchange. It stops at PC `0x0014e8ea`, instruction 368,947,987,
virtual time 1,890,296,231 ns. The next instruction submits identity command
zero, sequence three in MAIN state; its unimplemented body refuses.

## Allowed Files

- `src/devices/sapporo_ohr2_239.c`
- `tests/devices/test_sapporo_ohr2_239.c`
- `tests/integration/test_firmware_sapporo_239_ohr2_main_identity.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/737-sapporo-239-ohr2-main-identity.md`

## Frozen Interfaces

Packet framing, CRC, ready timing, selector/read lifecycle, sequence behavior,
snapshot bytes/version, generic command/state rules, BSL identity and
boot-mode bodies, legacy 2.22 fixture, profile selection, compatibility
descriptors/hit counts, board wiring, IOM/DMA, and every other device remain
unchanged.

## Evidence Inputs

E-SAP-OHR2-ID-MAIN-239-001 must cite read-only native capture
`$FIRMWARE_ROOT/emulator/display/captures/sapporo-239-native-full-ui/trace.log`
SHA-256 `fd5b6208937d1cfe4479405ea5af5635659ecf3163c8f033da6013d3b858ef6e`,
the OHR transport research SHA-256
`105c9835dd6dd6e5375db1665a3f3582b3c1eb4764aea89eef4722ab5b1c93fc`,
and exact application SHA-256
`85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`.
The trace records command zero, sequence three with all fifty data bytes
`0xff`. The reply echoes command/sequence, is otherwise zero except `MAIN\0`
at payload offsets 9..13, and carries CRC `0x762b1fb4`.

## Implementation

Extend only the dedicated 2.39 physical provider. In MAIN, accept identity
only when all fifty request data bytes are `0xff`; return the exact
zero/`MAIN\0` body. Retain the BSL identity and boot-mode bodies unchanged.
Refuse malformed fill and every later unimplemented response before mutation.

## Tests and Commands

`make test TEST_FILTER=sapporo_ohr2_239`, `make test
TEST_FILTER=sapporo_devices`, `make test TEST_FILTER=sapporo_fixture_providers`,
`make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_ohr2_main_identity`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=sapporo_ohr2_239`
must pass.

The focused regression must first fail against the current provider. Tests
cover the exact MAIN body, unchanged BSL body, bad-fill refusal without
response mutation, retained boot-mode behavior, and later-command refusal.
The private runner requires the exact external flash hash, two byte-identical
runs and snapshots, unchanged source flash and compatibility count, the full
OHR transcript through MAIN identity, and a later explicit checkpoint or
fail-closed boundary.

## Acceptance

The exact MAIN identity exchange passes through the normal physical path with
no compatibility hit. Malformed requests remain atomic, existing bodies and
2.22 behavior are unchanged, and two exact-firmware runs reach the same later
boundary without reset.

## Forbidden Scope

No result/echo response, new state transition, snapshot change, compatibility
intervention, permissive fill, firmware hook, IOM/DMA change, firmware byte,
profile, renderer, or unrelated change.

## Handoff

Extended only the dedicated profile-selected Sapporo 2.39 physical provider.
Identity requests in modeled BSL or MAIN state require all fifty data bytes
`0xff` and return the exact otherwise-zero `BSL\0` or `MAIN\0` body. Bad fill
in either state and all later response commands refuse before mutation;
boot-mode behavior remains unchanged.

Two fresh exact runs and snapshots are byte-identical at PC `0x0014e8ea`,
instruction 368,958,374, virtual time 1,890,306,618 ns (log SHA-256
`d82ebc5b061787b8cefad7f64f7b70168858bc8da29adb644cd486211a8bfc22`,
snapshot SHA-256
`352cdedcec47360eb478c6eec3649534025c7373b19c9c35c90e3922549c8a81`).
They complete MAIN identity without reset, preserve 118 logical-file
operations, and leave source flash unchanged. One-instruction continuation
identifies result command `0x000d`, sequence four as the next strict gate; it
refuses at PC `0x001c0db4`.
