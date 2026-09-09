# 735 — Sapporo 2.39 OHR2 Boot Mode

**Status:** done
**Phase:** 7
**Dependencies:** 417, 418, 729

## Goal

Implement the exact OHR2 command `0x0010` request/reply observed during
Sapporo 2.39 startup, then advance authentic execution to the next strict
boundary.

## Execution Budget

One model-day for transcript attribution, a strict command shape, the
version-pinned response body, refusal tests, and two deterministic authentic
firmware runs.

## Required Reading

Tickets 417, 418, 729, and 734; E-SAP-OHR2-001,
E-SAP-GPIO-WT1-239-001; `docs/{architecture,execution-model,
testing-strategy,compatibility-policy}.md`; `src/devices/sapporo_ohr2.*`,
`src/compat/sapporo_222_ohr.c`, their focused tests, and the read-only trace
named below.

## Current Baseline

Ticket 734 reaches an OHR2 request at PC `0x0014e8ea`, instruction
359,772,704, virtual time 1,881,120,948 ns. The request is command `0x0010`,
sequence zero, while the endpoint is in BSL; the strict transport refuses the
unknown command and the next instruction enters the precise fault vector.

## Allowed Files

- `src/devices/sapporo_ohr2.c`, `src/devices/sapporo_ohr2.h`
- `src/devices/sapporo_devices.c`, `src/devices/sapporo_devices_internal.h`
- `src/compat/sapporo_222_ohr.c`
- `tests/devices/test_sapporo_ohr2.c`
- `tests/devices/test_sapporo_devices.c`
- `tests/devices/test_sapporo_fixture_providers.c`
- `tests/integration/test_firmware_sapporo_239_ohr2_boot_mode.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/735-sapporo-239-ohr2-boot-mode.md`

## Frozen Interfaces

Packet sizes, CRC, ready timing, selector/read lifecycle, sequence behavior,
snapshot bytes/version, BSL-to-MAIN transition, all existing commands,
profiles, compatibility descriptors and hit counts, board wiring, IOM, DMA,
and every other device remain unchanged.

## Evidence Inputs

E-SAP-OHR2-BOOT-239-001 must cite read-only native capture
`$FIRMWARE_ROOT/emulator/display/captures/sapporo-239-native-full-ui/trace.log`
SHA-256 `fd5b6208937d1cfe4479405ea5af5635659ecf3163c8f033da6013d3b858ef6e`
and the exact application hash pinned by E-SAP-0011. The trace records two
complete command-`0x0010` exchanges: sequence zero before BSL identity and
sequence two after command-3 reboot but before MAIN identity. In both states,
the command data is byte `0x01` followed by forty-nine `0xff` bytes; the reply
echoes command/sequence and otherwise contains zero, with valid CRC and the
normal ready/select/read lifecycle. E-SAP-OHR2-001 separately establishes
that GPIO62 is low at reset, high while a reply is queued, and low after read.

## Implementation

Name command `0x0010` as the observed boot-mode command. Accept it in BSL and
MAIN only when its complete data field is exactly `01 ff...ff`; refuse all
other payloads before queuing a response or changing ready. The exact 2.39
profile-selected device provider returns a zero body without changing OHR
state or requiring a compatibility layer. Keep the 2.22 fixture refusing this
command. Reset must drive the endpoint's low ready level onto GPIO62 even when
the internal level was already low, so the first queued reply produces the
observed low-to-high transition.

## Tests and Commands

`make test TEST_FILTER=sapporo_ohr2`, `make test
TEST_FILTER=sapporo_fixture_providers`, `make test
TEST_FILTER=sapporo_devices`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_ohr2_boot_mode`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=sapporo_ohr2` must
pass.

The focused regression must first fail against the current transport. It
covers the exact BSL and MAIN exchanges, zero body and CRC, no state
transition, reset-low drive, bad mode/fill refusal without ready mutation, and
the continued 2.22 fixture refusal. The private runner requires the exact external flash
hash, two byte-identical runs, unchanged source flash and compatibility count,
absence of the ticket-734 refusal/fault, and a later explicit checkpoint or
new fail-closed boundary.

## Acceptance

The first evidenced exchange passes through the normal IOM2/OHR2 path, both
BSL and MAIN forms have focused transport coverage, malformed variants remain
atomic, 2.22 behavior is unchanged, and two exact-firmware runs reach the same
later boundary without reset or a new compatibility hit.

## Forbidden Scope

No guessed command data or response field, extra OHR command, new transition,
snapshot change, compatibility descriptor/hit, permissive fill, firmware
hook, IOM/DMA change, firmware byte, profile, renderer, or unrelated change.

## Handoff

Implemented exact command `0x0010` payload validation in BSL and MAIN, a
profile-selected 2.39 zero response, and an explicit reset-low ready drive.
Malformed mode/fill bytes refuse before mutation, and the 2.22 compatibility
provider still refuses this command. Two fresh exact runs and snapshots are
byte-identical at PC `0x0014e8ea`, instruction 359,790,038, virtual time
1,881,138,282 ns (log SHA-256
`b8977bf8911cc5435e19afc109205c267e822249c17e19fba3809779d046664e`,
snapshot SHA-256
`b7d1d84e2be435635cc6031b8424ece436b6557d3ba3883c59f92b7550916f86`).
Both runs complete the first request/response with the evidenced ready order,
contain no reset, preserve 118 logical-file interventions, and leave source
flash unchanged. One-instruction continuation identifies the next independent
gate as BSL identity command zero, sequence one; the 2.39 body is not yet
wired, so it refuses and enters the precise fault vector at PC `0x001c0db4`.
