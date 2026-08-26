# 670 — Sapporo 2.22 Onboarding Completion via Manual Time Entry

**Status:** in-progress
**Phase:** 6
**Dependencies:** 665

## Goal

Complete the Sapporo `2.22.60` onboarding to `main` in the standalone no-device
emulator by driving the firmware's own manual time-entry chain, so a fresh-boot
setup walk ends on a fully provisioned watch face instead of stalling at the
"Time/date" step. The chain is:
`w-tida` ("Time/date", `8362b9bc`) --MIDDLE--> `w-ltim` ("Searching for GPS")
--LOWER ("SET MANUALLY")--> `w-year` --next--> `w-mont` --next--> `w-day`
--next--> `w-time` --next--> `w-done` ("Done") --> `main`.

This ticket **replaces** the earlier "Phone (OHR2) Time Source" framing opened for
670. A firmware RE of the 2.22.60 build shows that a phone time-delivery command
does **not** exist in this firmware: OHR2 in this build is the PPG/optical-HR
sensor bridge (KISS protocol, ticket 417 scope), not a phone channel; there is no
`TimeSync`/`PhoneTime`/`setSystemTime` command, and the UI resource never
`$.put`s `Dev/Time/LocalTime`. The only writers of the clock are the
`saveTimeComponent` JS-bridge native (manual entry) and `GpsTimeSynchronizer`
(GPS NMEA time). Building a synthetic phone time source would therefore be
inventing guest-visible behavior with no firmware evidence, which the repo
contract forbids. The faithful path is to drive the onboarding's own manual
entry, which needs no new device behavior at all.

## Execution Budget

One to two model-days: a bounded opt-in time-scheduled input driver for the
setup-walk, the deterministic completion sequence, a regression check, and the
corrected evidence entry. No new guest-visible device behavior.

## Required Reading

- `docs/migration-evidence.md`: `E-SAP-ONBOARD-EMU-008`,
  `E-SAP-ONBOARD-EMU-009` (corrected), `E-SAP-COMPAT-GPS-005`.
- `plans/tasks/665-sapporo-time-sync.md` (onboarding screen map, time-source RE;
  its "phone-gated boundary" finding is superseded by this ticket's corrected
  evidence: the walk reached `w-ltim` all along; the UI had advanced past
  `w-tida`, but the `w-ltim` ring animates continuously so the frame-stepped walk
  never re-detected a settled step).
- `$FIRMWARE_ROOT/artifacts/analysis/sapporo-2.22.60/component-05-type-1-v3.raw`
  (onboarding views: `w-tida` @ `0x8bb61d`, `w-ltim` @ `0x892300`, `w-year` @
  `0x81be1d`, `w-mont` @ `0x69021d`, `w-day` @ `0x89701d`, `w-time` @
  `0x127f3d`, `w-done` @ `0x288e1d`, `startup` @ `0x81c600`) and
  `component-04-type-4-v2.raw` (negative phone-time RE; `saveTimeComponent` @
  `0x12b648`, `GpsTimeSynchronizer` @ `0x1092a4`, `settings/time` @ `0x5ea0c`).

## Current Baseline

- The setup-walk reaches `w-tida` "Time/date" (`8362b9bc`, step 21, ~23 s virtual)
  from a fresh boot. MIDDLE on it opens `w-ltim` (verified by PPM frames: the
  "Searching for GPS" ring renders immediately after the press). The walk then
  appears to stop: `w-ltim`'s ring animates every ~1-4 ms and never reports a
  settled frame, so the frame-stepped walk cannot act on it. Every unmodified
  run ends byte-stably at `pc=0x0010fbde`, `stop=compat-refused`, when the
  eleven-hit `gps-awake-pulse` budget (E-SAP-COMPAT-GPS-005) is exhausted at
  ~71-73 s virtual.
