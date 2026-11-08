# 820 — Ulsan 2.44.52 Native Hello

**Status:** ready
**Phase:** 7
**Dependencies:** 819

## Goal

Trace the firmware's boot-mode decision and reproduce the first stable
firmware-rendered Hello frame in two cold standalone boots. Keep the
existing Service Mode gate passing without an opt-in fixture.

## Execution Budget

Bound each lane and standalone run by virtual time or retired instructions.
Isolate startup gaps by dependency and record each successive boundary.
Cap the first lane selector probes at two virtual seconds.

## Required Reading

Read ticket 819, E-ULS-0033, E-ULS-0052, E-ULS-0054, and
E-ULS-0074 through E-ULS-0077. Read the compatibility, testing,
execution and architecture contracts. Read the reference lane's
`race-s-244-full-ui-checkpoint.md`, `ulsan-244-service-mode-boundary.md`,
`ulsan-244-post-service-startup.md`, the Race S launchers and fixture
builders, and the exact device modules corresponding to each observed gap.

## Current Baseline

The standalone OTA-only profile reaches the E-ULS-0077 Service Mode
frame. The reference lane documents a Hello frame with an explicit
synthetic first-boot fixture and further device coverage.

## Allowed Files

- `docs/migration-evidence.md` and this ticket's handoff
- `src/boards/machine{,_internal}.{c,h}`, `src/boards/ulsan_board.{c,h}`
- `src/devices/ulsan_*.{c,h}`, `src/compat/ulsan_*.{c,h}`
- `src/compat/production_records.{c,h}`, `src/compat/sapporo_235_production.c`
  for the shared synthetic sector encoder
- `src/frontends/cli{,_main,_profiles}.c`
- `include/semu/machine.h` for an optional pinned eMMC input field
- `profiles/ulsan/2.44.52/profile.semu` for opt-in layer declarations
- `tests/devices/test_ulsan_*.c`, `tests/integration/test_ulsan_244_*.c`
- `tests/integration/test_firmware_ulsan_244_*.sh`
- `src/devices/sapporo_nema_gpu{,_internal,_submission}.{c,h}`,
  `src/display/nema_{backend,backend_internal,backend_draw,backend_transaction,framing,state}*.{c,h}`,
  and `src/display/nema_ulsan*.{c,h}` for the shared GPU integration and
  native Race S frame composition
- `src/soc/apollo4/iom{,_internal}.{c,h}` for Race S IOM2 and the observed
  IOM0 motion startup transactions through the shared DMA controller

## Frozen Interfaces

Preserve the default OTA-only profile, existing Service Mode hashes,
firmware validation, existing public API behavior, deterministic instruction timing,
and the fail-closed bus. Source firmware and storage images are immutable.

## Evidence Inputs

E-ULS-0077 pins the negative control. Reproduce the native selector
with and without the documented first-boot inputs. Record exact input
and tooling hashes, normalized selector and device traces, and the
private Hello frame hash before claiming the respective behaviors.

## Implementation

Introduce only reproduced native hardware behavior and explicit,
identity-pinned compatibility inputs required by the observed startup
path. Keep compatibility opt-in, logged and hit-bounded. Obtain
onboarding pixels from the real firmware through the frame callback.
The owner authorized the shared NemaGPU and IOM2 integration in the
continuation of this ticket. Keep the Race S additions profile-scoped.
Use a separate component ticket if a further required interface is out
of scope.

## Tests and Commands

- `make test TEST_FILTER=ulsan`
- `SEMU_ULSAN_244_FIRMWARE_MANIFEST=/path/to/firmware.semu make test TEST_FILTER=ulsan_244_service_frame`
- `SEMU_FIRMWARE_MANIFEST=/path/to/firmware.semu make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_reset`
- `SEMU_ULSAN_244_EMMC_IMAGE=/path/to/emmc.img SEMU_FIRMWARE_MANIFEST=/path/to/firmware.semu make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_hello_startup`
- `make check-task-contracts && make check-lines && make check && make sanitize`

## Acceptance

