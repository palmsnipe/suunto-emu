# 799 — Renderer Frame Lifecycle Snapshot Integration

**Status:** ready
**Phase:** 7
**Dependencies:** 615,761,791

## Goal

Preserve an in-progress compressed frame and its resting cache across snapshot
save/load, with deterministic continuation and atomic malformed-input refusal.
The owner requested continuation of this integration on 2026-10-02 after
E-EMU-NEMA-CACHE-LIFECYCLE-001 identified the missing persistent state.

## Execution Budget

One integration slice: one renderer codec, focused synthetic tests, and paired
bounded 2.22/2.35 firmware captures. Private artifacts stay outside Git.

## Required Reading

`README.md`, `docs/current-status.md`, `plans/index.tsv`, architecture ownership,
execution-model display transactions/snapshots, testing strategy, compatibility
policy; E-EMU-RENDERER-SNAPSHOT-001, E-EMU-SAP222-SNAPSHOT-AUDIT-001,
E-EMU-NEMA-CACHE-LIFECYCLE-001, E-EMU-SAP235-TICKTRAIL-002. All existing Allowed
Files plus `include/semu/display.h`, `src/boards/machine_snapshot_display.c`,
`src/display/nema_backend_internal.h`, `src/display/nema_state_internal.h`,
`src/display/nema_tsc6a_internal.h`, `src/display/nema_tsc6a_sync.c`,
`src/display/nema_backend_transaction.c` and `src/core/snapshot.c`.

## Current Baseline

Commit aeed58b fixes transaction rollback. Renderer codec 1 omits shadow_fresh
and the baseline cache. Restoring loses strokes; rebuilding the cache can lose
historical pixels for unsupported auxiliary-bit blocks. The 2.22 menu itself
has an unresolved frame. Existing 2.22 restore hashes also predate a renderer
change and require control-build attribution before replacement.

## Allowed Files

- `src/display/nema_backend_snapshot.c`
- `tests/unit/test_renderer_snapshot.c`
- `tests/unit/test_renderer_snapshot_codec.c`
- `tests/unit/test_nema_tsc6a_lifecycle.c`
- `tools/test_sdl_snapshot_restore.sh`
- `tools/test_sdl_sapporo_235_restore.sh`
- `tests/integration/test_firmware_sapporo_235_snapshot.sh`
- `README.md`, `docs/current-status.md`, `docs/execution-model.md`, `docs/migration-evidence.md`

## Frozen Interfaces

Outer machine snapshot version 2 and section identity/order, public headers,
profiles, Makefile, CPU/device/compatibility semantics, rendering and compressed
admission are frozen. This integration owns renderer codec version 2 within the
existing backend persistence API and the three listed snapshot-runner pins.
Planning-only setup owns plans/index.tsv and tickets 799/800; implementation
must not change their status or the unavailable 2.39 era pins.

## Evidence Inputs

Existing ownership/transaction architecture authorizes persistence of modeled
state, not new hardware behavior. E-EMU-NEMA-CACHE-LIFECYCLE-001 supplies the
mid-frame regression and the cache-history counterexample. E-EMU-SAP235-
TICKTRAIL-002 defines the existing frame lifecycle; preserve it unchanged.

## Implementation

Always write renderer codec 2: retain the existing core layout, append explicit
frame/cache flags, cached base/span bytes and baseline pixels. Encode fixed-width
integers little-endian, canonicalize invalid-cache payload to zero, validate the
entire image before allocation-free mutation, and exclude transaction backups,
host pointers and diagnostics. Reject codec 1 with an actionable recreate-
snapshot diagnostic: its missing state cannot be reconstructed reliably.
Keep saves at idle points between submissions, including in-progress frames.

## Tests and Commands

- Before implementation, reproduce loss of unresolved strokes and cached history.
- `make test TEST_FILTER=renderer_snapshot` and
  `make test TEST_FILTER=nema_tsc6a_lifecycle`: continuation equality, repeated
  saves, empty/valid caches, legacy/truncated/malformed atomic refusals, busy
  operations, and whole-machine rollback after late renderer loading.
- `make check-task-contracts`, `make check-lines`, `make check`, `make sanitize`,
  `make sdl` must pass.
- Twice reproduce the bounded cold commands from docs/screenshots/README.md for
  2.22 and 2.35, saving snapshots outside Git. Attribute the historical 2.22
  mismatch using a pre-frame-lifecycle control build before replacing its pin.
- Derive the three allowed runner snapshot pins twice byte-identically; preserve
  cold guest stop/count/time and published frames. Record any continuation
  change against the modeled-state regression; do not weaken expected stops.
- Run `make check-sdl`, `sh tools/test_sdl_snapshot_restore.sh`,
  `sh tools/test_sdl_sapporo_235_restore.sh`, and
  `SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu make test-firmware TEST_PROFILE=sapporo-2.35.34 TEST_FILTER=sapporo_235`.
- Run `make check-era`; an absent verified full-flash fixture is an explicit
  skip, tracked by ticket 800, not acceptance of 2.39 snapshot hashes.

## Acceptance

Mid-frame and completed-frame continuation match uninterrupted rendering and
cache state; corrupt/legacy/busy images refuse atomically. Covered 2.22/2.35
snapshots and continuation are paired and pinned, with drift attribution. All
normal checks pass. Report ticket 800 separately; full 2.39 acceptance is not
part of this slice and remains unverified without its required fixture.

## Forbidden Scope

No auxiliary-bit decoding/clearing, writeback law, new rasterization, sensor
fixture extension, CPU change, public parallel API, proprietary Git artifacts,
or unrelated golden changes. Do not infer omitted legacy lifecycle state.

## Handoff

Report files, exact tests, old/new hashes and their attribution, repeated
censuses with log/probe hashes, legacy incompatibility, and ticket 800's gap.
Leave status ready for integrator review.