- `w-ltim` (`0x892300`): `onActivate` puts `Navigation/State=1`, waits 5 s,
  subscribes to `Dev/Time/LocalTime`; `t >= 1646092800` (2022-03-01Z) routes to
  the UTC-offset menu then `w-done`; otherwise `navigate('#seachforutc',1)` shows
  the "Searching for GPS" view whose down pushButton ("SET MANUALLY") runs
  `close('w-ltim');open('w-year')`.
- Verified empirically (host-side PPM captures, no-device layer, setup-walk):
  - MIDDLE on `8362b9bc` → `w-ltim` "Searching for GPS" + "SET MANUALLY".
  - LOWER ("SET MANUALLY", ~30 s virtual) → `w-year` spinner, default 2022.
  - MIDDLE on `w-year` (2023 after one LOWER) → `w-mont` (Jan) → MIDDLE →
    `w-day` (1) → MIDDLE → `w-time` (00:00) → MIDDLE (switch focus) → MIDDLE →
    `w-done` "Done" (settled CRC `578e2601`) → `main` menu face
    (Navigation/Logbook/Media controls, settled CRC `fb8e0155` at ~37.9 s
    virtual), stop `halt` at ~43.8 s virtual (clean idle, no GPS-cap abort).
- The manual chain needs no new device behavior: `saveTimeComponent('year'|
  'month'|'day'|'hour'|'minute'|'local', ...)` is the firmware's own NEMA write
  path; `w-done` sets `/Settings/Ui/FirstUseWizardExecuted=true` and opens
  `main` after 3 s.

## Allowed Files

- `src/frontends/sdl_live_test.{c,h}` and `src/frontends/main_sdl.c` (the
  opt-in time-scheduled press driver `SEMU_SDL_SETUP_WALK_TIMELINE` and the
  bounded completion walk; no new guest-visible behavior).
- `tests/` (regression that drives the onboarding to `main` and asserts the
  final face CRC and stop reason, plus the byte-identical disabled case).
- `docs/migration-evidence.md` (extend `E-SAP-ONBOARD-EMU-009` with the
  corrected boundary and the completion chain; add the phone-time negative-RE
  note).

## Frozen Interfaces

No new public `semu/` API. The `gps-awake-pulse` budget (E-SAP-COMPAT-GPS-005,
eleven hits) is not raised. NEMA service names, the OHR2 transport, and the RTC
are unchanged. The manual entry uses the firmware's existing
`saveTimeComponent` path; no host time is introduced.

## Evidence Inputs

- Onboarding view semantics from `component-05-type-1-v3.raw` (listed in
  Required Reading): `w-ltim` gate constant `1646092800`, "SET MANUALLY" down
  button, `w-year`/`w-mont`/`w-day`/`w-time`/`w-done`/`main` navigation,
  `startup`'s `FirstUseWizardExecuted` / `LocalTime >= 1646092800` branch.
- Negative phone-time RE of `component-04-type-4-v2.raw` and
  `component-05-type-1-v3.raw`: OHR2 = PPG/HR bridge (no time command); no
  `TimeSync`/`PhoneTime`/`setSystemTime`; no `$.put('/Dev/Time/LocalTime')` in
  the UI. Only `saveTimeComponent` (manual) and `GpsTimeSynchronizer` (GPS)
  write the clock.
- Empirical PPM captures (host-side, never in-repo) of the settled frames proving
  the chain reaches `main` in the standalone no-device context.

## Implementation

1. Add the opt-in `SEMU_SDL_SETUP_WALK_TIMELINE` time-scheduled press driver to
   the setup-walk (done this session): a bounded (64-entry) list of
   `virtual_ms:button` entries parsed in non-decreasing order, fired at their
   exact virtual time regardless of the 350 ms settle window, so animating
   screens (the `w-ltim` ring) can be driven. Off by default; byte-identical
   behavior when unset.
