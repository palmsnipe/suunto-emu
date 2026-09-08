# 766 — Sapporo 2.39 Personal Settings Budget Integration

**Status:** done
**Phase:** 7
**Dependencies:** 729,759,764

## Goal

Enable exactly the 228 measured settings operations so production reaches
the weight selector and preserves the unknown `settings/time` refusal.

## Execution Budget

One model-day for a narrow constant change, regressions and native gates.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; ticket 764; E-SAP-COMPAT-PERSONAL-239-001,
E-SAP-COMPAT-GENERAL-239-001; all existing Allowed Files;
`include/semu/{machine,compat,bus}.h`, `src/compat/sapporo_239_files.h`,
`src/compat/sapporo_239_files.c`, `src/compat/sapporo_239_file_hook.c`,
`src/core/snapshot_io.h`, `src/frontends/cli_snapshot.c`,
`src/display/nema_backend.h`, and the existing private general-budget
observer/gate. Read pristine `0x000acb78..0x000acba4` and `0x000d5084`
before reporting any follow-on static time-file observation.

## Current Baseline

All dependencies are done. Production ceiling is 76,371 logical / 76,374
aggregate. Accepted evidence measures two personal saves of 68 operations
and one general save of 92, without a file ABI correction.

## Allowed Files

- `src/compat/sapporo_239.c` (two constants and evidence comment only)
- `tests/unit/test_sapporo_239_compat.c`
- `tests/unit/test_sapporo_239_history_budget.c`
- `tests/unit/test_sapporo_239_activity_budget.c`
- `tests/unit/test_sapporo_239_general_budget.c`
- `tests/unit/test_sapporo_239_personal_budget.c`
- `tests/integration/sapporo_239_personal_probe.c` (private-only new helper)
- `tests/integration/test_firmware_sapporo_239_personal_budget.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`, this ticket

## Frozen Interfaces

Only change the production ceilings to 76,599 logical / 76,602 aggregate.
Keep other one-hit interventions, identity strings/hash pins, ABI, handles,
file paths/capacities, snapshot formats, renderer and four-pulse GPS limit.
No public header, Makefile, profile or registry changes. Private acceptance
helper uses existing APIs and normal production library, no diagnostic
descriptor or opaque-state repair. Historical goldens stay exact.

## Evidence Inputs

E-SAP-COMPAT-PERSONAL-239-001 pins the 228-operation suffix and twelve
post-general-save input edges. Full branch ends at `000920b4 / 3885178598 /
30368914377`, unknown mode-two `settings/time`, CRC `a8c9f3d3`, snapshot
`3d19f80df2c47a1e98f6bfa8dd0640f0d6cbb38a4d06f933d16f09c2c54245c6`.
Idle branch ends at `001291cc / 3152721353 / 32538694863`, GPS refusal,
snapshot `4faf5b8934c80cbadc33a7d6a389dd8f50a26bacdf2ed7208effd7a4abadb3e3`.

## Implementation

Start with a failing-before synthetic regression at the first post-76,371
open. Execute exact 68+68+92 writes with synthetic bytes in existing files;
check content, return lengths, mid-save round trip, final close at the new
limit and excess refusal preserving CPU/RAM/files/handles/counters/log.
Unknown path/mode still refuse below the limit. Existing general/activity
tests retain their historical suffix and saturate the later ceiling separately.

Build a bounded private-only helper: validate all components/full flash,
generate the pinned general-save midpoint using native input from cold boot,
run two full continuations and a personal-midpoint resume, and exercise the
idle branch. Check exact logs, snapshots, pixels and operation counts. Reject
wrong manifest/flash/snapshot before execution. Preserve first refusal;
never repin a mismatch merely to pass. Missing private inputs may skip.

## Tests and Commands

`make test TEST_FILTER=sapporo_239_personal_budget` before/after;
`make test TEST_FILTER=sapporo_239`, `make test TEST_FILTER=machine_snapshot`,
`make check-lines`, `make check-task-contracts`, `make check`,
`make sanitize`, `make sdl`, `git diff --check`.

```sh
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_personal_budget
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_general_budget
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_activity_budget
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_awake
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_live_input.sh
```

## Acceptance

Failing-before/passing-after regression, exact suffix with no diagnostic
headroom, atomic excess and unknown-operation refusal, native repeated/
resumed full and idle pins, unchanged historical gates and immutable flash.
Record all commands/results and absent evidence. A static-only path is not
acceptance. Status/index are left unchanged for separate integrator review.

## Forbidden Scope

