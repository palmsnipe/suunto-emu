# 763 — Sapporo 2.39 General Settings Budget Integration

**Status:** done
**Phase:** 7
**Dependencies:** 729,759,762

## Goal

Allow exactly the 92 measured language-selection persistence operations so
production reaches the native profile screen, preserving the next refusal.

## Execution Budget

One model-day for a narrow budget change, synthetic regression and native gates.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; tickets 729, 754, 759 and 762 handoffs;
E-SAP-0011, E-SAP-COMPAT-FILES-239-001, E-SAP-COMPAT-ACTIVITY-239-001,
E-SAP-UI-239-002 and E-SAP-COMPAT-GENERAL-239-001; every existing Allowed File;
`include/semu/compat.h`, `include/semu/machine.h`, `include/semu/bus.h`,
`src/compat/sapporo_239.h`, `src/compat/layer.c`,
`src/compat/sapporo_239_files.h`, `src/compat/sapporo_239_files.c`,
`src/compat/sapporo_239_file_hook.c`, `src/core/snapshot_io.h`,
`src/display/nema_backend.h`, `src/frontends/cli_snapshot.c`,
`tests/integration/test_firmware_sapporo_239_gps_awake.sh` and
`tests/integration/test_firmware_sapporo_239_activity_budget.sh`.

## Current Baseline

All dependencies are done. Production stops before the mode-two
`settings/general` open at logical limit 76,279 / aggregate 76,282.
Accepted 762 evidence proves exactly 92 further operations and repeat/resume
agreement at the fifth GPS-awake refusal; no file ABI correction is indicated.

## Allowed Files

- `src/compat/sapporo_239.c`
- `tests/unit/test_sapporo_239_compat.c`
- `tests/unit/test_sapporo_239_history_budget.c`
- `tests/unit/test_sapporo_239_activity_budget.c`
- `tests/unit/test_sapporo_239_general_budget.c`
- `tests/integration/test_firmware_sapporo_239_general_budget.sh`
- `tests/integration/sapporo_239_general_probe.c` (private gate only)
- `docs/current-status.md`, `docs/migration-evidence.md`, this ticket

## Frozen Interfaces

Only the two production ceilings and their evidence comment change, to
76,371 logical / 76,374 aggregate. Keep all three one-hit interventions,
hashes, layer identity, activation, native file ABI, paths/capacities, handle
allocation, snapshot encoding, renderer and four-pulse GPS limit unchanged.
No public headers, Makefile, registry or profile change. The standalone
private C observer links the normal library and invokes existing APIs only;
no diagnostic descriptor or bypass is linked into the acceptance gate.

## Evidence Inputs

E-SAP-COMPAT-GENERAL-239-001 pins open, 90 contiguous successful writes
totalling 1,505 bytes, close, and native pending-flag clear. Expected next
refusal: PC `0x001291cc`, instructions 2363623546, virtual ns 32619070564,
GPS hits `2,2,4`. Final snapshot SHA-256
`613712b78fd1ba748517e710d70280300bee0a48ee43133c9fd9f8070f5442f8`,
frame CRC `405d1af6`; reproduce these rather than repinning a mismatch.

## Implementation

Start with a synthetic regression that fails on the existing limit. Exercise
the exact 90 write sizes against synthetic bytes and a pre-existing file;
check native-sized returns, final close at the exact limit, mid-write file
snapshot restore, and atomic excess refusal including CPU/RAM/file/handles/
counters/log. Unknown create path/mode must refuse below budget. Preserve
the historical activity 21-operation suffix while updating only final-limit
assertions/saturation in existing tests.

Build a bounded private-only observer with normal renderer and all four
explicit layers. Validate every component/full flash before execution; create
the hash-pinned 700-million-instruction prefix without private fixture data
in Git, replay the evidenced four input edges, save mid-write, compare two
prefix continuations and a mid-save resume, and check the next GPS refusal.
No CPU/RAM repair, direct native callback or log suppression. All artifacts
remain in a temporary directory, with a clear missing-input skip and mismatch
failure before execution.

## Tests and Commands

`make test TEST_FILTER=sapporo_239_general_budget` before and after;
`make test TEST_FILTER=sapporo_239`,
`make test TEST_FILTER=machine_snapshot`, `make check-task-contracts`,
`make check-lines`, `make check`, `make sanitize`, `make sdl`.
Run the following exact private gates without repinning historical goldens:

```sh
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_general_budget
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_awake
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_activity_budget
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_live_input.sh
```

## Acceptance

Failing-before/passing-after regression; precisely 92 admitted suffix hits,
atomic excess refusal and unknown-operation refusal; matching native repeated
and resumed final snapshot/pixels and native file operations; unchanged
historical private gates and source flash. Record every exact command/result
and any missing private evidence. New observer failures cannot be ignored.

## Forbidden Scope

No diagnostic headroom, file/handle ABI change, new path/mode, GPS response or
pulse, assertion bypass, firmware bytes, fabricated settings, framebuffer
patch, general watch-face claim, persistent format or historical golden edit.

## Handoff

Planning baseline: integration not yet implemented. Native profile screen is
the bounded outcome; watch-face activation and subsequent menus remain later
evidence work. Implementation must leave this ticket/index status unchanged.

### Implementation handoff, 2026-09-08