2. Define the deterministic completion sequence: POST handoff to `w-tida`
   (`mlllmlllmm`), MIDDLE into `w-ltim`, timeline LOWER at ~30 s for "SET
   MANUALLY", then MIDDLE confirms on `w-year`/`w-mont`/`w-day`/`w-time`
   (twice) to reach `w-done` -> `main` within the eleven-hit
   `gps-awake-pulse` budget.
3. Record the settled CRCs for `w-year`/`w-mont`/`w-day`/`w-time`/`w-done`/`main`
   and the stop reason in `E-SAP-ONBOARD-EMU-009`.

## Tests and Commands

- `make check` (full), `make check-sdl`, and `make check-lines` pass.
- Reproduce the completion:
  `SDL_VIDEODRIVER=dummy SEMU_SDL_PPM_DIR=<dir> SEMU_SDL_LIVE_TEST=setup-walk SEMU_SDL_SETUP_WALK_POST=mlllmlllmmlmmmmm SEMU_SDL_SETUP_WALK_TIMELINE=30000:l build/suunto-emu-sdl run --profile sapporo-2.22.60 --firmware tests/private/sapporo-2.22.60/firmware.semu --layer sapporo-2.22-no-device --until setup-next --max-instructions 40000000000 --max-time 300000000000`
  Expect deterministic settle past `8362b9bc` into `w-year`/`w-mont`/`w-day`/
  `w-time`/`w-done` (`578e2601`) and the `main` menu face (`fb8e0155`), stop
  `halt` at ~43.8 s virtual.
- Refusal / baseline case: with the timeline driver and POST extensions disabled
  (default walk), the run must remain byte-identical to the
  `E-SAP-ONBOARD-EMU-009` baseline (last settled frame `8362b9bc`; abort at
  `pc=0x0010fbde`, `stop=compat-refused`, eleven-hit GPS budget, ~71-73 s).

## Acceptance

- A fresh-boot walk with the completion driver deterministically reaches the
  `main` face within the existing `gps-awake-pulse` budget.
- With the driver disabled, behavior is byte-identical to the
  `E-SAP-ONBOARD-EMU-009` baseline.
- No golden/checkpoint weakened; `make check`, `make check-sdl`, and
  `make check-task-contracts` pass.

## Forbidden Scope

- No raising of the `gps-awake-pulse` budget; no new NEMA/CTIMER opcodes.
- No invented phone/OHR2 time command (none exists in 2.22.60); no host-time
  input (`clock_gettime`/`gettimeofday`/`time()`) into the guest clock; the only
  time written is what the firmware's own manual-entry `saveTimeComponent` path
  stores.
- No copying the 2.39 `TimeProvider` publish as 2.22.60 authorization.
- No firmware/resource bytes copied into the repository.

## Handoff

Progress (this session):
- `SEMU_SDL_SETUP_WALK_TIMELINE` driver implemented in
  `sdl_live_test.{c,h}` + `main_sdl.c`; parser moved into
  `semu_sdl_live_test_set_timeline()` so `main_sdl.c` stays under 500 lines.
- Deterministic completion sequence verified twice, byte-identical
  `frames.log` between runs: POST `mlllmlllmmlmmmmm` + TIMELINE `30000:l`.
- Settled CRCs and stop reason recorded in E-SAP-ONBOARD-EMU-010(c).
- Disabled-case regression verified byte-identical to the 009 baseline
  (same `frames.log`, stop `pc=0x0010fbde`, `stop=compat-refused`, ~71 s).
- Evidence E-SAP-ONBOARD-EMU-010 added; 009, 665 and current-status.md
  corrected (the "phone-gated Time/date" conclusion is superseded).

Remaining before this ticket can be closed:
- A committed regression for the completion walk (a new script under
  `tools/` or an extension of `test_sdl_live_input.sh`); note the existing
  live-input pins are stale due to manifest drift, so re-pinning there is a
  separate golden change.
- Optionally, a sequence variant that writes a post-gate time
  (year >= 2023 or month >= Mar 2022) so the next boot routes to `main`
  instead of `n-sync-rec`.
