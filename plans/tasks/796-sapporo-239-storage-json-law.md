# 796 — Sapporo 2.39 Native Storage JSON Write Law

**Status:** ready
**Phase:** 7
**Dependencies:** 777

## Goal

Extend the Sapporo 2.39 compat logical-file law so the guest's native
storage JSON persistence (`storage/<key>/data.jsn`, observed key prefix
`38d123`) is modeled with the same named, hash-pinned, opt-in, hit-bounded,
logged treatment as the existing table paths, unblocking the six era
scripts that currently end at the fail-closed unknown-writable-path
refusal.

## Execution Budget

Two to three model-days. Expected: one compat evidence entry (offline RE
per the 2026-09-23 owner decision or a lane probe), table/capacity edits in
`src/compat/sapporo_239_files.c` and the file hook, focused unit cases, and
the six era scripts re-derived green twice.

## Required Reading

`AGENTS.md`; `docs/migration-evidence.md` E-SAP-COMPAT-FILES-239-001,
E-SAP-TIME-NATIVE-239-001, and the personal-settings entry carrying the
`pc=000920b4 ... path=settings/time` refusal census;
`src/compat/sapporo_239_file_hook.c`, `sapporo_239_files.c`,
`sapporo_239_files_internal.h`; ticket 777's classification record;
`tests/integration/test_firmware_sapporo_239_{gps_awake,gps_reopen,gps_startup,gps_five,general_budget,personal_budget}.sh`.

## Current Baseline

Every 2.39 GPS/settings era window ends at
`compat-refused pc=0x000920b4 instructions=446660148 virtual_time_ns=2831939375
detail=unknown Sapporo 2.39 writable file path` — the file hook refuses a
mode-2 open of `storage/38d123/data.jsn` (path captured 2026-09-27 with a
temporary, reverted diagnostic; capture noted in the 777 classification
entry). The path is absent from `semu_s239_file_paths`; the refusal is
correct fail-closed behavior for an unmodeled path. The six scripts' pinned
choreographies (GPS awake 32.7 s, five-pulse 37.9 s, settings budgets) are
unreachable past 2.83 s guest time.

## Allowed Files

`src/compat/sapporo_239_files.c`, `src/compat/sapporo_239_files_internal.h`,
`src/compat/sapporo_239_file_hook.c`, focused unit modules under
`tests/unit/`, the six era scripts named above (pin re-derivation only),
`docs/migration-evidence.md`, `docs/current-status.md`, this ticket.
`Makefile`, registries, profiles, and other scripts are integration-owned.

## Frozen Interfaces

The existing table-owned state machine, capacities, quiet-read bypass,
settings/time native retention, ordinal intervention accounting, refusal
message prefixes pinned by other scripts, deterministic checkpoints, and
firmware safety remain. Unknown paths keep refusing fail-closed; this
ticket admits only the lane/RE-evidenced storage JSON path family with its
measured capacity and size law.

## Evidence Inputs

The refusal census in ticket 777's classification entry (twice-identical
across all six scripts, integrator-reproduced manually). Required next:
the native storage write semantics for `storage/<key>/data.jsn` — block
size/capacity, write/commit sequence, key derivation — from offline RE of
the pinned 2.39 application partition or an equivalent lane probe, named
with SHA-256 inputs and a twice-reproduced census.

## Implementation

Derive the storage JSON write law from the pinned application image;
extend the file table and hook with the measured capacity and operation
sequence; keep the refusal for every still-unmodeled path. Red-first unit
cases: admitted-path success, capacity boundary refusal, unknown-key
refusal. Then re-derive the six era scripts' pins from two byte-identical
runs each; the choreography goldens (stop reason, later windows) must
RECOVER their original shapes, not be re-pinned to the early refusal.

## Tests and Commands

`make test TEST_FILTER=sapporo_239` (or the focused module), `make check`,
`make sanitize`, and the six era scripts twice each with
`SEMU_EMULATOR/TEST_PROFILE/SEMU_FIRMWARE_MANIFEST/SEMU_SAPPORO_239_FULL_FLASH`
set; `make check-era` census improving from 31 red to 25 red before the
25 re-pin batch, then green as 777 completes.

## Acceptance

A hash-pinned offline-RE (or lane) evidence entry for the storage JSON
path; unit success+refusal coverage; all six era scripts exit 0 twice with
re-derived pins whose choreography shapes match their pre-file-hook goldens
at unchanged CRCs; zero new refusal classes introduced.

## Forbidden Scope

No blanket unknown-path admission, no fabricated capacity, no guessed
serializer behavior, no weakening of the six scripts' choreography goldens
to the early refusal, no silent ordinal-budget growth, no engine-file
changes outside the compat layer named above.

## Handoff

Integrator-created 2026-09-27 during the 777 classification batch: the six
era scripts are blocked on exactly this law; everything else about the 777
re-derivation is reproducible without it.
