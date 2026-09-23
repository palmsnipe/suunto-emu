# 791 — Renderer Snapshot and Interactive Restore Integration

**Status:** done
**Phase:** 7
**Dependencies:** 513, 615, 761

## Goal

Restore a saved Sapporo menu with its original image, generation and drawing
state, then accept native button input. Preserve uninterrupted execution and
rendering. Other device snapshot gaps and compressed codecs remain separate.

## Execution Budget

Three model-days; at most twenty implementation/header files and four focused
test/runner files. Synthetic snapshots only in Git; authentic artifacts volatile.

## Required Reading

`README.md`, `docs/current-status.md`, `docs/architecture.md` ownership and
stable contracts, `docs/execution-model.md` display transactions,
`docs/testing-strategy.md`, `docs/compatibility-policy.md`; tickets 513, 615,
761 and evidence E-EMU-SAPPORO-BRANCH-GATES-001, E-CPU-F57F-001,
E-EMU-NEMA-ATOMIC-001. Every existing Allowed File; additionally
`src/core/snapshot_io.h`, `src/display/nema_tsc6a_internal.h`,
`src/devices/sapporo_nema_gpu_snapshot.c`, `tests/unit/test_nema_backend_atomic.c`.

## Current Baseline

Machine snapshots omit the caller-owned display backend. Restored idle is
blank and SDL refuses buttons before its first frame. An explicit replay can
redraw the menu but restarts generation at 1 instead of 4510. The backend owns
inherited registers, a current RGB565 surface and a TSC6A shadow; inline draws
can make the current surface differ from the last published image. Existing
machine rollback and component serializers need a renderer participant.

## Allowed Files

- `include/semu/display.h`, `include/semu/machine.h`, `include/semu/trace.h`
- `src/boards/machine.c`, `src/boards/machine_internal.h`, `src/boards/machine_snapshot.c`
- `src/boards/machine_snapshot_display.c`
- `src/display/nema_backend.c`, `src/display/nema_backend.h`, `src/display/nema_backend_internal.h`
- `src/display/nema_backend_transaction.c`, `src/display/nema_backend_snapshot.c`
- `src/display/nema_state.c`, `src/display/nema_state_internal.h`, `src/display/surface.c`
- `src/frontends/cli.c`, `src/frontends/main_sdl.c`
- `tests/unit/test_renderer_snapshot.c`, `tests/unit/test_snapshot.c`
- `tools/test_sdl_onboarding_completion.sh`, `tools/test_sdl_snapshot_restore.sh`
- `README.md`, `docs/current-status.md`, `docs/migration-evidence.md`, `docs/execution-model.md`

## Frozen Interfaces

This integration owns the minimal public backend persistence contract and
machine option wiring. Other CPU/device/compatibility interfaces, input grammar,
firmware profiles, Makefile and cold execution/rendering behavior are frozen.
Snapshots become version 2 with a required backend identity/data section;
version 1 cannot reconstruct missing state and must fail explicitly.

## Evidence Inputs

E-EMU-SAPPORO-BRANCH-GATES-001 records paired blank idle restores and a native
LOWER redraw with reset generation. Existing architecture ownership and
transaction contracts authorize state persistence, not new GPU behavior.
No new hardware observation or guessed pixel decoding is involved.

## Implementation

Provide public backend save/load/last-published-frame callbacks associated
with the existing context, copied at machine creation. Save transfers an
owned bounded byte buffer to the caller; load validates the full component
before mutation and publishes nothing. Missing codecs for present backends,
wrong identity, malformed data and active transactions refuse. Backend-absent
machines use an explicit empty identity. Persist inherited registers/presence,
draw counters, current pixels, last published pixels/generation and TSC6A
shadow. Do not persist host pointers, pending staging or diagnostic history.

Join renderer save/restore to machine atomicity and republish only after the
complete successful restore, without incrementing generation or guest time.
SDL restored checkpoints accept any native button after the restored frame.
Save/restore must not re-execute old command lists or synthesize input.

## Tests and Commands

- `make test TEST_FILTER=renderer_snapshot`: actual snapshot regression fails
  before the fix; restored image/generation, inherited draws, shadow and inline
  unpublished pixels match; malformed/truncated/identity/busy cases are atomic.
- `make test TEST_FILTER=snapshot`: all snapshot families select and pass,
  including explicit version-1 refusal.
- `make check-lines`, `make check-task-contracts`, `make check`, `make sanitize`,
  `make sdl`: exit 0; no normal-build dependency added.
- `SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_onboarding_completion.sh`
- `SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_snapshot_restore.sh`

Authentic commands have explicit guest instruction/time bounds and 900-second
wall caps; repeat twice with byte-identical derived censuses. New snapshots
must be captured from cold version-2 runs, not patched from version 1.
Preserve cold pins. Re-derive only restore-specific publication logs with
before/after evidence. Run affected profile era scripts or report possible
pin drift explicitly; do not re-pin unrelated eras.

## Acceptance

Synthetic continuation matches uninterrupted rendering byte-for-byte across
normal, inherited-state, shadow and inline paths. Failed loads preserve machine
and backend state and emit no frame. A cold-created authentic menu snapshot
opens visibly, preserves generation, accepts a button and produces an observed
next menu frame in two bounded runs. Source firmware remains unchanged. A live
SDL window is verified for user control. All exact checks above pass.

## Forbidden Scope

No synthetic input to hide a blank restore, guessed old-snapshot migration,
private parallel backend API, firmware bytes/pixels/logs in Git, new GPU command
support, 2.35 device codecs, unlimited GPS, or unrelated golden updates.

## Handoff

Report format/API ownership, all changed files, exact commands and results,
paired hashes, interactive-window proof, remaining gaps and requested
integrator changes. The integrator reviews and updates ticket status.

## Integrator acceptance (2026-09-23, delegated)

Verified on the consolidated commit: the ticket's recorded paired
firmware/SDL regressions and focused cases pass; `make check` reports 989 PASS
/ 0 FAIL, `make sanitize` 984 PASS / 0 FAIL, and 155 task contracts validate.
Status set `done`. Gaps listed in the handoff stay open and are owned by their
named follow-up tickets. Per the standing era-drift rule, 2.39 era pins may
have drifted after this change; tickets 777/783 own re-derivation.

