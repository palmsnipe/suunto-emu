# 665 — Sapporo 2.22 Time / Onboarding Trace

**Status:** done
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

## Session findings (2026-08-26, recorded in E-SAP-ONBOARD-EMU-009)

- Fresh-boot screen map (corrected pin mapping): steps 1–11 advance on
  MIDDLE; step 12 is "Connect with mobile" (`261712ad`), which advances only
  on MIDDLE from a fresh boot to the handoff (`ea3bc5f8`); three LOWER presses
  settle `9b58f243`, `eb868d29`, `ed7eeb7a` ("Watch info / Later"); MIDDLE on
  `ed7eeb7a` settles a phone-pairing recommendation sequence `74a5e6ab`,
  `b26dd658`, `0b93f6c9` ("Connect / Connect Later"), `a797ec30`, and MIDDLE
  on `a797ec30` settles `8362b9bc` "Time/date" (`w-tida`, step 21, ~23.0 s
  virtual). On `8362b9bc` MIDDLE/LOWER/UPPER all fail to settle a new frame
  under the walk's one-press-per-frame model; this was confirmed with the new
  opt-in `SEMU_SDL_SETUP_WALK_REPEAT` bounded same-frame re-press helper
  (repeat-off `reg` vs. repeat-on `rep`/`repU`/`repL`, up to 12 re-presses at
  400 ms virtual spacing): all three runs settle byte-identical through step
  21. The `w-tida` resource shows the "Time/date" popup is `#vs-h` index 1
  (the only non-empty `selected` div), where a single `onTap` should reach
  `targetData==2` and open `w-ltim`/`w-year` - which never happens under that
  model. The original conclusion that `8362b9bc` is a phone-gated handoff was
  subsequently CORRECTED by E-SAP-ONBOARD-EMU-010: MIDDLE does open `w-ltim`;
  the walk only failed to observe it because the `w-ltim` "Searching for GPS"
  ring animates continuously and never reports a settled frame, so the
  one-press-per-settled-frame model cannot act on it.
- Correction to 008 (superseded in part by E-SAP-ONBOARD-EMU-010): the
  furthest *settled* screen under the plain walk is "Time/date" (`w-tida`,
  `8362b9bc`); the `w-ltim` "Search for GPS" screen is reached by MIDDLE but
  is never observed by the plain walk. With the time-scheduled
  `SEMU_SDL_SETUP_WALK_TIMELINE` driver, onboarding completes standalone to
  `main` via the firmware's own manual-entry chain.
- Every run (RTC-value gated or not) stops byte-stably at
  `pc=0x0010fbde`, `stop=compat-refused`, 11-hit `gps-awake-pulse` budget at
  virtual time 73.08–73.15 s.
- Time-source RE (2.22.60 application binary + resources): `w-ltim` puts
  `Navigation/State=1`, waits 5 s, subscribes to `Dev/Time/LocalTime`;
  `t >= 1646092800` (2022-03-01Z, unix seconds) → UTC-offset menu → `w-done`;
  otherwise "Search for GPS" whose down button ("Set manually") opens
  `w-year`. The authoritative application clock path is the
  `GpsTimeSynchronizer` worker (NMEA GGA/RMC time-of-day; "setting new utc
  time %u"); no OHR2/phone time-sync command exists in 2.22.60. The manual
  chain `w-year` → `w-mont` → `w-day` → `w-time` → `w-tset`/`w-done`
  (auto-opens `main` after 3 s) is fully present in the resources.
- Empirical NMEA-time boundary: injecting a valid time-bearing GGA+RMC+EPU
  group on the `@GSR` running-status exchange (with or without coordinates)
  makes the firmware issue repeated deliberate `SYSRESETREQ` writes from
  `0x000be93e` and does not stop the GPS power-cycling. Synthetic NMEA time
  is therefore not an authorized `LocalTime` source in the standalone
  no-device context (experiment code reverted; tree clean).
- Empirical RTC boundary: returning `1704110400` from `0x40004820` (plus
  time-set bit at `0x40004800`) leaves the onboarding byte-identical — the
  RTC value register does not feed `Dev/Time/LocalTime` in the standalone
  context (experiment code reverted; tree clean).

## Handoff

Delivered: the pin-mapping root cause and fix, the extended bounded walk, the
PPM diagnostic, evidence E-SAP-ONBOARD-EMU-008, and the mapping regression
test (all in commit 895ef29). 2026-08-26 session added evidence
E-SAP-ONBOARD-EMU-009 (corrected screen map to "Time/date", time-source RE,
NMEA-time/RTC-value boundaries) plus a new opt-in
`SEMU_SDL_SETUP_WALK_REPEAT` bounded same-frame re-press helper
(`sdl_live_test.{c,h}` + `main_sdl.c`; off by default, walk stays
byte-identical). CORRECTION (E-SAP-ONBOARD-EMU-010): `8362b9bc` "Time/date"
is not phone-gated - MIDDLE opens `w-ltim`, and onboarding completes
standalone to `main` through the firmware's own manual-entry chain driven by
the new time-scheduled `SEMU_SDL_SETUP_WALK_TIMELINE` walk helper (ticket
670); the earlier "phone-gated / inert buttons" reading was an artifact of
the `w-ltim` ring animating without ever settling a frame. The negative
time-source RE stands: 2.22.60 has no OHR2/phone time-delivery command, and
synthetic NMEA time resets the guest, so manual entry is the only authorized
standalone time source. Keep the ticket `in-progress`.
