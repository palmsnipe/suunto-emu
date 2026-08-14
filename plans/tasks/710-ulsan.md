# 710 — Ulsan Profiles and Hardware

**Status:** blocked
**Phase:** 7
**Dependencies:** 700

## Goal

Add exact Ulsan 2.35 and 2.44 board profiles with Apollo4 Plus differences, MSPI1, SDIO/eMMC, 466x466 NemaDC display, crown, and touch.

## Allowed Files

New Ulsan profiles/board/devices/display modules and tests, necessary Apollo4 Plus extensions, registries, docs coverage/evidence, Makefile lists, `plans/index.tsv` status only.

## Frozen Interfaces

Keep existing product behavior unchanged. Ulsan board owns memory/wiring and semantic crown/touch mapping. eMMC uses typed storage transactions and immutable base overlays; 466x466 frames declare explicit format/stride.

## Evidence Inputs

Exact Ulsan hashes, Apollo4 Plus maps, MSPI1/SDIO/eMMC/NemaDC/crown/touch traces recorded in new ledger entries.

## Implementation

Onboard 2.35 then 2.44: bounded reset, controller traffic, storage, native display, then inputs. Add pairing/BLE only in a later ticket if native traffic proves it is required for the declared checkpoint.

## Tests and Commands

`make check`; `make test TEST_FILTER=ulsan`; `make test-firmware FIRMWARE_ROOT="$FIRMWARE_ROOT" TEST_PROFILE=ulsan-2.35`; repeat for 2.44; run legacy Sapporo suite.

## Acceptance

Both profiles validate and reach their independent display/input checkpoints deterministically; storage bases are unchanged; unknown NemaDC/SDIO transactions refuse; all Sapporo tests pass.

## Forbidden Scope

No assumed Apollo4 identity, Sapporo wiring inheritance, BLE/pairing without observed need, writable eMMC base, approximate scaled 240x240 output, or profile hash wildcards.

## Handoff

Report product-specific maps, shared lower-layer proofs, trace/hash checkpoints, and deferred wireless traffic.

