# 768 — Sapporo 2.39 Native Time Settings Routing

**Status:** done
**Phase:** 7
**Dependencies:** 729,759,767

## Goal

Let the exact native `settings/time` create open execute through firmware
storage, preserving the measured bytes and the next GPS refusal.

## Execution Budget

One model-day for a narrow routing change, regressions and private gates.

## Required Reading

`docs/{architecture,execution-model,testing-strategy,compatibility-policy}.md`;
ticket 767; E-SAP-TIME-NATIVE-239-001 and E-SAP-COMPAT-PERSONAL-239-001;
all existing Allowed Files; `tests/unit/test_sapporo_239_quiet_read.c`;
`include/semu/{machine,compat,bus}.h`, `src/compat/sapporo_239_files.h`,
`src/compat/sapporo_239_files.c`, `src/compat/sapporo_239.c`,
`src/boards/machine_snapshot_layers.c`, `src/frontends/cli_snapshot.c`.
Use ticket 767's pinned external probes for read-only production comparisons.

## Current Baseline

All dependencies are done. Production `bca8f7d` refuses the native time create;
ticket 767 shows that exact forwarding retains both 349-byte native saves
without consuming any additional file hits or requiring a file slot.

## Allowed Files

- `src/compat/sapporo_239_file_hook.c` (exact native route only)
- `tests/unit/test_sapporo_239_time_native.c` (new)
- `tests/unit/test_sapporo_239_personal_budget.c` (unknown-path fixture only)
- `tests/integration/sapporo_239_personal_probe.c`
- `tests/integration/test_firmware_sapporo_239_personal_budget.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`, this ticket
- External temporary read-only production observers, never firmware artifacts
  in Git; any follow-on GPS finding is evidence only.

## Frozen Interfaces

Forward only normalized exact `settings/time` in mode two after existing layer
and path validation. Routing must leave PC/registers/RAM/files/handles/counters/
logs unchanged; native CPU execution owns all subsequent effects. Preserve
all twelve file slots/capacities, the 76,599/76,602 ceilings, public headers,
Makefile, profiles, registry, formats, rendering and four-pulse GPS bound.

## Evidence Inputs

E-SAP-TIME-NATIVE-239-001 pins native final
`ac0a32899f57d7b84733f458d4bc2b246d005b2885554686d20e157b5674193d`,
midpoint `b85eed95839285b520bb560cd1fff13431b837c59b29b60f5d6a12d8b86b58f2`,
and final bytes SHA-256 `7c471060d2e4e4c8f8c3258d328de60564a60164b96c4b96daa284e2c81b108d`
at flash offset `0x00a91a00`. No fabricated clock or serializer is needed.

## Implementation

Start with a failing-before exact-route test. Check zero-state-change routing
at zero and exhausted file budget; malformed/sibling paths, invalid modes,
disabled/wrong-hash layers still refuse. Existing synthetic-owned files still
use their original adapter and consume hits. Change the personal-budget unit's
now-supported unknown-path example to an unobserved sibling, retaining all
write/limit/refusal assertions.

Extend the existing private full personal branch through native time persistence
to the accepted GPS endpoint. This explicitly supersedes the former time-path
refusal assertion; retain its earlier general/personal midpoint and log/count
pins, and preserve the shorter idle branch verbatim. Add native-midpoint restore
and require exact ticket-767 final state; no experimental 13-slot state is used.
Compare production native storage bytes using the pinned external observer and
check former refusal snapshots still load without mutation. All component,
flash and starting-checkpoint validation must precede execution.

## Tests and Commands

`make test TEST_FILTER=sapporo_239_time_native` before/after;
`make test TEST_FILTER=sapporo_239`, `make test TEST_FILTER=machine_snapshot`,
`make check-lines`, `make check-task-contracts`, `make check`, `make sanitize`,
`make sdl`, `git diff --check`.

```sh
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_personal_budget
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_general_budget
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_activity_budget
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_awake
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_live_input.sh
```

## Acceptance

Failing-before/passing-after routing regression, unchanged atomic refusals,
native repeated/resumed file bytes and machine state, exact historical prefix/
idle gates and unchanged source flash. Missing private evidence precludes
completion. Leave status/index unchanged during implementation.

## Forbidden Scope

No new file slot/capacity, budget, snapshot format, public API, synthetic return,
GPS response/pulse, fabricated clock, unknown-create wildcard, firmware patch,
renderer change, dependency or release-golden weakening. No post-setup claim.

