# 822 — Ulsan 2.44.52 Crown-Only Pairing Guidance

**Status:** done
**Phase:** 7
**Dependencies:** 821

## Goal

Scroll native Race S pairing guidance to its last reference frame using
only the crown after opening the guidance. Resolve the stalled guest
work recorded in E-ULS-0099.

## Execution Budget

Bound standalone runs to 65 virtual seconds and 8 billion instructions
per boot. Bound each lane run to 100 virtual seconds. Limit finer probes
to the input interval, relevant task and observed interrupt sources.

## Required Reading

Read ticket 821 and E-ULS-0095 through E-ULS-0099, the architecture,
execution, testing and compatibility contracts, and the reference
lane's native UI setup and crown route. Read the existing onboarding,
Hello and Service Mode integration tests, the machine run and input
paths, and the implementation and tests of the subsystem identified
by the divergence trace.

## Current Baseline

Ticket 821 reaches the bottom of the native guidance after a touch
swipe followed by crown input. The crown-only sequence reaches the
first scrolled frame and can then stall with a self-linked guest RTOS
list item. The lane reaches the bottom using only the crown.

## Allowed Files

- This ticket, `plans/index.tsv` for integration, and `docs/migration-evidence.md`
- `tests/integration/test_ulsan_244_hello_frame.c` and
  `tests/integration/test_ulsan_244_onboarding.c`
- `src/boards/machine.c` to bind the observed actuator in the full UI session
- `src/devices/ulsan_iom4.c` and `src/devices/ulsan_iom4.h` for the
  demonstrated actuator DMA discrepancy
- `tests/devices/test_ulsan_iom4_engine.c` for focused coverage

## Frozen Interfaces

Keep existing Hello, Service Mode and pairing guidance reference frame
pins in both input routes. Record any rederived standalone numeric
selector pins and boot checkpoints. Preserve the native input API,
profile identity checks and deterministic timing outside the corrected
behavior.

## Evidence Inputs

Use E-ULS-0099 as the red baseline. Identify the first divergent device
or execution transition and reproduce the attribution twice using the
reference lane or approved hash-pinned offline evidence.

## Implementation

Fix the observed divergence at its source. Preserve the failing
input sequence as a private firmware regression and verify that
a subsequent crown burst continues past the former stall.

## Tests and Commands

- `make test TEST_FILTER=ulsan` with the private Ulsan manifest and eMMC paths
- `make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_` with the private inputs
- `make check-task-contracts && make check-lines && make check && make sanitize`
- `make check-sdl`

## Acceptance

Two cold standalone boots match the ordered native pairing guidance
frames, including the bottom reference, using no touch during the
crown-only walk. The run remains responsive after the former stall
window. The mixed input, Hello and Service Mode gates pass.

## Forbidden Scope

Guest patches, synthetic frames, hidden touch events, input suppression
that avoids the failing sequence, pairing bypass, BLE transport and
tracked firmware or frame pixels.

## Handoff

E-ULS-0100 traces the stall to timed-out IOM4 waveform uploads at
actuator address `0x50`. The firmware later tries to play an effect
with an invalid index, asserts with interrupts masked, and inserts
the same RTOS list item again. The IOM4 model now completes only the
observed waveform writes, register writes, and status read in the
validated Race S full UI session.

Two cold standalone boots use only the crown to scroll through the
1,650 ms idle interval to the same 466×466 RGB565 reference frame.
Both end at generation 265, 40,863,500,172 virtual ns, and
5,766,925,599 instructions with SHA-256
`d3014e708de945e59f5a41e226fd89156bb9ba9467c676ed4eef634229346ae4`.
The mixed route also reaches that frame on two cold boots. Hello and
Service Mode preserve their exact reference hashes. Hello timing and
the three standalone numeric selector pins were rederived in E-ULS-0100.

The private `make test TEST_FILTER=ulsan` passed all 89 cases.
Both opt-in scripts passed under
`make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_`.
`make check-task-contracts`, `make check-lines`, `make check`,
`make sanitize`, and `make check-sdl` also passed.

The verified walk ends at the last pairing guidance screen. BLE
pairing remains outside this ticket.
