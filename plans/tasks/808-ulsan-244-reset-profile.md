# 808 — Ulsan 2.44.52 Reset Profile

**Status:** done
**Phase:** 7
**Dependencies:** 720,725

## Goal

Boot pristine Ulsan 2.44.52 firmware under its own profile and record the
first unsupported hardware boundary as the starting point for Service Mode.

## Execution Budget

One integration day. Run two cold boots per checkpoint under explicit budgets.
Keep all firmware and raw logs outside tracked files.

## Required Reading

Read tickets 720 and 725, E-ULS-0003 through E-ULS-0006 and E-ULS-0051 in
`docs/migration-evidence.md`, and
`fixtures/evidence/ulsan/ulsan-2.44.52.contract.semu`. Read the extension
boundaries in `docs/architecture.md`, reset and reproducibility in
`docs/execution-model.md`, and `docs/profile-format.md`,
`docs/testing-strategy.md`, and `docs/compatibility-policy.md`.
Read `include/semu/{manifest,machine,bus}.h`, `src/boards/ulsan_board.{c,h}`,
`src/boards/machine.c`, `src/frontends/cli_profiles.c`, and
`tests/unit/test_ulsan_reset{,_map}.c`.

## Current Baseline

Only 2.35.36 is accepted by the standalone Ulsan board. The 2.44.52 reference
lane uses the same declared memory regions and different startup peripheral
attachments. Its one-second boundary is later than the initial reset scope.

## Allowed Files

- `profiles/ulsan/2.44.52/*.semu`, `profiles/index.semu`
- `src/boards/ulsan_board.{c,h}`, `src/boards/machine.c`,
  `src/frontends/cli_profiles.c`
- `tests/integration/test_firmware_ulsan_244_reset.sh`
- `fixtures/evidence/ulsan/ulsan-2.44.52.contract.semu`,
  `docs/migration-evidence.md`, and this ticket's handoff

## Frozen Interfaces

Use the existing strict manifest, CLI, and machine APIs. Keep 2.35.36
attachments and checkpoints intact. Unmapped peripherals retain the existing
bus fault behavior.

## Evidence Inputs

E-ULS-0003 and E-ULS-0005 pin component identities and vectors. Compare the
reference lane's `ulsan-2.44.52.resc`, `ulsan-platform.repl`,
`ulsan-storage.repl`, and bundled `ambiq-apollo4.repl` for the 2.44.52
memory and reset wiring. Reproduce the bounded reference stop from
`ulsan-2.44.52-boundary.resc` twice and retain normalized checkpoint hashes.

## Implementation

Register the exact component identities and reuse the declared memory map
and observed GPIO pad setup. Map the OTA tail at its XIP address and
leave the personalized prefix unmapped. Execute from the application vector
table with pristine firmware. Capture the first unsupported access and the
resulting standalone stop.

## Tests and Commands

- `make test TEST_FILTER=ulsan`
- `SEMU_FIRMWARE_MANIFEST=/path/to/firmware.semu make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_reset`
- `make check-lines && make check && make sanitize`

## Acceptance

The built-in ID lists and validates the exact three components. A wrong
firmware identity fails before execution. Two independent CLI runs with the
private manifest match the pinned reset transcript and stop. The existing
Ulsan reset and device regressions pass.

## Forbidden Scope

New device behavior, flash-prefix fixtures, Renode translator patches in the
standalone runtime, frame publication, pairing, and BLE.

## Handoff

Record reference and standalone hashes, the precise unsupported access, and
the next evidence needed to advance toward the first Service Mode frame.

E-ULS-0052 records the reference reset tuple and the two-run standalone
checkpoint. The profile passes the observed GPIO initialization and reaches
the PWRCTRL status read at PC `0x00076eca`, address `0x40021008`.
Ticket 809 scopes that gap using the two-run access census E-ULS-0053.
E-ULS-0054 records the twice-reproduced reference Service Mode frame.

Integrator review (2026-11-04): the 2.44.52 firmware gate and all 60 Ulsan
cases passed on a fresh run. The private gate checked the wrong-identity
refusal and both full cold-start transcripts. The implementation's
`make check-lines`, `make check`, and `make sanitize` also passed.
