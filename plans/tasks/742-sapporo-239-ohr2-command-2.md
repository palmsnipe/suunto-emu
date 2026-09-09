# 742 — Sapporo 2.39 OHR2 Command 2 Integration

**Status:** done
**Phase:** 7
**Dependencies:** 417, 418, 729

## Goal

Integrate the observed command 2 into the OHR2 transport interface and the
Sapporo 2.39 response provider, advancing authentic execution beyond echo.

## Execution Budget

One model-day for packet, snapshot, refusal, and authentic-run verification.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; tickets 417, 418, 729, 741;
E-SAP-OHR2-001 in `plans/evidence/phase3-5.tsv` and
E-SAP-OHR2-ECHO-239-001 in `docs/migration-evidence.md`;
`include/semu/peripheral.h`, `src/devices/sapporo_ohr2.h`,
`src/devices/sapporo_ohr2.c`, `src/devices/sapporo_ohr2_239.h`,
`src/devices/sapporo_ohr2_239.c`, the OHR provider selection in
`src/devices/sapporo_devices.c`, `src/compat/sapporo_222.h`,
`src/compat/sapporo_222_ohr.c`, `tests/devices/test_sapporo_ohr2_239.c`,
`tests/devices/test_sapporo_ohr2_snapshot.c`, and
`tests/integration/test_firmware_sapporo_239_ohr2_echo.sh`.

## Current Baseline

Ticket 741 stops before command 2, sequence seven, at PC `0x0014e8ea`,
instruction 369,037,329, virtual time 1,890,385,573 ns. The generic registry
refuses the next request before invoking the body provider.

## Allowed Files

- `src/devices/sapporo_ohr2.h`, `src/devices/sapporo_ohr2.c`
- `src/devices/sapporo_ohr2_239.c`
- `tests/devices/test_sapporo_ohr2_239_command2.c`
- `tests/integration/test_firmware_sapporo_239_ohr2_command2.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/742-sapporo-239-ohr2-command-2.md`

This integration ticket explicitly owns the command enum and the shared
known-command/state registry changes. No private parallel API is needed.

## Frozen Interfaces

Keep wire framing, CRC algorithm, ready lifecycle, provider signature,
sequence rules, snapshot layout/version, existing command bodies, board
selection, compatibility descriptors, and other devices unchanged.

## Evidence Inputs

E-SAP-OHR2-CMD2-239-001 pins the read-only native-firmware reference capture
`$FIRMWARE_ROOT/emulator/display/captures/sapporo-239-native-full-ui/trace.log`
SHA-256 `fd5b6208937d1cfe4479405ea5af5635659ecf3163c8f033da6013d3b858ef6e`.
Lines 150–155 record command 2, sequence 7 in MAIN, fifty `ff` request data
bytes (CRC `0x8cef3ba3`), and fifty zero response data bytes (CRC `0xf0ffccf3`).
This establishes the reference startup response, not physical sensor behavior
or an inferred meaning for command 2. Firmware hashes remain E-SAP-0011.

## Implementation

Name enum value 2 neutrally as COMMAND_RESULT_2. Accept it in MAIN only in
the shared registry. The 2.39 provider validates every request data byte as
`ff` before supplying the zero body. The existing transport echoes command
and sequence, computes CRC, and drives ready. Other providers still refuse.
The existing snapshot validator must accept a queued command-2 response in
MAIN and reject it in BSL without changing snapshot bytes.

## Tests and Commands

Run `make test TEST_FILTER=sapporo_ohr2_239_command2` before and after the
implementation. Cover the complete response/CRC, ready edges, queued-request
refusal, selector/read sizes, all fifty malformed data positions, bad CRC,
unknown command, wrong state, absent/legacy provider, reset, and queued
snapshot round trip plus atomic invalid-state refusal.

Run `make test TEST_FILTER=sapporo_ohr2`,
`make test TEST_FILTER=sapporo_devices`,
`make test TEST_FILTER=sapporo_fixture_providers`, `make check-task-contracts`,
`make check-lines`, `make check`, and `make sanitize TEST_FILTER=sapporo_ohr2`.
With `SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin`, run
`make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_ohr2_command2`.

## Acceptance

The exact packet exchange succeeds through the normal endpoint. Refusals
preserve device state and response buffers. Two authentic runs and snapshots
match at a measured later boundary; source flash remains immutable. Record
the next unsupported operation without broadening this ticket.

## Forbidden Scope

No command meaning inferred from its number, further commands, measurement
simulation, firmware hooks, CPU/MMIO/renderer changes, compatibility budget
increase, snapshot version change, or older golden weakening.

## Handoff

Implemented and verified; status remains in-progress pending integrator review.
Changed files are exactly the Allowed Files above. References are
E-SAP-OHR2-001, E-SAP-OHR2-ECHO-239-001, E-SAP-OHR2-CMD2-239-001,
E-SAP-0011, and E-SAP-STARTUP-SLEEP-239-001.

The initial narrow regression ran three tests and failed the successful packet
case before implementation. After implementation all three pass. The exact
commands above passed: OHR2 23 tests, board devices 12 tests, fixture providers
10 tests, task contracts (115 tickets), check-lines (existing review-threshold
warnings only), make check, and OHR2 Address/UndefinedBehavior sanitizers
(23 tests). The private command-2 runner passed with all three component hashes
validated, two byte-identical logs/snapshots, an exact one-step resume, and
unchanged source-flash hash.

The new checkpoint is `stop=budget pc=0x00079e1c instructions=393235868
virtual_time_ns=1914584112`. Log SHA-256 is
`9161895c12da70077ec78fb76bae6062196194a80f1df5b8c9609876fa20b17a`;
snapshot SHA-256 is
`c36512287d4bf7d5a06762334ba261d984d0259a1466ec83076e73ebb253dcb0`.
There are 449 logical-file operations, no device refusal or reset, and no
compatibility budget change. The next instruction executes firmware BKPT:
`stop=halt pc=0x00079e1e instructions=393235869 virtual_time_ns=1914584113`.
Read-only snapshot/disassembly evidence identifies the StartupClient failure
path for `sleepln` command zero, result 500; determining its cause is the next
task. No normal frame or physical sensor behavior is claimed. Older checkpoint
goldens and their historical refusal probes were not changed. No further
interface change is requested; the integrator must review before marking done.
