# 665 — Sapporo 2.22 Time / Onboarding Trace

**Status:** in-progress
**Phase:** 6
**Dependencies:** 515, 516

## Goal

Drive the Sapporo 2.22.60 onboarding flow past the `Continue the setup on
your phone` handoff (`w-conn-1`) to the next observable onboarding screen, and
record the evidence for where the walk genuinely stops. This ticket diagnoses
why the walk previously stalled at the handoff, lands the minimal
evidence-backed fix, and pins the result. The remaining step — actually
advancing past the time-zone screen to `w-done`/`main` — is a separate,
evidence-gated effort (see Below).

## Execution Budget

One to two model-days. This ticket's code work is a small board-boundary fix
plus one new SDL diagnostic helper and one focused regression test.

## Required Reading

- `docs/migration-evidence.md`: `E-SAP-BUTTONS-001`,
  `E-SAP-ONBOARD-EMU-006`, `E-SAP-ONBOARD-EMU-007`,
  `E-SAP-ONBOARD-EMU-008`, and `E-SAP-COMPAT-GPS-005`.
- `docs/research/native-live-ui-navigation.md` (spatial/semantic button
  correspondence).
- `$FIRMWARE_ROOT/artifacts/analysis/sapporo-2.22.60/component-05-type-1-v3.raw`
  (type-1 `xz` resource containing the onboarding view definitions).
- `plans/tasks/516-sdl-semantic-input.md` and the `sdl_live_test` walk.

## Current Baseline

- The SDL setup-walk stalled at the `Continue the setup on your phone`
  handoff (gen `1295`, CRC32 `ea3bc5f8`, E-SAP-ONBOARD-EMU-006).
- E-SAP-ONBOARD-EMU-007 recorded the "inert button" symptom: pressed buttons
  reached the guest `INPUT_READ1` poll but the guest submitted zero NEMA
  command lists; it attributed the stall to the standalone 2.22.60 build
  lacking post-handoff view definitions.

## Allowed Files

- `src/boards/machine.c` (semantic-to-physical button pin mapping only).
- `src/frontends/sdl_live_test.{c,h}`, `src/frontends/main_sdl.c`,
  `src/frontends/sdl_ppm_dump.c` (new, diagnostic-only PPM dump).
- `tests/unit/test_machine.c` (button-mapping regression test).
- `docs/current-status.md`, `docs/migration-evidence.md`.

## Frozen Interfaces

- No new public `semu/` API. `semu_machine_input`, the semantic button enum,
  `semu_apollo4_set_gpio_input`/`_get_gpio_input`, and the NEMA/panel
  contracts are unchanged.
- The `gps-awake-pulse` compatibility budget (E-SAP-COMPAT-GPS-005, eleven
  hits) is not modified by this ticket.

## Evidence Inputs

- `E-SAP-BUTTONS-001`: GPIO57/58/59 = upper/middle/lower.
- `E-SAP-ONBOARD-EMU-008`: the corrected mapping advances onboarding; the
  post-handoff view definitions DO exist in the 2.22.60 resource; the `w-ltim`
  "Search for GPS" screen waits on a `Dev/Time/LocalTime` publish.
- Missing evidence: a native 2.22.60 onboarding time-sync capture (or a
  researcher-owned full-flash dump exposing the time source). The 2.39
  `sapporo-2.39-wfa-atlas-lifecycle.md` time-sync trace is a different
  firmware and does NOT authorize a 2.22.60 time provider.

## Implementation

- `machine.c`: correct the semantic-to-GPIO table to
  `upper=GPIO57 / middle=GPIO58 / lower=GPIO59` (was inverted upper/lower).
  This is the root cause of the 007 "inert button" symptom: the walk's lower
  (Skip) press was driving the physical back pin.
- `sdl_live_test.{c,h}` + `main_sdl.c`: optional
  `SEMU_SDL_SETUP_WALK_POST=<≤32 chars of u/m/l>` drives a bounded post-handoff
  sequence; `sdl_ppm_dump.c` (gated by `SEMU_SDL_PPM_DIR`) writes one PPM per
  distinct settled frame for inspection. Default behavior is unchanged.
- Add `test_semantic_button_pin_mapping` pinning UPPER→57, MIDDLE→58,
  LOWER→59 through `semu_machine_input` + `semu_apollo4_get_gpio_input`.

## Tests and Commands

- `make check` (full; `test_machine` now includes
  `test_semantic_button_pin_mapping`).
- `make check-sdl` (SDL3 present).
- Reproduce the walk: build SDL, run with
  `SEMU_SDL_LIVE_TEST=setup-walk SEMU_SDL_SETUP_WALK_POST=llll…`
  (`SEMU_SDL_PPM_DIR` optional) from the pre-frame snapshot; expect three new
  settled frames (gen 1395 `9b58f243`, 1492 `eb868d29`, 1592 `ed7eeb7a`)
  stopping deterministically at `pc=0x0010fbde`, 5,280,223,507 instructions,
  `stop=compat-refused`.

## Acceptance

- `make check` and `make check-sdl` pass; `test_semantic_button_pin_mapping`
  pins 57/58/59.
- Two independent post-handoff walks are byte-identical to the CRCs above.
- No golden/checkpoint weakened; the `gps-awake-pulse` budget and NEMA
  contracts are unchanged.

## Forbidden Scope

- No `Dev/Time/LocalTime` publish / TimeProvider model (needs a 2.22.60
  native time-sync trace; see 008).
- No raising of the `gps-awake-pulse` budget, no new NEMA/CTIMER opcodes, no
  screen-specific golden for the post-handoff screens.

## Handoff

Delivered: the pin-mapping root cause and fix, the extended bounded walk, the
PPM diagnostic, evidence E-SAP-ONBOARD-EMU-008, and the mapping regression
test (all in commit 895ef29). Remaining / gated: driving past `w-ltim` to
`w-done`→`main` requires a synthetic `Dev/Time/LocalTime` publish (local
resource `0x2705`, packed `0x2705001f`) plus `gps-awake-pulse` budget
headroom, which needs a 2.22.60 onboarding time-sync native trace before it is
authorized. Keep this ticket `in-progress` until that trace exists.
