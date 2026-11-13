# 827 — Ulsan 2.44.52 UART3 Transport

**Status:** done
**Phase:** 7
**Dependencies:** 825

## Goal

Expose the Race S UART3 controller at `0x4001f000` with IRQ18,
CTS flow control, receive timeout and transmit completion. Exercise
it with a deterministic test endpoint. In the reference lane, replay
the observed exchange after the second UART3 open and retry Connect.

## Execution Budget

Limit each cold Renode boot to 100 virtual seconds and six host
minutes. Keep the scripts, raw logs, media and captures outside Git.

## Required Reading

Read tickets 825 and 826, E-ULS-0102 through E-ULS-0104,
E-ULS-0110 and E-ULS-0116, the Apollo4 and UART source and snapshot
codecs, scheduler contracts and UART and Race S tests.

## Current Baseline

The standalone Apollo4 maps UART1 for GPS. The Race S Renode
lane maps UART3 to IRQ18. The controlled lane can open the port,
receive the observed exchange and accept the sequence-3 reply.
It opens UART3 again at the pairing request.

## Allowed Files

- `plans/index.tsv`, this ticket and ticket 826's evidence handoff
- `docs/migration-evidence.md`
- `src/soc/apollo4/apollo4.c`, `apollo4_internal.h`, and `apollo4_snapshot.c`
- `src/soc/apollo4/uart.c`, `uart.h`, `uart_internal.h`, and `uart_snapshot.c`
- `src/core/scheduler.c` and `src/core/scheduler_internal.h`
- `src/boards/machine_snapshot.c`, `machine_snapshot_events.c` and `machine_snapshot_scheduler.c`
- `tests/devices/test_apollo4_uart.c`, `tests/unit/test_core.c` and `tests/unit/test_machine_snapshot.c`

## Frozen Interfaces

Preserve UART1, the existing standalone checkpoints, and the firmware
and reference lane. Preserve older snapshot loading.

## Evidence Inputs

Use the Race S mapping and native register census in E-ULS-0103,
E-ULS-0104 and E-ULS-0110. Use the [Apollo4 Programmer's Guide §14](https://ambiq.com/wp-content/uploads/2022/10/Apollo4-Family-Programmers-Guide.pdf)
for the FIFOs and flow control and [Arm DDI 0183G §§2.6, 2.8.4 and 3.3](https://documentation-service.arm.com/static/5e8e36c2fd977155116a90b5)
for line timing, flags and receive timeout. Use E-ULS-0116 as the
starting point for the second-open replay.

## Implementation

Scope the new controller instance to the Race S 2.44.52 profile.
Use virtual time for the evidenced 1-Mbaud, 8N1 mode and a distinct
scheduler event identity for each UART3 event class. Check that the
port can open and exchange bytes with a test endpoint and that
unknown accesses refuse. In Renode, measure both opens and report
the second exchange and the native Connect result.

## Tests and Commands

- `make test TEST_FILTER=apollo4_uart`
- `make test TEST_FILTER=uart_snapshot`
- `make test TEST_FILTER=machine_snapshot`
- `make test TEST_FILTER=ulsan` with the pinned private manifest and eMMC
- `make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_` with private inputs
- `make check-task-contracts && make check-lines && make check && make sanitize`

## Acceptance

Tests demonstrate CTS gating, IRQ18, RX timeout, TX completion,
reset and snapshot behavior, and refusal of unsupported accesses.
Two cold Renode boots reproduce the second-open replay and native
Connect result, or identify the first repeatable blocking condition.
Hello, Service Mode and both pairing-guidance routes pass.

## Forbidden Scope

Synthesizing the cold radio boot or enabling a production BLE peer.
Ticket 826 requires independently verified cold-radio timing.

## Handoff

The Race S profile maps UART3 at `0x4001f000` and routes IRQ18.
The transport admits the observed 1-Mbaud 8N1 mode, with
explicit CTS input, 32-byte FIFOs, 32-bit-time receive timeout
and a TX-complete interrupt when the timed transmit queue drains.
The test endpoint supplies bytes through the UART interface.
The scheduler and Apollo4 snapshot restore pending UART3 events,
and an older snapshot restores UART3 at reset defaults.

E-ULS-0117 records two matched cold second-open traces and a
matched lower-button retry pair. The watch dispatches the
already-pending pairing request when BLE returns to ON, emits
sequence 6, then goes back to guidance on the native UI retry.
The sequence-6 peer response and cold-radio timing remain open.

The focused UART and snapshot tests passed. All 89 Race S cases
passed, including two cold standalone boots each for Hello,
Service Mode, crown-only and mixed-input guidance. Both firmware-era
scripts, task contracts, line checks, `make check` and `make sanitize`
passed. Private verification logs are under `/tmp/semu-827.yXqNuX`.

- `test-ulsan.log` — `570efa4c5ec05258a9887bbb22de7ffea035224c263f4b49293687d7b14982e7`
- `era.log` — `65a61d1075bc1d0e4da4c4424c9658b7bd55bfd3d41a8a05fe6aca1cda497c81`
- `check-final.log` — `8d49b8915201d725baa931bd8ea16695b76c8a041eac6f48f7221401c0121fa7`
- `sanitize.log` — `00b6f838bf32b59eb9a97fe12131561b4a361b9df84251c54aad14b269857ca5`

## Integrator decision (2026-11-12, project owner): `ready` -> `done`

The acceptance is met by the recorded handoff and E-ULS-0117: focused UART
and snapshot tests pass, all 89 Race S cases pass, both firmware-era scripts,
task contracts, line checks, `make check` and `make sanitize` pass, and two
matched cold second-open traces plus a matched lower-button retry pair
reproduce the second-open replay and native Connect result. The sequence-6
peer response and cold-radio timing remain open under ticket 826, which stays
blocked on a hash-pinned radio-peer source.
