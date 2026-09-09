# 754 — Sapporo 2.39 Post-Logo Activity Budget

**Status:** done
**Phase:** 7
**Dependencies:** 729,615

## Goal

Allow exactly the measured native post-logo activity updates and identify the
next independent native failure. Do not synthesize activity records.

## Execution Budget

One model-day for native operation tracing, regression and exact runs.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; tickets 729, 747, 748 and 753 handoffs;
E-SAP-0011, E-SAP-COMPAT-FILES-239-001, E-SAP-COMPAT-ONGOING-239-001,
E-SAP-BOOT-LOGO-239-001 and E-SAP-COMPAT-ACTIVITY-239-001;
`src/compat/sapporo_239.c`, `src/compat/sapporo_239.h`,
`src/compat/sapporo_239_files.c`, `src/compat/sapporo_239_files.h`,
`src/compat/sapporo_239_files_internal.h`, `src/compat/sapporo_239_file_hook.c`,
`src/compat/layer.c`, `include/semu/compat.h`, `include/semu/bus.h`;
`tests/unit/test_sapporo_239_compat.c`,
`tests/unit/test_sapporo_239_history_budget.c`,
`tests/integration/test_firmware_sapporo_239_widgets.sh`;
pristine application routines at 0xb9df8, 0xb9e14, 0xb94e0, 0xb9518,
0xb932a and 0xb938e as named by the new evidence entry.

## Current Baseline

The rendered-logo snapshot refuses update-open of `actitmln/247.bin` at
instruction 610,599,945 with all 76,258 logical operations consumed.

## Allowed Files

- `src/compat/sapporo_239.c`
- `tests/unit/test_sapporo_239_compat.c`
- `tests/unit/test_sapporo_239_history_budget.c`
- `tests/unit/test_sapporo_239_activity_budget.c`
- `tests/integration/test_firmware_sapporo_239_activity_budget.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/754-sapporo-239-post-logo-activity.md`

## Frozen Interfaces

Only the logical-file ceiling grows to 76,279, aggregate 76,282. Other
one-hit limits, exact hashes, event fields, ABIs, paths/capacities, handles,
cache bytes, snapshots, public headers, profile and Makefile stay unchanged.
Preserve historical private runners/goldens, including the logo hashes.

## Evidence Inputs

E-SAP-COMPAT-ACTIVITY-239-001 measures nine database and twelve ongoing-file
operations from the pinned rendered-logo state. The diagnostic ceiling is
200,000; it must not enter production. Native record/header writes and their
success checks agree with pristine disassembly. The next unknown mode/path
remains refused without a new file, fallback or provider translation.

## Implementation

Change only the two budget constants and their evidence comment. Pin the
21-operation suffix with synthetic bytes, exact final success, atomic excess
refusal, and below-budget unknown dump-mode/path refusal.

## Tests and Commands

Run `make test TEST_FILTER=sapporo_239_activity_budget` before and after;
`make test TEST_FILTER=sapporo_239`,
`make test TEST_FILTER=machine_snapshot`, `make check-task-contracts`,
`make check-lines`, `make check`, `make sanitize`.
Run `SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_activity_budget`.

## Acceptance

Failing-before/passing-after regression; 21 successful suffix operations and
next excess operation refused without CPU/RAM/file/counter/log mutation.
Two independently cold-started logo captures keep both historical hashes;
their continuations have identical logs/snapshots at the new boundary.
An intermediate updated-file snapshot resumes identically. One instruction
at the production boundary executes native BKPT after the GPS driver's
assertion (line 910). Keep the backend-less diagnostic dump-mode refusal
separate from the CLI's rendering-backed checkpoint. Validate all components before execution, verify
immutable source flash, and report any skipped gate.

## Forbidden Scope

No dump-file implementation, mode translation, guessed record or header,
activity-state repair, provider success, extra headroom, CPU/device/renderer
change, public API, snapshot format or historical golden edits. Do not infer
settled setup or interaction from the already-established boot logo.

## Handoff

Implemented and verified; status remains in-progress for integrator review.
Changed files are exactly the nine Allowed Files above. Evidence references:
E-SAP-0011, E-SAP-COMPAT-FILES-239-001, E-SAP-COMPAT-ONGOING-239-001,
E-SAP-BOOT-LOGO-239-001 and E-SAP-COMPAT-ACTIVITY-239-001.
Only two production constants and their evidence comment change. No header,
profile, registry, device, CPU, renderer or snapshot implementation changes.

The new one-case regression fails before implementation at the first
post-76,258 update-open. Afterward the synthetic nine/twelve-operation
sequences pass with the existing seek/write/read/close return values and
unchanged file sizes. Operation 76,279 succeeds and 76,280 refuses without
CPU, RAM, file/handle snapshot, counter or log mutation. Unknown dump mode ten
and unknown enum-create still refuse below the limit. The history test retains
all 144 x 248 record scans and tests the exact new final read/atomic refusal;
descriptor tests pin 76,279/76,282 and the unchanged three one-hit limits.

Commands/results:

- `make test TEST_FILTER=sapporo_239_activity_budget`: fails before, passes one case after.
- `make test TEST_FILTER=sapporo_239`: all 24 cases pass.
- `make test TEST_FILTER=machine_snapshot`: one case passes.
- `make check-task-contracts`: 125 indexed tickets validate.
- `make check-lines`: passes with existing review warnings only.
- `make check` and `make sanitize`: each passes all 740 cases.
- The exact private command in Tests and Commands passes: two independent
  cold-started logo captures retain both old hashes; continuations match logs
  and snapshots, perform exactly 21 operations, and contain no reset/refusal.
  A snapshot at 610,700,000 after both file closes resumes byte-identically.
  One-step continuation executes the native BKPT at the exact checkpoint.
- `sh -n tests/integration/test_firmware_sapporo_239_activity_budget.sh` and
  `git diff --check` pass. Direct private-script invocation skips an absent
  full-flash variable (exit zero) and rejects the application component supplied
  as a wrong full-flash input (exit two), both before firmware execution.

Production pre-BKPT checkpoint:
`stop=budget pc=0x00079e1c instructions=932397949 virtual_time_ns=11388431926`.
Continuation log SHA-256
`3a625809c79c1fdb8937ac36cd6e912b026ffcdc8fbc80c7ed888680d8bb11a7`;
snapshot `8d9b262474b00c6c0a2b5423ce4100363eb4582a96205eae8d045b70c8ae50a7`.
The next instruction halts at PC `0x00079e1e`, instruction 932,397,950,
time 11,388,431,927 ns. Full flash retains SHA-256
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
Ticket 753 logo log/snapshot remain
`63eb4997ff645958e70ed0586613762f88ee5e6e699434c1fbae48f0f435528b` /
`30050924fa4986412226750eb422aaccfca934a485ad7813e349e6b1da8b01a3`.
Historical runners and all their golden values remain unchanged; the old
post-logo file-budget refusal is superseded only by this measured increment.

The initial external diagnostic omitted the NEMA backend and reached a
different dump-mode refusal; no such mode/path is implemented. With the
normal backend attached, single stepping reproduces the production halt
exactly and identifies `CXD5610GF-driver.cpp:910`, LR `0x00128f55`, after a
native byte retry count reaches three. Its precise missing GPS exchange/state
transition is the next investigation. This explains the diagnostic/CLI
divergence without claiming a scheduler or snapshot defect.

No skipped acceptance, additional interface request, recovered persisted state,
settled 2.39 setup or interaction claim. Firmware, pixels, private snapshots
and diagnostic traces remain outside Git. Completion status is integrator-owned.
