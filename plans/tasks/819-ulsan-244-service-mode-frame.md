# 819 — Ulsan 2.44.52 Service Mode Frame

**Status:** done
**Phase:** 7
**Dependencies:** 818

## Goal

Publish the first firmware-rendered Race S 2.44.52 Service Mode
frame from the standalone machine's native display callback.

## Execution Budget

One controller attachment and its observed L4 scanout. Run two
cold boots capped at 160,000,000 instructions and 8,000,000,000 ns.

## Required Reading

Read tickets 808 and 818, E-ULS-0039, E-ULS-0052,
E-ULS-0054, E-ULS-0075 and E-ULS-0076. Read the execution model's
frame and DMA rules, the cited lane display source and observer,
`src/devices/ulsan_disp.{c,h}`, `src/boards/ulsan_board.{c,h}`,
`src/boards/machine.c`, `include/semu/{bus,frame,machine}.h`,
the Ulsan display device tests and the private reset gate.

## Current Baseline

The first refused access is the lane-observed write to
`0x400a8074` at PC `0x000f3632`. The scratch display attachment
reaches the three enabled L4 PLAY submissions. Decoded guest
pixels for the 42-row submission match E-ULS-0054 byte for byte.

## Allowed Files

- `src/devices/ulsan_disp*.{c,h}`
- `src/boards/ulsan_board.{c,h}`, `src/boards/machine.c`,
  `src/boards/machine_internal.h`
- `tests/devices/test_ulsan_disp.c`
- `tests/integration/test_ulsan_244_service_frame.c`
- `tests/integration/test_firmware_ulsan_244_reset.sh`
- `docs/migration-evidence.md` and this ticket's handoff

## Frozen Interfaces

Use the existing frame callback and frame format. Preserve
the 2.35.36 controller behavior, CPU timing, firmware identity
checks, and public APIs.

## Evidence Inputs

E-ULS-0039 identifies the display register plane and IRQ law.
E-ULS-0054 pins the reference Service Mode frame and PLAY path.
E-ULS-0075 identifies the full 2.44.52 MMIO access set and the
three independent scratch layer outputs. E-ULS-0076 preserves
the previous firmware checkpoint.

## Implementation

Attach a machine-owned display controller with IRQ 29. Admit
the observed word offsets per direction. On the observed active
PLAY command, validate the mode, destination and complete packed
SRAM span before committing the display, IRQ and frame. Decode
the indexed L4 layer into little-endian RGB565 on a black
466x466 surface. Publish through the machine's frame callback.

## Tests and Commands

- `make test TEST_FILTER=ulsan_disp`
- `make test TEST_FILTER=ulsan`
- `SEMU_ULSAN_244_FIRMWARE_MANIFEST=/path/to/firmware.semu make test TEST_FILTER=ulsan_244_service_frame`
- `SEMU_FIRMWARE_MANIFEST=/path/to/firmware.semu make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_reset`
- `make check-lines && make check && make sanitize`

## Acceptance

Verify palette conversion, nibble order, placement, the background,
IRQ completion and refusal without partial frame or IRQ mutation.
In two independent firmware boots, verify the ordered frame
callbacks, including the first Service Mode frame SHA-256
`bc071732e63081b938b4d98e74cdb693a5781365e92abc14bcff99fa924ce526`.
Verify the 466x466 size and 3,150 nonzero pixels within
`x=90..373 y=6..32`. Existing Ulsan pins pass and the private
CLI gate matches both standalone boot transcripts.

## Forbidden Scope

Additional pixel formats, input behavior, snapshot changes,
compatibility hooks, firmware patches, and tracked frame pixels.

## Handoff

Record the exact display callback transcript, Service Mode hash,
CLI checkpoint, test results and remaining coverage limits.

Integrator review (2026-11-04). The private callback test failed
before display attachment and passes twice per invocation with
three ordered frames. Generation 3 is Service Mode. Its 434,312
bytes match both E-ULS-0054 Renode frames directly. E-ULS-0077
records the production callback hashes and the two-run CLI
checkpoint.

All commands under Tests and Commands passed. The sanitized
callback test also passed with the private manifest. Two
`SDL_VIDEODRIVER=dummy` runs of `build/suunto-emu-sdl run`
with the same profile, verified manifest and budgets reached
the expected checkpoint. Coverage is limited to the observed
display MMIO and indexed L4 scanout through Service Mode.
