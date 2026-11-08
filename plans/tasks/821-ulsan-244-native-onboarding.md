# 821 — Ulsan 2.44.52 Native Onboarding Input

**Status:** done
**Phase:** 7
**Dependencies:** 820

## Goal

Use the Race S crown and touch controller during native 2.44.52 onboarding.
Walk the firmware's supported setup flow to its documented pairing boundary
while preserving the standalone Hello and Service Mode checkpoints.

## Execution Budget

Bound each cold run by instructions and virtual time. Capture input sequences
at the first interactive screen before attempting the full setup walk.

## Required Reading

Read ticket 820, E-ULS-0077, E-ULS-0080, and E-ULS-0088 through E-ULS-0095,
the input and verification sections of the architecture, execution and testing
contracts, and the compatibility policy. Read the lane's
`race-s-244-full-ui-checkpoint.md`, `race_s_setup_auto_advance.py`,
`Apollo4IomDma.cs` Race S endpoints and the IOM3/IOM6 platform routes.
Read the in-tree input event, GPIO and IOM contracts, board wiring, crown
and ALS endpoints, SDL input adapter, and existing Race S regressions.

## Current Baseline

Ticket 820 verifies two cold 466×466 Hello frames. The standalone IOM6
PAT9126 supplies idle startup values, IOM3 serves ALS, and Ulsan host input
is refused. The reference lane has reached the pairing guidance.

## Allowed Files

- This ticket, `plans/index.tsv` for integration, and `docs/migration-evidence.md`
- `include/semu/input.h` for existing event semantics
- `src/boards/machine.c`, `src/boards/ulsan_board.{c,h}`
- `src/devices/ulsan_*.{c,h}` for input and evidenced setup traffic
- `src/soc/apollo4/{gpio,iom}{,_internal}.{c,h}` for observed input transport
- `src/frontends/{sdl_input,main_sdl}.{c,h}` and `src/frontends/sdl_present.c`
  for live input
- `src/display/nema_ulsan*.{c,h}`, `src/display/nema_state{,_internal}.{c,h}`,
  and the existing Nema backend and GPU modules for evidenced setup rendering
- `tests/devices/test_ulsan_*.c`, `tests/sdl/test_sdl_input.c`, and
  `tests/integration/test_ulsan_244_*.c`

## Frozen Interfaces

Preserve existing button and replay semantics and checkpoint pins.
Keep the Race S input transport scoped to its validated full-UI boot.

## Evidence Inputs

Use two matching cold lane traces for observed button, crown and touch
transactions. Record native frame checkpoints and the exact input schedule.

## Implementation

Route ordinary host input through the existing machine input API, GPIO,
IOM and DMA paths. Admit only observed transaction shapes and validate an
entire request before consuming it. Publish the firmware's own frames.

In the full UI SDL session, Enter presses the crown, the wheel or Up/Down
turns it, a click or drag uses touch, and Page Up and Page Down press the
upper and lower buttons.

## Tests and Commands

- `make test TEST_FILTER=ulsan` with the private Ulsan manifest and eMMC paths
- `make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_` with the private inputs
- `make check-task-contracts && make check-lines && make check && make sanitize && make check-sdl`

## Acceptance

Reproduce a native setup walk through the documented pairing boundary.
Verify successful and refused crown and touch operations and ordered native
screen transitions in two cold standalone runs. The original Hello and
Service Mode gates pass.

## Forbidden Scope

Guest patches, invented input reports, synthetic frames, generic peripheral
fallbacks, pairing bypass, BLE transport, tracked firmware or frame pixels.

## Handoff

The SDL adapter sends crown and touch through the machine input API to the
PAT9126 on IOM6/GPIO104 and the touch controller on IOM3/GPIO100.
The renderer accepts the observed A8 transition and vector draws.
See E-ULS-0096 through E-ULS-0099.

Two cold standalone runs reached the pairing guidance bottom at frame
245, 52,538,537,483 virtual ns, and 4,325,812,021 instructions. The
RGB565 SHA-256 was
`d3014e708de945e59f5a41e226fd89156bb9ba9467c676ed4eef634229346ae4`.
The original Hello and Service Mode gates also passed twice with unchanged
pins. The Race S unit and private firmware commands, task contracts, line
review, full checks, sanitizers, and SDL checks passed.

At the first pairing guidance screen, swipe up and then turn the crown
clockwise. The supported walk ends at the bottom of the guidance; BLE
pairing is outside this ticket. E-ULS-0099 records a separate crown-only
sequence that can stall the native RTOS list walk. Birth year, weight,
and height use standalone frame pins; the reference layout differs at
those screens.