Record two independently reproduced selector outcomes and two cold
standalone runs publishing the exact 466x466 Hello frame, with verified
firmware identity. Confirm successful and refused device or compatibility
transactions as applicable. Both original Service Mode gates still pass.

## Forbidden Scope

Guest patches, forcing the boot decision or manufacturing validation,
synthetic frames, pairing or further onboarding, generic peripheral
fallbacks, and tracked firmware, fixture images or proprietary pixels.

## Handoff

E-ULS-0078 traces mode 2 with the OTA-only inputs and mode 5 with the
synthetic ProductionData page as the sole change. Enable the in-tree
production-data encoder using `--layer ulsan-2.44-production-data`.
Enable the separately verified synthetic card using
`--layer ulsan-2.44-emmc --ulsan-emmc /path/to/emmc.img`. Use the
reference lane's `tools/build_ulsan_synthetic_ui_fat.py` command in
`emulator/devices/ulsan/run-2.44-full-ui.sh` to generate the image in
a private scratch directory. Its SHA-256 must be
`62b711d026fa42cbcade59e5de6b1f9fb14397dba828a392e993991298c4d962`.
E-ULS-0079 through E-ULS-0081 cover SDIO, the pad selector, and TIMER7.

The two cold pre-GPU runs stop at `0x000b22cc` after 85,000,000
instructions at 558,718,255 ns. The first fault with a larger budget
is NemaGPU `0x40090094`. A scratch attachment of the existing GPU
reaches IOM2 `0x40052104`. E-ULS-0082 records both boundaries.
The integrator needs to extend the shared GPU interface with
Race S register admission and 466×466 retained-frame support, and
bind the observed IOM2 startup endpoints through the shared IOM
controller. Continue checking later device boundaries after wiring
those paths.

Two lane boots reproduce the E-ULS-0080 English Hello frame SHA-256
`9d02076e5fe66dad7ceccf7b4b86c1c59ab5e5f2948906868ba87a1df7ff9c76`.
The corresponding standalone frame acceptance remains open. Both
existing private Service Mode gates pass. The frame gate still
publishes SHA-256
`bc071732e63081b938b4d98e74cdb693a5781365e92abc14bcff99fa924ce526`
on both cold boots.

The private Hello startup gate also exercises missing path/layer and
wrong-size image refusals. A separate CLI run with a 256 MiB zero image
refuses on the SHA-256 check before execution.

The continuation wires the Race S GPU register window and IOM2
through the shared controllers, adds the 466×466 retained renderer,
and models the observed IOM0 motion and TIMER8 startup. The two
standalone runs now reach late SDIO at `0x40070034` after
289,872,617 instructions and 10,942,690,193 ns. See E-ULS-0083
through E-ULS-0086. The focused tests are
`make test TEST_FILTER=ulsan_244_hello_io`,
`make test TEST_FILTER=ulsan_244_nema`, and
`make test TEST_FILTER=ulsan_timer0wake`. The native Hello
acceptance still requires two standalone frame matches.

Files changed for this ticket

- `plans/index.tsv`, this ticket, and `docs/migration-evidence.md`
- `include/semu/machine.h`, `src/frontends/cli.c`,
  `src/boards/machine.c`, `src/boards/ulsan_board.h`, and
  `profiles/ulsan/2.44.52/profile.semu`
- `src/compat/production_records.{c,h}`,
  `src/compat/ulsan_244_production.{c,h}`, and
  `src/compat/sapporo_235_production.c`
- `src/devices/ulsan_sdio.{c,h}`, `src/devices/ulsan_buszero.{c,h}`,
  `src/devices/ulsan_timer0.{c,h}`, and `src/devices/ulsan_pwrctrl.c`
- `tests/devices/test_ulsan_244_production.c`,
  `tests/devices/test_ulsan_244_sdio.c`, `tests/devices/test_ulsan_buszero.c`,
  `tests/devices/test_ulsan_timer0wake.c`, `tests/devices/test_ulsan_pwrctrl.c`,
  and `tests/integration/test_firmware_ulsan_244_hello_startup.sh`
