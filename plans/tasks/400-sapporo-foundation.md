# 400 — Sapporo 2.22 Profile and Wiring

**Status:** done
**Phase:** 4
**Dependencies:** 295, 298, 320

## Goal

Freeze exact Sapporo 2.22 component/memory/profile and verified wiring tables. This unlocks compatibility 418 and headless assembly 420; device instantiation and inferred wiring remain deferred.

## Execution Budget

Two agent-days. Freeze exact memory/component/profile and board wiring data without instantiating physical devices.

## Required Reading

`profiles/sapporo/2.22.60/profile.semu`, `profiles/index.semu`, `src/boards/machine.c:known_sapporo_profile/map_sapporo/semu_machine_input`, `../suunto-firmware/emulator/renode/sapporo.resc`, `suunto-sapporo.repl`, and `docs/profile-format.md`.

## Current Baseline

The checked-in profile pins three hashes and vector `0x00040000`. `machine.c` hardcodes MRAM/SRAM/external-flash ranges and button pins 57–59; no board object, typed wiring table, controller instance selection, endpoint attachment, or IRQ/polarity evidence exists locally.

## Allowed Files

Only `profiles/sapporo/2.22.60/{profile,firmware.example}.semu`, `profiles/index.semu`, `src/boards/{sapporo_profile,sapporo_wiring}.{c,h}`, and `tests/unit/test_sapporo_profile.c`.

## Frozen Interfaces

`sapporo_profile.h` exposes immutable regions/components/vector/display data. `sapporo_wiring.h` exposes controller instance, address/chip-select, GPIO/IRQ, polarity, and endpoint role records keyed by stable role enum; lookup refuses duplicates/unknown roles. It maps no memory and owns no device.

## Evidence Inputs

`E-SAP-PROFILE-001` must verify exact component hashes/sizes/load addresses, MRAM/SRAM/XIP ranges, vector, NVIC priority mask, and 240×240 contract. Wiring rows additionally require the corresponding device evidence ID; unverified rows are omitted, not zero-filled.

## Implementation

Move hardcoded facts into immutable checked tables, validate region/wiring uniqueness and overflow, and keep built-in profile ID `sapporo-2.22.60`. Do not change CLI or machine selection here.

## Tests and Commands

`make test TEST_FILTER=sapporo_profile` runs only its binary and exits 0; exact valid profile, wrong hash/size/version, overlap, duplicate role, unknown role, and absent-unverified-wiring cases pass. `build/suunto-emu show-profile sapporo-2.22.60` retains exact hashes and exits 0. `make check` exits 0.

## Acceptance

All table values cite verified IDs; malformed profile/wiring fails before mapping; no private firmware path/bytes are added; data has no implicit family inheritance.

## Forbidden Scope

No machine/device instantiation, new guessed address, package extraction, compatibility auto-enable, external profile restriction, Apollo4 implementation, or display rendering.

## Handoff

Report exact profile table, present/blocked wiring roles, evidence IDs, command output, and APIs consumed by 420.
