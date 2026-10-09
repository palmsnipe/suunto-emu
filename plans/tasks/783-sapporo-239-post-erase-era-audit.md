# 783 — Sapporo 2.39 Post-Erase Era Audit

**Status:** done
**Phase:** 7
**Dependencies:** 729, 782

## Goal

Separate corrected-storage checkpoint changes from remaining defects across
all 43 Sapporo 2.39 era scripts after ticket 782's shared NOR erase correction.
Restore only evidence-supported gates; preserve genuine failure boundaries.

## Execution Budget

One audit instance, external bounded probes, focused era/test edits and an
explicit handoff for each behavior change requiring a separate implementation.

## Required Reading

Tickets 729, 777 and 782; E-SAP-0043, E-SAP-ERA-GATES-239-001;
`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; the `Makefile` check-era target,
`tests/integration/test_firmware_sapporo_239_*.sh`,
`src/compat/sapporo_239.c`, `src/compat/sapporo_239_files.c`,
`src/devices/sapporo_flash.c`, `src/soc/apollo4/mspi.c` and their focused tests.

## Current Baseline

The prior 2.39 census had 27 passing scripts and 16 failures. Correct DC
block erases change native filesystem contents and expose new checkpoint
mismatches and logical-file refusals: the completed sweep now fails 30 of 43,
including all sixteen prior failures and fourteen additional gates. No 2.39
pin is changed in ticket 782.
That implementation remains ready for integrator review, so this audit is
blocked on its accepted shared storage contract.

## Allowed Files

- `tests/integration/test_firmware_sapporo_239_*.sh`
- `tests/devices/test_sapporo_239_*.c`
- `docs/current-status.md`, `docs/migration-evidence.md`

Planning owns this ticket and its index row. Runtime/header/registry changes
require a separately scoped integration instance with its own evidence.

## Frozen Interfaces

Preserve component/full-flash hashes, compatibility hit limits, unknown-path
refusals, CPU/device semantics, replay/snapshot formats and CLI options.

## Evidence Inputs

E-SAP-0043 shared erase correction and before/after era failure censuses.
Full-flash fixture SHA-256
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
Every replacement pin needs two identical bounded runs plus a documented
causal derivation; a failed assertion or unexpected path is not a new golden.

## Implementation

Reproduce all affected scripts. Classify changed instruction/time/frame pins,
newly reachable native storage behavior, and real compatibility gaps. Derive
only justified pin updates; report the smallest required runtime integration
when an existing interface or compatibility scope is insufficient.

## Tests and Commands

`SEMU_SAPPORO_239_FULL_FLASH=/path/to/exact.bin make check-era` must finish and
report the complete census. Run each changed runner through `make test-firmware
TEST_PROFILE=sapporo-2.39.20 SEMU_FIRMWARE_MANIFEST=/path/to/firmware.semu
TEST_FILTER=<runner-tag>` with the same full-flash variable. Retain each
script's explicit instruction/time budgets. Run `make check-task-contracts`,
`make check-lines`, `make check`, and focused changed C tests if any.

## Acceptance

Every changed pin has causal evidence and two-run equality. Wrong hash,
wrong snapshot and unsupported operations still refuse. No unaccounted era
regressions remain; unresolved behavior is explicitly blocked rather than
re-pinned. Report source-image immutability and all remaining failures.

## Forbidden Scope

No fabricated frame, assertion bypass, unknown-file fallback, increased
compatibility hit limit, physical-device acquisition or silent era repin.

## Handoff

Record the full pass/fail census, exact commands and hashes, changed files,
causal evidence for each pin, and separately required integration work.

Integrator review (2026-10-09): acceptance verified — the census is 43/43
green in two back-to-back passes on the rebuilt fixture; every moved pin
carries causal evidence and two-run equality (E-SAP239-SNAPSHOT-REPIN-003);
the corrected-storage scripts never moved; source-image immutability holds
inside every runner; and no unaccounted regression remains. The ticket's
own "fails 30 of 43" baseline was superseded by the 800 re-derivation
before the audit began. Status done; the audit record is
E-SAP239-POSTERASE-AUDIT-001.
