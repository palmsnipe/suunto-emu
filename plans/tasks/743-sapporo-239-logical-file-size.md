# 743 — Sapporo 2.39 Logical File Size Integration

**Status:** done
**Phase:** 7
**Dependencies:** 729

## Goal

Correct the missing public size wrapper for session-local logical files so
native sleep-database validation can use its actual firmware-written length.

## Execution Budget

One model-day for read-only firmware tracing, regression, and deterministic
authentic continuation.

## Required Reading

Ticket 729; `docs/architecture.md`, `docs/execution-model.md`,
`docs/testing-strategy.md`, `docs/compatibility-policy.md`;
E-SAP-COMPAT-FILES-239-001 and E-SAP-STARTUP-SLEEP-239-001;
`src/compat/sapporo_239_files.h`, `src/compat/sapporo_239_files_internal.h`,
`src/compat/sapporo_239_files.c`, `src/compat/sapporo_239_file_hook.c`;
`tests/unit/test_sapporo_239_files.c`; machine dispatch in
`src/boards/machine_run.c`; the read-only research
`$FIRMWARE_ROOT/docs/research/sapporo-2.39-startup-wbsto-preload.md`.

## Current Baseline

Ticket 742's authentic run reaches StartupClient's `sleepln` result 500 and
BKPT. The native header validation succeeds; the public size wrapper at
`0x00092244` falls through with synthetic handle `0x1015f900` and returns
zero. The guest expects `32 + 248 * 72 = 17888`, matching retained file data.

## Allowed Files

- `src/compat/sapporo_239_file_hook.c`
- `tests/unit/test_sapporo_239_file_size.c`
- `tests/integration/test_firmware_sapporo_239_file_size.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/743-sapporo-239-logical-file-size.md`

This integration ticket owns the existing file-hook PC registry expansion.
It does not need a new public API or any machine dispatch change.

## Frozen Interfaces

Keep layer ID, exact hash pins, intervention names/budgets, allowlisted paths,
capacities, handles, snapshot format, other file operations and native-handle
fallthrough unchanged. Return the retained length, not capacity or cursor.

## Evidence Inputs

E-SAP-COMPAT-FILE-SIZE-239-001 pins exact application disassembly and the
read-only C continuation. Public wrapper `0x00092244` takes a handle in R0
and returns file length in R0. Native validator `0x000ba058` requires that
length minus 32 equal the header's record count times 72. No result-500
translation, record mutation, or instruction bypass is authorized.

## Implementation

Admit exactly `0x00092244` to the existing registry. For an active synthetic
handle, return the retained file size after the existing intervention budget
check and log `operation=size` using the standard logical-file event. Preserve
cursor, file contents, all other CPU registers, and snapshot bytes. Stale or
unknown synthetic handles refuse; native handles pass through untouched.

## Tests and Commands

Run `make test TEST_FILTER=sapporo_239_file_size` before and after the fix.
Cover empty/nonempty length, cursor independence, reopen, truncate, snapshot
restore, native fallthrough, disabled layer, stale/unaligned/unknown synthetic
handles, and atomic budget refusal. Check log content and intervention counts.
Run `make test TEST_FILTER=sapporo_239_files`,
`make test TEST_FILTER=sapporo_239_compat`,
`make test TEST_FILTER=machine_snapshot`, `make check-task-contracts`,
`make check-lines`, `make check`, and `make sanitize TEST_FILTER=sapporo_239`.
Run `SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_file_size`.

## Acceptance

The narrow regression fails before the change and passes after. Two authentic
runs and snapshots match at a measured later boundary, source flash stays
immutable, and native sleep-file validation no longer fails its size check.
Record any next refusal without broadening scope. Older goldens stay intact.

## Forbidden Scope

No first-boot provider-result translation, native header/data change, host
persistence, budget increase, new paths, CPU/MMIO/device/renderer change,
snapshot version change, profile or public-header edit, or FAT repair.

## Handoff

Implemented and verified; status remains in-progress for integrator review.
Changed files are exactly the Allowed Files above. Evidence is
E-SAP-COMPAT-FILES-239-001, E-SAP-STARTUP-SLEEP-239-001, and
E-SAP-COMPAT-FILE-SIZE-239-001, with exact component pins E-SAP-0011.

The narrow regression compiled and ran before implementation: two tests,
both failed (missing PC registry entry, unchanged return registers, and absent
refusals). Both pass after the fix. All commands above pass: file operations
4 tests, compatibility 4 tests, machine snapshots 1 test, task contracts
116 tickets, check-lines (existing review warnings only), full make check,
and ASan/UBSan Sapporo 2.39 tests (10 tests). The private runner validates all
three components, checks the flash hash, compares two fresh logs and snapshots,
and verifies exact one-instruction continuation and immutable source flash.

Read-only single-step tracing starts from instruction 393,118,000 (snapshot
SHA-256 `0990ba61f505559f35e605f4603fdbba2b0864080c32f5d47e79c5f2dfc5428c`).
The pre-fix trace hashes to
`90bd72869b3b1c3036bbcc8a299bafa84a6a04559094761c87d69faa976fe520`.
Post-fix `sleepln` command zero reaches the normal callback at instruction
393,135,988 with R3=200; the bounded post-fix trace hashes to
`5acdc1e4f0505e07670da263714328b42184b13bbd455d5e943f0694dc8536bc`.
These private diagnostic artifacts remain external to Git.

The new authentic checkpoint is `stop=budget pc=0x00079e1c
instructions=405895301 virtual_time_ns=1927243545`. Log SHA-256 is
`8139068b549a4e2be4baf57c94bc3b8eff385cb2bbb8efca506a8b30469de0d8`;
snapshot SHA-256 is
`0fa411dde053a15ef42d1b4ce2bf7282ad1990f88532b059dcf1c14ca9824193`.
There are 512 logical-file operations, including successful size queries of
17,888 and 2,384 bytes. No reset/refusal or budget increase occurs. Resume is
`stop=halt pc=0x00079e1e instructions=405895302 virtual_time_ns=1927243546`.
Snapshot inspection identifies `TrainingTss` command zero, result 500 as the
next failure; its underlying cause remains for another task. No normal frame,
physical persistence, new API, or additional status translation is claimed.
Existing goldens remain intact. Only review/status is requested of the
integrator; no interface change is needed.
