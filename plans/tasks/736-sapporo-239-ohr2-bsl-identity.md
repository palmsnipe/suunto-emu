# 736 — Sapporo 2.39 OHR2 BSL Identity

**Status:** done
**Phase:** 7
**Dependencies:** 417, 418, 729

## Goal

Implement the exact BSL identity request/reply observed during Sapporo 2.39
startup, then advance authentic execution to the next strict boundary.

## Execution Budget

One model-day for provider separation, exact request validation, refusal
coverage, and two deterministic authentic-firmware runs.

## Required Reading

Tickets 417, 418, 729, and 735; E-SAP-OHR2-001 and
E-SAP-OHR2-BOOT-239-001; `docs/{architecture,execution-model,
testing-strategy,compatibility-policy}.md`; `src/devices/sapporo_{devices,
ohr2}*`; `src/compat/sapporo_222_ohr.c`; their focused tests; and the
read-only trace named below.

## Current Baseline

The committed ticket-735 implementation completes command `0x0010`, sequence
zero and stops at PC `0x0014e8ea`, instruction 359,790,038, virtual time
1,881,138,282 ns. The next instruction submits identity command zero,
sequence one in BSL state. Its missing 2.39 response body refuses and the
guest enters the precise fault vector.

## Allowed Files

- `src/devices/sapporo_devices.c`, `src/devices/sapporo_devices_internal.h`
- `src/devices/sapporo_ohr2_239.c`, `src/devices/sapporo_ohr2_239.h`
- `tests/devices/test_sapporo_devices.c`
- `tests/devices/test_sapporo_ohr2_239.c`
- `tests/integration/test_firmware_sapporo_239_ohr2_bsl_identity.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/736-sapporo-239-ohr2-bsl-identity.md`

## Frozen Interfaces

Packet framing, CRC, ready timing, selector/read lifecycle, sequence behavior,
snapshot bytes/version, generic command/state rules, legacy 2.22 fixture,
compatibility descriptors/hit counts, board wiring, profiles, IOM/DMA, and all
other device behavior remain unchanged.

## Evidence Inputs

E-SAP-OHR2-ID-BSL-239-001 must cite read-only native capture
`$FIRMWARE_ROOT/emulator/display/captures/sapporo-239-native-full-ui/trace.log`
SHA-256 `fd5b6208937d1cfe4479405ea5af5635659ecf3163c8f033da6013d3b858ef6e`,
the OHR transport research SHA-256
`105c9835dd6dd6e5375db1665a3f3582b3c1eb4764aea89eef4722ab5b1c93fc`,
and exact application SHA-256
`85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`.
The trace records command zero, sequence one with all fifty data bytes
`0xff`. The reply echoes command/sequence, is otherwise zero except `BSL\0`
at payload offsets 9..12, and carries CRC `0x4b0c33f7`.

## Implementation

Move exact 2.39 OHR bodies to a dedicated physical provider. Preserve the
ticket-735 zero boot-mode body. In BSL, accept identity only when all fifty
request data bytes are `0xff`; return the exact zero/`BSL\0` body. Refuse a
bad fill and every unimplemented 2.39 body before response queuing or ready
mutation. Do not route any 2.39 command to the 2.22 compatibility provider.

## Tests and Commands

`make test TEST_FILTER=sapporo_ohr2_239`, `make test
TEST_FILTER=sapporo_devices`, `make test TEST_FILTER=sapporo_ohr2`,
`make test TEST_FILTER=sapporo_fixture_providers`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_ohr2_bsl_identity`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=sapporo_ohr2_239`
must pass.

The focused regression must first fail against the current provider. Tests
cover the exact BSL body and CRC through IOM2, malformed fill refusal without
response mutation, MAIN identity refusal, retained boot-mode behavior, and
continued 2.22 refusal. The private runner requires the exact external flash
hash, two byte-identical runs and snapshots, unchanged source flash and
compatibility count, the complete native transcript through reboot and the
second boot-mode exchange, and a later explicit checkpoint or fail-closed
boundary.

## Acceptance

The exact BSL identity exchange passes through the normal physical path with
no compatibility hit. Malformed and cross-state forms remain atomic, existing
2.22 behavior is unchanged, and two exact-firmware runs reach the same later
boundary without reset.

## Forbidden Scope

No MAIN identity response, result/echo response, new state transition,
snapshot change, compatibility intervention, permissive fill, firmware hook,
IOM/DMA change, firmware byte, profile, renderer, or unrelated change.

## Handoff

Implemented a dedicated profile-selected Sapporo 2.39 physical OHR body
provider. It retains the exact boot-mode reply and accepts BSL identity only
with all fifty request data bytes `0xff`, returning the otherwise-zero body
with `BSL\0` at offsets 9..12. Bad fill, MAIN identity, and all other bodies
refuse before response mutation, and 2.39 never falls through to the 2.22
compatibility fixture.

Two fresh exact runs and snapshots are byte-identical at PC `0x0014e8ea`,
instruction 368,947,987, virtual time 1,890,296,231 ns (log SHA-256
`c5599a2faf3016cdeb85bbb2cd6951f70fad49d6732639bbda861d7f5348c1ed`,
snapshot SHA-256
`2a823cb69c1bdb7463233c553a2e55312c462bca99aa1715246cb1fd3866d690`).
They complete BSL identity, the existing fire-and-forget reboot, and the
second boot-mode exchange in MAIN without reset, preserve 118 logical-file
operations, and leave source flash unchanged. One-instruction continuation
identifies MAIN identity command zero, sequence three as the next strict gate;
it refuses at PC `0x001c0db4`.
