# 520 — Sapporo Display Golden Gate

**Status:** blocked
**Phase:** 5
**Dependencies:** 510, 515

## Goal

Integrate native rendering and inputs, then prove the first release with the three exact private Sapporo frame hashes and repeated-run comparison.

## Allowed Files

Display/frontend/board integration glue, `tests/integration/sapporo_display*`, `tests/golden/sapporo_2_22*`, Makefile integration targets, `docs/{hardware-coverage,migration-evidence}.md`, `plans/index.tsv` status only.

## Frozen Interfaces

Normal frame is 115200 RGB565 bytes. Private tests request named normal, middle-language, and lower-transition checkpoints through CLI/input replay and compare SHA-256 only; no proprietary pixel bytes are committed.

## Evidence Inputs

`E-SAP-0002`, `E-SAP-0003`, `E-SAP-0004`, exact validated firmware manifest, command evidence from 500.

## Implementation

Wire operation stream to panel/frame callback and SDL; define deterministic button replay scripts; compare ordered checkpoints, frame generations/hashes, layer counts, and device transcripts for two runs.

## Tests and Commands

`make check`; `SDL_VIDEODRIVER=dummy make check-sdl`; `make test-firmware FIRMWARE_ROOT="$FIRMWARE_ROOT" TEST_FILTER=sapporo_display`; `make test-firmware FIRMWARE_ROOT="$FIRMWARE_ROOT" TEST_FILTER=sapporo_repeat`.

## Acceptance

Normal hash equals `8503ffbde124e35f914b09eea858ffcda2fc4bc453d3f9c7eb3e88388621d9cc`, middle equals `68a4126a8f908e9dd7c5703982c6fe141e6cc89cc383ee0d9d6d502eeadcb2d9`, and lower equals `dcec235c8b450c96356b27b49306026ab9d14e7626714cdacb8bf3737a623ad3`; byte count is 115200; repeated records are identical; no unexpected stop/layer event occurs.

## Forbidden Scope

No committing pixels/firmware, changing hashes, tolerances or visual similarity, auto-enabling compatibility, later profiles, or accepting unknown commands to reach a frame.

## Handoff

Report full hashes, checkpoints, stop/compat summaries, source-image before/after hashes, and first-release gate result.
