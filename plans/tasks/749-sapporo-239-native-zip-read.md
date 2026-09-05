# 749 — Sapporo 2.39 Native ZIP Read Passthrough

**Status:** in-progress
**Phase:** 7
**Dependencies:** 729

## Goal

Allow the observed native mode-nine ZIP read to execute in firmware without
the logical writable-file adapter rejecting it or supplying synthetic data.

## Execution Budget

One model-day for mode recovery, regression and bounded native verification.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; ticket 748; E-SAP-0011 and
E-SAP-COMPAT-ZIP-READ-239-001; `src/compat/sapporo_239_file_hook.c`,
`src/compat/sapporo_239_files.h`, `src/compat/sapporo_239.h`;
`tests/integration/test_firmware_sapporo_239_ongoing.sh`.

## Current Baseline

At instruction 451,511,675, PC `0x000920b4`, LR `0x000843e9`, the logical
adapter refuses mode nine for `zapp/zwspee01.zip`. The native mode parser
accepts low bits one as read mode and uses bit eight only for failure logging.

## Allowed Files

- `src/compat/sapporo_239_file_hook.c`
- `tests/unit/test_sapporo_239_zip_read.c`
- `tests/integration/test_firmware_sapporo_239_zip_read.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/749-sapporo-239-native-zip-read.md`

## Frozen Interfaces

All headers, formats, logical paths/capacities, handles, descriptors, hit
budgets, profile/hash pins and public APIs remain unchanged. No new synthetic
operation is performed or counted. Historical prefix bytes stay exact.

## Evidence Inputs

E-SAP-COMPAT-ZIP-READ-239-001 pins native mode parsing, read-only research
and ticket 748's exact observed request. The exception is mode nine and
normalized path `zapp/zwspee01.zip` only. No other mode-nine path is authorized.

## Implementation

After path validation, return not-handled for that exact pair, preserving
registers, RAM, files and counters. Firmware runs the original wrapper and
filesystem, handles missing/corrupt content and generates its own return value.
All other unsupported modes retain the existing refusal.

## Tests and Commands

Run `make test TEST_FILTER=sapporo_239_zip_read` before and after the change;
cover exact native fallthrough, unchanged CPU/RAM/file/counter/log state even
at the exhausted synthetic budget, wrong paths/modes, disabled layer and
malformed paths. Run `make test TEST_FILTER=sapporo_239`,
`make test TEST_FILTER=machine_snapshot`, `make check-task-contracts`,
`make check-lines`, `make check`, `make sanitize`.
Run `SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_zip_read`.

## Acceptance

Regression fails before and passes after. Two fresh bounded authentic runs
and snapshots are byte-identical at a later explicit boundary. The old ticket
748 checkpoint remains byte-identical and resumes identically. Native trace
identifies the ZIP open result and next blocker. All private components
validate and source flash stays immutable; no normal frame is assumed.

## Forbidden Scope

No fabricated ZIP, host-file overlay, widened path glob, additional mode,
status translation, file budget increase, filesystem repair, CPU/device/MMIO,
header, profile, Makefile, snapshot or historical golden change.

## Handoff

Implemented and verified; status remains in-progress for integrator review.
Changed files are exactly the Allowed Files above. Evidence:
E-SAP-COMPAT-ZIP-READ-239-001 and E-SAP-0011. The entire production change
is a three-line exact mode/path native-fallthrough guard after path validation.
No synthetic intervention or new state is added; the native interpreter owns
the open, file contents, return value and all subsequent operations.

The narrow regression fails before the change at the first exact native ZIP
request and passes afterward. It checks lowercase/normalized uppercase paths,
wrong modes/flags, create refusal, other archive/UI/logical paths, suffix and
traversal variants, malformed/unterminated/overflowed paths and a disabled
layer. CPU, RAM, serialized files, exhausted synthetic counters and log stream
are unchanged on both native fallthrough and refusals.

All exact Tests and Commands above pass: focused Sapporo 2.39 tests 20 cases,
machine snapshot one case, task contracts 121 indexed tickets, check-lines
(existing warnings only), `make check` and `make sanitize` (127 binaries,
734 cases each), and the private firmware runner. `git diff --check` and
`sh -n tests/integration/test_firmware_sapporo_239_zip_read.sh` pass.

Read-only bounded tracing from ticket 748's validated snapshot reaches native
open `0x000e74c4`, returns real handle `0x30` to `0x000843e8` at instruction
451,622,704, and closes it at instruction 451,773,751. Trace/log hashes are
recorded in the evidence entry; there are no diagnostic budget or guest-state
changes. Native archive completeness and installation are not inferred.

Two fresh authentic logs and snapshots are byte-identical at
`stop=budget pc=0x000920b4 instructions=459796107 virtual_time_ns=1998752649`.
Log SHA-256 `ea04ac89a277fc58cc1c653e59e595f2a40f25d7202afd16b5adc309e6bf2732`;
snapshot `13e104c98a6fdf5a741a15615bf1b77ea53ebe05db39e7bf224cf0818560b5ee`.
There remain exactly 76,258 logical-file hits with no new intervention, reset
or device refusal. The old ticket-748 prefix retains original log SHA-256
`33f75a3051a8487405a4f5221d9db36a806fc54b2cf26fee8ebff5082bf62a7e` and
snapshot `42de6549afe8bef32603a4acd497f2aee0bb41a92a77f59022064c23e194998b`;
snapshot continuation reaches the identical new state. Source flash is
unchanged. Historical private runners/goldens are untouched.

The next attempted instruction refuses mode nine for `ui/js/config.js`, LR
`0x000843e9`, before PC/time/count advance. That UI resource route is the next
bounded task; no normal 2.39 frame is claimed. No additional integrator-owned
interface change is requested. Private data and diagnostic artifacts remain
outside Git; integrator review is required before marking done.
