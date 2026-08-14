# 520 — Native Display, SDL, Replay, and Golden Integration

**Status:** blocked
**Phase:** 5
**Dependencies:** 490, 500, 502, 504, 506, 510, 511, 512, 513, 515, 516, 518

## Goal

Wire backend, SDL, semantic input, and replay to reproduce all three exact Sapporo frame hashes twice. This completes the first-release Phase 5 gate and unlocks Phase 6 hardening; later products and golden changes remain deferred.

## Execution Budget

Two to three agent-days. Wire the frozen display backend/frontend/replay seams and prove the three exact private Sapporo frame goldens twice.

## Required Reading

`src/frontends/{cli.c,cli.h,main_sdl.c}`, `tests/golden/sapporo-2.22.60.frames`, `tests/private/sapporo-2.22.60`, all Phase 5 handoffs, native frame metadata/logs, and `E-SAP-0002..0004`.

## Current Baseline

CLI creates/runs machine synchronously, accepts only `--until wfi`, ignores input replay, and does not instantiate display backend. SDL presents callback frames but cannot inject events. Golden file contains hashes only; `test-firmware` validates manifests without running frames.

## Allowed Files

Only `src/frontends/{cli.c,cli.h,main_sdl.c}`, `tests/integration/test_sapporo_display.c`, `tests/private/sapporo-2.22.60/{display.replay,display.expected}`, and `tests/golden/sapporo-2.22.60.frames` only to verify—not alter—the three existing lines.

## Frozen Interfaces

CLI adds `--input-replay PATH` and named `--until startup-complete|normal-frame|middle-language|lower-transition|wfi`, all with instruction/time bounds. It creates backend, passes it through machine options, pumps scheduler/input/frontend at deterministic run boundaries, and prints normalized checkpoint/frame/layer/device summary. SDL adds `--scale`; headless uses same backend/replay without SDL.

## Evidence Inputs

`E-SAP-0002` normal 115200-byte hash `8503ffbde124e35f914b09eea858ffcda2fc4bc453d3f9c7eb3e88388621d9cc`; `E-SAP-0003` middle hash `68a4126a8f908e9dd7c5703982c6fe141e6cc89cc383ee0d9d6d502eeadcb2d9`; `E-SAP-0004` lower hash `dcec235c8b450c96356b27b49306026ab9d14e7626714cdacb8bf3737a623ad3`. Command/panel/replay evidence IDs must all be verified; block rather than change a hash.

## Implementation

Integrate without changing component semantics; bound each checkpoint to at most 10000000 instructions/2000000000 virtual ns; hash callback bytes immediately; compare two complete normalized records; keep private pixels/firmware outside repository.

## Tests and Commands

`make test TEST_FILTER=sapporo_display` runs only synthetic integration and exits 0. `SEMU_FIRMWARE_MANIFEST="$SEMU_FIRMWARE_MANIFEST" make test-firmware TEST_FILTER=sapporo_display` exits 0 with normal/middle/lower byte count 115200 and the three exact hashes twice. `SDL_VIDEODRIVER=dummy make check-sdl TEST_FILTER=sapporo_display` exits 0 after presenting/injecting all three buttons. `make sanitize TEST_FILTER=sapporo_display` and `make check` exit 0.

## Acceptance

All three hashes/byte counts match exactly in two runs; ordered checkpoints, generations, device transcripts, layer counts, stop/time/instruction summaries match; source hashes remain unchanged; no unexpected refusal or unsupported behavior occurs.

## Forbidden Scope

No golden/tolerance update, committed pixels/firmware, hidden layer, unbounded run, unknown-command acceptance, renderer workaround in CLI, later profile, network/host time, or component-file edit.

## Handoff

Report exact two-run summaries/hashes, commands, SDL/headless results, skips, source before/after hashes, and first-release gate status.
