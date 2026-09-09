# 771 — Sapporo 2.39 Optional Five-Pulse GPS Integration

**Status:** done
**Phase:** 7
**Dependencies:** 759,768,769

## Goal

Expose the measured five-pulse fixture as a separately selected alternative,
preserving the existing four-pulse layer and all its historical gates.

## Execution Budget

One model-day for integration, lifecycle/refusal tests and authentic checks.

## Required Reading

`docs/{architecture,execution-model,testing-strategy,compatibility-policy}.md`;
ticket 769; E-SAP-COMPAT-GPS-AWAKE-239-001, E-SAP-GPS-FIFTH-239-002;
all existing Allowed Files; `src/boards/{machine_run.c,machine_snapshot_layers.c}`;
`src/devices/{sapporo_cxd5610.c,sapporo_cxd5610.h,sapporo_devices_internal.h}`;
`include/semu/{machine,compat,manifest,trace}.h`;
`tests/integration/sapporo_239_personal_probe.c`, `src/frontends/cli_snapshot.c`.

## Current Baseline

All dependencies are done. `15c2a53` records one additional native pulse and
HEIGHT navigation with the otherwise unchanged production library. Only
`sapporo-2.39-gps-awake` (four pulses) is currently registered.

## Allowed Files

- `src/compat/sapporo_239_gps_awake.{c,h}` (shared descriptor/validation/binding)
- `src/devices/sapporo_device_compat.c` (alternative owner acceptance)
- `src/boards/machine.c` (layer registry only)
- `profiles/sapporo/2.39.20/profile.semu` (append optional layer declaration only)
- `tests/unit/test_sapporo_profile_239.c`
- `tests/unit/test_sapporo_239_gps.c`, `tests/unit/test_sapporo_239_gps_reopen.c`
  (profile discovery count only; existing lifecycle assertions unchanged)
- `tests/unit/test_sapporo_239_gps_awake.c`
- `tests/unit/test_sapporo_239_gps_awake_snapshot.c`
- `tests/unit/test_sapporo_239_gps_awake_five.c` (new)
- `tests/unit/test_sapporo_239_gps_awake_five_snapshot.c` (new)
- `tests/integration/sapporo_239_five_probe.c` (new)
- `tests/integration/test_firmware_sapporo_239_gps_five.sh` (new)
- This ticket, `docs/current-status.md`, `docs/migration-evidence.md`
- External bounded production observers/comparisons; no private artifacts in Git.

## Frozen Interfaces

Use `sapporo-2.39-gps-awake-five` instead of, never alongside, the existing
awake layer. Require the same explicit startup/reopen dependencies and hashes.
Five total pulses, 100-ms delay, 1-ms width; all driver predicates unchanged.
Old identity, four-hit limit, event logs, snapshots and refusals remain exact.
New snapshots retain their distinct layer ID; cross-identity loads refuse.
No new public API, runtime dependency, Makefile or snapshot format change.

## Evidence Inputs

E-SAP-GPS-FIFTH-239-002 supplies the complete measured native sequence:
idle sixth-admission refusal `001291cc / 4071207676 / 37929735196`;
MIDDLE at `3991602893 / 34088644931` and release
`3995360381 / 34174452535` reaches HEIGHT CRC `cd4c0a99`, then
`000920b4 / 4232903136 / 34413596174` personal-file refusal.
All component/full-flash and intermediate hashes remain in that entry.

## Implementation

Start with a failing optional-layer discovery test. Share the existing pulse
implementation with exact identity-selected bounds; do not trust arbitrary
descriptor budgets. Bind instance-owned counters and reject simultaneous awake
variants before mutation. Cover disabled/wrong-hash/state/budget/scheduler
refusals, instance isolation, reset and pending/high/completed restore.
New production native checkpoints must match diagnostic state except for the
explicit serialized layer ID (test-only normalization, never runtime migration).
Generate new-layer input checkpoints by native execution from cold boot; do
not relabel a snapshot to execute it. Repeat idle/input continuations and phase
restores. Pin final hashes under the new identity and keep every legacy gate.

## Tests and Commands

`make test TEST_FILTER=sapporo_239_gps_awake`;
`make test TEST_FILTER=sapporo_profile_239`;
`make test TEST_FILTER=machine_snapshot`;
`make check-lines`, `make check-task-contracts`, `make check`, `make sanitize`,
`make sdl`, `git diff --check`.

```sh
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_five
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_awake
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_personal_budget
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_live_input.sh
```

## Acceptance

Exact opt-in alternative, atomic duplicate/cross-identity refusals, five native
pulses only, repeated/resumed equivalence and unchanged legacy gates. All
private inputs must be validated before execution; skipped evidence cannot
complete acceptance. Leave status unchanged during implementation.

## Forbidden Scope

No sixth pulse, automatic fixture selection, live-state/counter repair,
snapshot migration, file-budget increase, GPS fix/time/response, firmware
patch, generic heartbeat, physical timing claim or weakened legacy golden.

## Handoff

