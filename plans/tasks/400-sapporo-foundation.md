# 400 — Sapporo Profile, Wiring, Flash, and Panel Transport

**Status:** blocked
**Phase:** 4
**Dependencies:** 320

## Goal

Create the exact Sapporo `2.22.60.3383-P` board/profile contract and wire its memory, flash, buttons/backlight, and raw panel transport for parallel device work.

## Allowed Files

`profiles/sapporo-2.22.60.3383-P.semu`, `src/boards/sapporo*`, `src/devices/{spi_flash,panel_transport}*`, `include/semu/board.h`, related `tests/{devices,integration}/**`, profile/board registry and Makefile integration.

## Frozen Interfaces

Profile and firmware manifest implement `docs/profile-format.md`, including exact component roles, addresses, sizes, SHA-256, vector table, 240x240 RGB565 display, and allowed layer IDs. Board maps semantic upper/middle/lower events to explicit GPIO transitions.

## Evidence Inputs

`E-SAP-0001`; exact resident/application/resource/SRAM/XIP/NVIC/vector/hash evidence migrated from `suunto-firmware` before profile values are committed.

## Implementation

Validate all components before mapping; implement immutable external-flash overlay behavior and raw panel command/data transport; make wiring tables board-private and reject unavailable inputs.

## Tests and Commands

`make test TEST_FILTER=sapporo_profile`; `make test TEST_FILTER=spi_flash`; `make test TEST_FILTER=sapporo_board`; `make test TEST_FILTER=panel_transport`; `make check`.

## Acceptance

Wrong product/version/size/hash/missing file/unsafe path fail before reset; valid synthetic components map correctly; flash source hash is unchanged after program/erase; all three input mappings and transport refusals pass.

## Forbidden Scope

No copyrighted firmware, guessed hashes/addresses, sensors, GPS/OHR, Nema interpretation, frame golden, direct SOF1/XZ loading, or compatibility auto-enable.

## Handoff

Publish board attachment points, profile component table, evidence IDs, and frozen paths for parallel 410/415.
