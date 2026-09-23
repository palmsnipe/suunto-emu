# 789 — Sapporo 2.22 and 2.35 Post-Branch Gate Re-derivation

**Status:** done
**Phase:** 7
**Dependencies:** 220, 225, 670, 705

## Goal

Attribute and re-derive the 2.22 SDL and 2.35 firmware checkpoints affected by
the F57F Thumb dispatch correction. Restore truthful bounded regressions;
full 2.35 onboarding remains outside this ticket.

## Execution Budget

One to three model-days; eleven existing shell runners and three documentation
files at most. No engine modules or firmware-derived fixtures in Git.

## Required Reading

`README.md`, `docs/current-status.md`, `docs/execution-model.md`,
`docs/testing-strategy.md`, `docs/compatibility-policy.md`, and ledger entries
E-CPU-0002, E-CPU-F57F-001, E-EMU-SAP222-HEAP-001, E-SAP-0043,
E-SAP-0048 and E-NEMA-RGBA4444-001. Read dependency tickets 220, 225, 670,
705; `src/cpu/armv7m/thumb32.c`, `thumb32_system.c`, `thumb32_data.c`;
`include/semu/cpu.h`, `include/semu/machine.h`;
`tests/unit/test_cpu_thumb32_dispatch.c`; every runner listed in Allowed Files.

## Current Baseline

The corrected CPU passes 985 normal checks and 980 sanitizer checks. Paired
2.22 scripted walks finish step 31 and idle through 60 seconds; old onboarding
pins instead expected a fatal halt. The existing live-input runner fails.
Of seven 2.35 era runners, pressure and production pass; block erase, OHR,
GPS startup, reopen and awake fail after the fix. Failures require attribution,
not automatic hash replacement. Existing 2.35 main TSC6A refusal remains open.

## Allowed Files

- `tools/test_sdl_live_input.sh`
- `tools/test_sdl_onboarding_completion.sh`
- `tools/test_sdl_sapporo_235.sh`
- `tools/test_sdl_sapporo_235_scroll.sh`
- `tests/integration/test_firmware_sapporo_235_block_erase.sh`
- `tests/integration/test_firmware_sapporo_235_gps_awake.sh`
- `tests/integration/test_firmware_sapporo_235_gps_reopen.sh`
- `tests/integration/test_firmware_sapporo_235_gps_startup.sh`
- `tests/integration/test_firmware_sapporo_235_ohr.sh`
- `tests/integration/test_firmware_sapporo_235_pressure.sh`
- `tests/integration/test_firmware_sapporo_235_production.sh`
- `README.md`
- `docs/current-status.md`
- `docs/migration-evidence.md`

## Frozen Interfaces

Public headers, profiles, firmware manifests, CLI/layer contracts, SDL input
mapping and trace formats are frozen. No Makefile or registry edits. Report
interface deficiencies to the integrator. Missing private firmware may skip;
a present mismatched component must fail before execution.

## Evidence Inputs

E-CPU-0002 pins ARM DDI 0403E.e A5.3 branch encodings and A7.7 branch/barrier
pseudocode. E-CPU-F57F-001 contains paired lane branch and native increment
results, the corrected 2.22 cold/idle checkpoints and the era failure audit.
Use each existing runner's evidence and hash-pinned manifest. New pins need
two byte-identical bounded observations, raw-log SHA-256 and derived census
in the ledger. Stop and report a new behavior gap if a failure cannot be
attributed to the corrected branch semantics or separately recorded evidence.

## Implementation

1. Preserve each old pin in an evidence before/after table. Run the bounded
   commands twice, distinguishing instruction/time drift from changed outcomes.
2. Pin 2.22 successful navigation and a bounded active idle continuation. A
   fatal loop or assertion is never onboarding completion. Verify fresh
   post-setup button input and frame contents before claiming usable main UI.
3. Audit all seven 2.35 runners and both SDL prefixes. Retain layer hit bounds,
   reset/refusal checks and unsupported later boundaries. A new fault is a
   blocker, not a new passing golden.
4. Update only evidence-supported pins and the corresponding documentation.

## Tests and Commands

- `make sdl`: exit 0 with optional SDL3 installed.
- `SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_live_input.sh`
- `SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_onboarding_completion.sh`
- `make test-firmware TEST_PROFILE=sapporo-2.35.34 TEST_FILTER=sapporo_235 SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu`
- `SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu sh tools/test_sdl_sapporo_235.sh`
- `SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu sh tools/test_sdl_sapporo_235_scroll.sh`
- `make check-lines`, `make check`, `make check-task-contracts`: all exit 0.

Each authentic runner must exit 0 twice with its expected successful prefix
and refusal cases. Preserve explicit instruction/virtual-time caps; cap each
runner at 900 wall seconds. Retain malformed-timeline negative checks.

## Acceptance

Every listed runner passes twice with preserved refusal coverage and paired
censuses. Every changed pin has an attributed before/after evidence row.
2.22 no longer calls a fatal halt completion; visible input after setup is
verified. No engine source, firmware input or old unrelated golden changes.
If private evidence is absent or a new behavior gap appears, report incomplete.

## Forbidden Scope

No CPU/device behavior changes, invented rendering, heap inflation, assertion
bypass, global NOP/read-zero fallbacks, unlimited compatibility fixtures,
firmware bytes/pixels/logs in Git, or claims about 2.39. Tickets 777 and 783
retain 2.39 ownership. Do not update ticket status as implementation work.

## Handoff

Report old/new checkpoints and hashes, paired results for every runner,
exact commands, unchanged firmware hashes, remaining unsupported boundaries
and any required integrator change. The integrator decides completion.

## Integrator acceptance (2026-09-23, delegated)

Verified on the consolidated commit: the ticket's recorded paired
firmware/SDL regressions and focused cases pass; `make check` reports 989 PASS
/ 0 FAIL, `make sanitize` 984 PASS / 0 FAIL, and 155 task contracts validate.
Status set `done`. Gaps listed in the handoff stay open and are owned by their
named follow-up tickets. Per the standing era-drift rule, 2.39 era pins may
have drifted after this change; tickets 777/783 own re-derivation.