2026-09-08 implementation: optional five-pulse integration and all acceptance
checks pass locally, including private evidence. Status remains `ready` pending
separate integrator review; implementation changes are uncommitted.

Changed files: the five runtime/header/registry/profile paths in Allowed Files;
the seven unit-test files there; the new private probe and shell gate; this
ticket and both status/evidence documents. Separate planning maintenance
accepted ticket 769 and registered this ticket in `plans/index.tsv` before
implementation. No public API, Makefile, snapshot format or integration change
is requested.

Evidence: E-SAP-0011, E-SAP-COMPAT-GPS-AWAKE-239-001 and
E-SAP-GPS-FIFTH-239-002 (production verification appended to the latter).
The narrow optional-profile discovery regression fails before implementation
(five cases, one failure); afterward all five pass. The original startup/reopen
tests change only the discovered profile-layer count from four to five; their
selected-layer lifecycle and legacy assertions are preserved.

Every command in Tests and Commands above passed: awake filter 11 cases,
profile filter five, snapshot filter four; full normal and sanitizer suites
836 each; 138 task contracts; line check (existing warnings only), SDL build
and diff whitespace check. The three authentic 2.39 gates and 2.22.60 SDL
live-input command ran with private evidence present, without skips. Live input
preserves instruction 774081920 / time 6520939902. The new gate validates cold
startup, repeated idle/MIDDLE branches, four phase restores, exact whole-state
comparison and input/hash/ownership refusal, retaining the immutable flash hash.

Additional authentic sanitizer command (also passed):

```sh
cc -std=c99 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc -Isrc/devices tests/integration/sapporo_239_five_probe.c build/sanitize/libsemu.a -o /tmp/semu-771.BtFo6c/probe-sanitize
/tmp/semu-771.BtFo6c/probe-sanitize tests/private/sapporo-2.39.20.22297/firmware.semu /tmp/sapporo-239-full-flash-exact.bin /tmp/semu-771.BtFo6c/cold.prefix.sems 6e67094057d9510874a3766ced6372066fc134c1014840a0908a990266afa6de /tmp/semu-771.BtFo6c/sanitize-middle middle > /tmp/semu-771.BtFo6c/sanitize-middle.trace 2> /tmp/semu-771.BtFo6c/sanitize-middle.log
```

`cmp` of its trace, log and final snapshot against the normal production MIDDLE
run passes. New-identity idle final SHA-256 is
`d5244833987e6801192af15f0c57077eba2d23329fbaa4e09a6ccc4ec5b455c1`;
MIDDLE final `2224b55ed72f0cac548fa80117787ae5b049f5783f10a8b6730f5ea1a0936467`.
All intermediate pins and exact native count/time tuples are in the ledger.
The comparison-only normalized copies match ticket 769; they are never used
for execution. Genuine new-identity snapshots are generated from cold boot.

Next work: measure the complete personal-settings save suffix exposed by
WEIGHT → HEIGHT before any file-budget change. Physical GPS cadence/fix/time,
sixth pulse, later setup screens, watch face and post-setup menus remain gaps.

## Integrator Acceptance

The 2026-09-09 separate integrator review accepts ticket 771 and updates only
this ticket, `plans/index.tsv`, and `docs/current-status.md` in the review.
Implementation changes are committed as `0826523`. Source review confirms
identity-selected bounds with the budget fixed by the static descriptor rather
than caller state, one shared awake binding slot refusing variant duplication
before mutation, cross-identity snapshot refusals in both directions with
byte-identical re-saves, and unchanged four-pulse lifecycle predicates, logs,
limits and legacy gates.

Every command in Tests and Commands was rerun with private evidence present:
awake filter 11 cases, profile filter 5, snapshot filter 4; `make check` and
`make sanitize` 836 cases each; 138 task contracts; line check with existing
warnings only; SDL build; `git diff --check`. The authentic 2.22.60 live-input
gate passed with the exact E-SAP-ONBOARD-EMU-012 stop
`pc=0x000bacf4, 774081920 instructions / 6520939902 ns`. All three exact 2.39
gates passed: the new five-pulse repeat/resume/refusal gate, the legacy
four-pulse awake gate, and the personal/time save gate. No golden, budget, or
fixture file was changed by the review.

The synthetic 32 MiB full-flash fixture had been removed from `/tmp` with its
recipe undocumented. The review recovered it and rebuilt the image
byte-identically to the pinned hash
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` from
read-only inputs only: the exact 2.39 component-05 fragment
(SHA-256-pinned by E-SAP-0011) FF-padded to 16 MiB; the pre-existing synthetic
manufacturing sector (SHA-256
`c08816067aed620fb8c3a074f5f0e3a8ceb398416d6f9c33d1f6c13df5619a53`, reproduced
by `$FIRMWARE_ROOT/tools/build_production_data_fixture.py` from the exact
2.22.60 application) patched at device address `0x00FFF000`; the upper 16 MiB
all `0xFF`. External `shasum -a 256` of the rebuilt file equals the pin. The
fixture remains external, hash-gated, and opt-in; no firmware or proprietary
bytes entered Git.