## Handoff

2026-09-08: implemented and verified; status remains `ready` for separate
integrator review. Changed the existing file hook, added
`tests/unit/test_sapporo_239_time_native.c`, changed only the unknown-path
fixture in the personal-budget unit, extended the existing personal probe and
shell gate, and updated the two allowed documentation files and this handoff.
The separate preceding planning review accepted 767 and created this ticket;
pre-existing 766/767 worktree changes are preserved. No commit made this turn.

Evidence: E-SAP-TIME-NATIVE-239-001, E-SAP-TIME-SCHEMA-239-001 and existing
personal/general pins. Read-only follow-on E-SAP-GPS-FIFTH-239-001 identifies
the next exhausted fixture, without changing GPS behavior.

All commands above passed. The new exact-route regression failed before the
runtime change (three forwarding assertions), then passed; focused suite
47/47, machine snapshot 4/4, normal and sanitizer suites 832/832 each.
Line checks pass with existing review-threshold warnings; task contracts
validate 136 tickets. `sh -n tests/integration/test_firmware_sapporo_239_personal_budget.sh`
also passes. All five listed private commands ran with available, validated
inputs and passed, including repeated/resumed native time, unchanged idle and
earlier gates, atomic refusal retries and wrong-input refusals.

Production-only external verification commands (artifacts outside Git):

```sh
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/compat -Isrc/devices /tmp/semu-767-time.PaLEy5/probe.c build/libsemu.a -o /tmp/semu-768.42dyOp/production-probe
TIME_EXPECTED_PAYLOAD=/tmp/semu-767-time.PaLEy5/time-payload.bin /tmp/semu-768.42dyOp/production-probe /tmp/semu-768.42dyOp/native-a /tmp/semu-766.FStvr2/first.final.sems 3d19f80df2c47a1e98f6bfa8dd0640f0d6cbb38a4d06f933d16f09c2c54245c6
TIME_EXPECTED_PAYLOAD=/tmp/semu-767-time.PaLEy5/time-payload.bin /tmp/semu-768.42dyOp/production-probe /tmp/semu-768.42dyOp/native-b /tmp/semu-766.FStvr2/first.final.sems 3d19f80df2c47a1e98f6bfa8dd0640f0d6cbb38a4d06f933d16f09c2c54245c6
cmp /tmp/semu-768.42dyOp/native-a.loaded.sems /tmp/semu-766.FStvr2/first.final.sems
cmp /tmp/semu-768.42dyOp/native-a.final.sems /tmp/semu-768.42dyOp/native-b.final.sems
cmp /tmp/semu-768.42dyOp/native-a.final.sems /tmp/semu-767-time.PaLEy5/native-full-a.final.sems
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/compat -Isrc/devices /tmp/semu-768.42dyOp/gps-inspect.c build/libsemu.a -o /tmp/semu-768.42dyOp/gps-inspect
/tmp/semu-768.42dyOp/gps-inspect /tmp/semu-768.42dyOp/gps-state /tmp/semu-767-time.PaLEy5/native-full-a.final.sems ac0a32899f57d7b84733f458d4bc2b246d005b2885554686d20e157b5674193d
cmp /tmp/semu-768.42dyOp/gps-state.final.sems /tmp/semu-767-time.PaLEy5/native-full-a.final.sems
shasum -a 256 /tmp/sapporo-239-full-flash-exact.bin
```

Native midpoint/final and retained payload exactly match the Evidence Inputs;
source flash remains `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
Final PC/count/time: `001291cc / 3960123530 / 32455738919`; logical hits
76,599, GPS `2,2,4`, WEIGHT CRC `a8c9f3d3`. Native saves consume no synthetic
hit or slot. Source/trace hashes are recorded in the ledger.

Remaining gap: recover evidence for the fifth GPS-awake lifecycle, then
continue setup/watch-face/menu work. No fifth pulse or physical cadence is
inferred from budget exhaustion. No public-interface integration is requested;
acceptance/status review is the only current integrator-owned action.

### Integrator acceptance, 2026-09-08

Separate planning review accepts implementation commit `44ee4d6`. Reviewed
the exact-route delta, immutable routing/refusal regression, native checkpoint
extension and recorded normal/sanitizer/private results. The focused routing
test passes again and 136 task contracts validate before this planning change.
All prior acceptance inputs were available; no condition was skipped. The next
evidence ticket is 769. No GPS or other runtime change accompanies acceptance.