No new time file, fabricated settings/time, file-capacity change, GPS pulse
or response, assertion bypass, firmware bytes, renderer patch, new dependency,
public API, persistent format change, automatic layer or weakened golden.

## Handoff

2026-09-08 implementation and all listed local acceptance checks pass;
status remains ready for separate integrator review. No commit made in this
continuation. Dependencies 729, 759 and 764 were confirmed done before editing.

Changed files: the runtime, tests, documentation and ticket listed under
Allowed Files. Runtime changes are only the two constants and evidence
comment in `src/compat/sapporo_239.c`. Existing tests retain historical suffixes;
the new 205-line unit test checks two 66-write personal saves and one 90-write
general save, contents, mid-write codec restoration and atomic refusal. The
177-line private observer and 99-line shell gate validate cold-prefix generation,
repeated/resumed full and idle branches, immutable refusals and input negatives.
No public-interface, Makefile, registry, profile or format change is requested.

Evidence: E-SAP-COMPAT-PERSONAL-239-001, E-SAP-COMPAT-GENERAL-239-001;
follow-on static-only findings are E-SAP-TIME-SCHEMA-239-001. The separate
planning review preceding implementation accepted 764 and instantiated this
ticket/index row; those planning edits are not runtime implementation changes.

Exact Tests and Commands above were run: the personal unit fails before the
change at its first post-76,371 open (line 175), then passes (one case).
Focused Sapporo 2.39: 46 passed; machine snapshot: four passed. `make check`
and `make sanitize`: 831 passed each, zero failed. `make check-lines` passes
with existing review-threshold warnings; `make check-task-contracts` validates
134 tickets. `make sdl`, `git diff --check` and all five private command lines
above pass. `sh -n tests/integration/test_firmware_sapporo_239_personal_budget.sh`
also passes. Private gate negatives reject wrong application metadata, wrong
full flash and a valid but unexpected checkpoint before native execution.
Local verification logs are external at `/tmp/semu-766.FStvr2/`.

Production pins (all SHA-256; full and idle each repeat and resume exactly):

| Checkpoint/artifact | Value |
| --- | --- |
| General midpoint | `76a7af5385eb2ddf5dfe94f6607db34e6e820edb06f5054a31bce7a5ef4ada66` |
| Personal midpoint | `68ab2fa1fda8596e1b51a6b3d83950363effb0598f1835506492d20636fad7d8` |
| Full final snapshot | `3d19f80df2c47a1e98f6bfa8dd0640f0d6cbb38a4d06f933d16f09c2c54245c6` |
| Full prefix log | `21a566167b8e34a1bf36e25feca4f1e337e8ca1f4d858822fe8e24661d3de139` |
| Full resumed log | `f287fc1e9276ea1540596234b8b65ae4d560fdda493ae7865bd9c3d928ae54bf` |
| Idle final snapshot | `4faf5b8934c80cbadc33a7d6a389dd8f50a26bacdf2ed7208effd7a4abadb3e3` |
| Idle prefix log | `509437ffa701685958420794fdf70d24ef4704b2869c0f49fb3fc09f0130dfcf` |
| Idle resumed log | `f6d6c1bab95a4150129d65d917ddbeade37bd7b0647354ba6877d68ad2013bd3` |

Full endpoint: `000920b4 / 3885178598 / 30368914377`, CRC `a8c9f3d3`,
pixel SHA-256 `3820703556359211f629aea5ef013dda45fba092229b8d61e5a10d684c42d585`.
Idle endpoint: `001291cc / 3152721353 / 32538694863`, CRC `568bdc7d`,
pixel SHA-256 `d2c4833a433610b5087f6e04fe16c7c4bd9d3baf6573df21cc72e0abde77b09b`.
Both one-instruction/one-nanosecond refusal retries produce byte-identical
snapshots. Full prefix logs contain exactly 279 operations (51 prior + 228),
132 personal writes and 140 general writes, final ordinal 76,599. Source
flash retains `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.

Remaining gaps: unknown `settings/time`, subsequent profile choices, completed
setup, watch-face activation and post-setup menus. Static recovery identifies
19 time fields but neither full emitted writes nor a justified file capacity;
measure those before a separate integration ticket. The four-pulse GPS bound
is preserved. No fabricated clock or later hardware behavior is authorized.

### Integrator acceptance, 2026-09-08

User-requested commit `bca8f7d` records the implementation and evidence.
Separate planning review accepts all recorded acceptance results, including
the exact full/idle production pins and private input/refusal negatives.
Fresh review runs pass the personal unit (one), machine snapshot tests (four)
and all 831 normal tests. This review marks 766 done and instantiates 767
for time-file evidence only; no production scope expansion is implicit.
