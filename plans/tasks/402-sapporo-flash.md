# 402 — Sapporo External Flash Device

**Status:** blocked
**Phase:** 4
**Dependencies:** 285, 295, 298, 316, 317

## Goal

Implement a strict MSPI flash endpoint over immutable-base sparse overlays. This supplies verified storage to headless integration 420; OTA staging compatibility and controller behavior remain deferred.

## Execution Budget

Two to three agent-days. Implement one typed MSPI flash endpoint with immutable-base overlay semantics.

## Required Reading

`src/core/storage.c`, `include/semu/storage.h`, `src/boards/machine.c:map_sapporo`, `SapporoApollo4Mspi2.cs:CompleteObservedDma/ReadFlashByte`, `docs/research/external-flash-layout.md`, and `emulator/storage/TRACE.md` in `suunto-firmware`.

## Current Baseline

External flash is a 32 MiB RAM region loaded with resources. `semu_storage` already supports overlay program/erase but is not attached. The Renode MSPI2 wrapper recognizes ID/status/read/write-enable/sector-erase/page-program and also contains compatibility staging shortcuts that are not hardware.

## Allowed Files

Only `src/devices/{sapporo_flash.c,sapporo_flash.h}` and `tests/devices/test_sapporo_flash.c`.

## Frozen Interfaces

Opaque create/destroy/reset and typed serial endpoint; constructor accepts immutable `semu_storage`, exact capacity/sector/page geometry, and no board addresses. Implement evidenced opcodes as a strict command-state machine. Program is bitwise 1→0; erase is aligned; source storage remains unchanged.

## Evidence Inputs

`E-SAP-FLASH-001` must identify chip ID, capacity, opcodes, address widths, status bits, page/sector sizes, write-enable lifecycle, and byte-exact positive/negative traces. Cite `SapporoApollo4Mspi2` methods but exclude `StagingApertureBase`/production fallback unless compatibility ID covers it.

## Implementation

Separate command framing from overlay operations; validate entire command/range before mutation; reset volatile write-enable/status only; refuse unsupported mode/opcode/count.

## Tests and Commands

`make test TEST_FILTER=sapporo_flash` runs only its binary and exits 0; ID/status/read, write-enable, page program, sector erase, 0→1 protection, boundary/cross-page, unknown opcode, reset, source-hash guard, and repeat transcript pass. `make sanitize TEST_FILTER=sapporo_flash` and `make check` exit 0.

## Acceptance

Verified transcripts match; all mutations stay in overlay; source SHA-256 is unchanged; unverified staging/OTA behavior refuses; no controller register logic appears.

## Forbidden Scope

No MSPI controller, manufacturing/GPS fixture, direct XIP mapping policy, guessed JEDEC mode, host file write, package extraction, or board integration edit.

## Handoff

Report geometry/opcodes/state transitions, evidence hash, source/overlay proof, and endpoint attachment parameters for 420.