Implemented the exact measured ceiling; no broader runtime changes. Evidence:
E-SAP-COMPAT-GENERAL-239-001 (native suffix and integration),
E-SAP-COMPAT-FILES-239-001 (unchanged adapter ABI),
E-SAP-COMPAT-ACTIVITY-239-001 (preserved historical suffix), E-SAP-0011
(firmware identity), and E-SAP-UI-PERSONAL-239-001 (next observed boundary).

Changed implementation files are every Allowed File above: the descriptor
source, three existing budget tests, the new synthetic general-budget test,
the private shell gate and private-only C observer, both documentation files
and this ticket. The separate planning pass accepted 762 and updated its
ticket/index before implementation; 763's status is left ready for review.
No public interface, Makefile, profile, registry, file codec/ABI, renderer or
GPS implementation change is requested or included.

The new one-case regression fails on the old runtime at line 124, the first
post-76,279 open. After the two constant changes it passes all 90 native-sized
synthetic writes, content checks, mid-write file snapshot restore and the close
at hit 76,371. Further open refuses without changing CPU, RAM, file/handle
snapshot, counters or log position. Unknown path/mode refuse below the limit.
Historical activity still performs exactly nine plus twelve operations; a
separate saturation step now checks the later current ceiling.

Exact Tests and Commands results:

- `make test TEST_FILTER=sapporo_239_general_budget`: one case fails before,
  passes after; logs `/tmp/semu-763.WR6heT/{before,after}.log`.
- `make test TEST_FILTER=sapporo_239`: 45 cases pass.
- `make test TEST_FILTER=machine_snapshot`: four cases pass.
- `make check` and `make sanitize`: each passes all 830 cases.
- `make check-lines`: passes with existing 300-line review warnings only.
- `make check-task-contracts`: 132 indexed tickets validate.
- `make sdl`: passes with installed SDL3.
- All four exact private commands above pass without skipping: the new
  general-budget gate, historical GPS-awake gate, historical activity-budget
  gate and 2.22 SDL live-input gate. Source flash remains unchanged.
- `sh -n tests/integration/test_firmware_sapporo_239_general_budget.sh` and
  `git diff --check`: pass. Explicit missing-input invocation skips (exit 0);
  an application file supplied as full flash refuses (exit 2) before execution.
  The complete native gate independently rejects wrong component metadata and
  wrong flash inside its observer, without producing a prefix snapshot.

The initial new gate failed only in its negative-fixture assertion: absolute
component paths were correctly rejected by the manifest parser before reaching
the intended hash mismatch. Its fixture now uses safe relative paths through
a temporary source-directory symlink and asserts the exact application/profile
mismatch. The final full gate passes; no expected success hash was changed.
Normal and sanitizer checks were completed after the C implementation;
`make check` was rerun after the final gate/documentation corrections.

Production pins match accepted 762 evidence:

- Prefix: `7650d82fe72e58d544dc0043df99ab756ece41092d39e94d7c0b2b7460a904d2`.
- Mid-save: `76a7af5385eb2ddf5dfe94f6607db34e6e820edb06f5054a31bce7a5ef4ada66`.
- Final: `613712b78fd1ba748517e710d70280300bee0a48ee43133c9fd9f8070f5442f8`.
- Full continuation log: `47979b14ad148e366fce73b64a5589c7793ae58a58645f02f2bdaf761ae3458b`.
- Resume log / exact full-log suffix:
  `9dc4cff27c7a2a3d8499af7460e830d2dc9cc8615c5d50290abc56422b95eb3f`.
- Frame CRC `405d1af6`; 676 prefix-run frames and 528 mid-save-resume frames.
- Stop `compat-refused / 001291cc / 2363623546 / 32619070564`, GPS hits `2,2,4`.

Historical 2.39 GPS log/snapshot stay
`06698600df74b250f987bf3f23f2c14d65b7cbf2f62c9f5ebd00784ab52d0543` /
`da5bb8e0d002683719d6079ca496b03884045775d7268f5911b4b8cc36f6997a`.
The activity gate retains its earlier logo, update and native-GPS-halt pins.
The SDL gate retains the four 2.22 CRCs, log
`9ee0637132d115f0136e490c2314db49ef4a792ab0a1eae1d70ed15ff3a9382c`
and stop `000bacf4 / 774081920 / 6520939902`.

Further observational-only native input reaches the birth-year selector and
then the next `settings/personal` mode-two open. Two runs match at
`000920b4 / 2953605137 / 24380651994`, LR `000adb2f`, GPS hits `2,2,3`;
772/772 renderer submissions succeed. The next file serializer is `0x000d5ff8`,
but its complete operation sequence is not yet measured. Recover it under a
separate evidence scope before requesting another finite allowance; do not
guess its size/count from this general-settings save. Watch-face activation,
completed setup and post-setup menu navigation remain unproven.

All logs and retained production/follow-on snapshots are outside Git under
`/tmp/semu-763.WR6heT/`; new helper/gate sources contain no firmware or pixels.
Acceptance has passing implementation evidence and awaits integrator review.

### Integrator acceptance, 2026-09-08

Separate planning review accepts committed implementation `3afb5ff`. The
runtime diff is restricted to the two measured constants and evidence comment;
synthetic refusal/snapshot checks and private observer identity/limit checks
were reviewed. Retained normal/sanitizer and all four private-gate logs confirm
the handoff, with no skipped acceptance or repinned checkpoint. The focused
general-budget test is rerun and passes. Ticket 764 scopes the next personal
save as evidence-only work; production remains unchanged during that research.
